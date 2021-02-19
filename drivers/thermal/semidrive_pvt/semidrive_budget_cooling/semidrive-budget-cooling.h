#ifndef SEMIDRIVE_BUDGET_COOLING_H
#define SEMIDRIVE_BUDGET_COOLING_H

#include <linux/thermal.h>
#include <linux/cpufreq.h>

#define NOTIFY_INVALID NULL
#define INVALID_FREQ    (-1)
#define CLUSTER_MAX	(1)




struct semidrive_cpufreq_cooling_table{
	u32 cluster_freq[CLUSTER_MAX];
};

struct semidrive_hotplug_cooling_table{
	u32 cluster_num[CLUSTER_MAX];
};

struct semidrive_budget_cpufreq{
	u32 cluster_freq_limit[CLUSTER_MAX];
	u32 cluster_freq_roof[CLUSTER_MAX];
	u32 cluster_freq_floor[CLUSTER_MAX];
	u32 tbl_num;
	struct semidrive_cpufreq_cooling_table *tbl;
	struct notifier_block notifer;
	struct semidrive_budget_cooling_device *bcd;
	spinlock_t lock;
};


struct semidrive_budget_cooling_device {
	struct device *dev;
	struct thermal_cooling_device *cool_dev;
	u32 cooling_state;
	u32 state_num;
	u32 cluster_num;
	struct cpumask cluster_cpus[CLUSTER_MAX];
	struct semidrive_budget_cpufreq *cpufreq;
	struct list_head node;
};

int semidrive_cpufreq_update_state(struct semidrive_budget_cooling_device *cooling_device, u32 cluster);
struct semidrive_budget_cpufreq *
semidrive_cpufreq_cooling_register(struct semidrive_budget_cooling_device *bcd);
void semidrive_cpufreq_cooling_unregister(struct semidrive_budget_cooling_device *bcd);
int semidrive_cpufreq_get_roomage(struct semidrive_budget_cooling_device *cooling_device,
					u32 *freq_floor, u32 *freq_roof, u32 cluster);
int semidrive_cpufreq_set_roomage(struct semidrive_budget_cooling_device *cooling_device,
					u32 freq_floor, u32 freq_roof, u32 cluster);
#endif /* SEMIDRIVE_BUDGET_COOLING_H */
