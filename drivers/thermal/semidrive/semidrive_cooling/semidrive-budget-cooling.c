#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include "semidrive-budget-cooling.h"

#define SEMIDRIVE_BUDGET_COOLING_NAME "semidrive-budget"
#define SEMIDRIVE_BUDGET_DRIVER_NAME "semidrive-budget-cooling"

static struct semidrive_budget_cooling_device *budget_cdev;

static int cpu_budget_apply_cooling(struct semidrive_budget_cooling_device *cooling_device,
				 unsigned long cooling_state)
{
	int ret = 0,cluster;
	u32 pre_state;
	u32 pre_freq = 0, next_freq = 0;
	/* Check if the old cooling action is same as new cooling action */
	if (cooling_device->cooling_state == cooling_state)
		return 0;
	if(cooling_state >= cooling_device->state_num)
		return -EINVAL;
	pre_state = cooling_device->cooling_state;
	cooling_device->cooling_state = cooling_state;
	for(cluster = 0; cluster < cooling_device->cluster_num; cluster++){
		if(cooling_device->cpufreq){
			pre_freq = cooling_device->cpufreq->tbl[pre_state].cluster_freq[cluster];
			next_freq = cooling_device->cpufreq->tbl[cooling_state].cluster_freq[cluster];
		}
		if((pre_freq != next_freq)){
			ret = semidrive_cpufreq_update_state(cooling_device, cluster);
			if(ret)
				return ret;
		}
	}
	return 0;
}

/**
 * cpu_budget_get_max_state - callback function to get the max cooling state.
 */
static int cpu_budget_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct semidrive_budget_cooling_device *cooling_device = cdev->devdata;
	*state = cooling_device->state_num-1;
	return 0;
}

/**
 * cpu_budget_get_cur_state - callback function to get the current devices cooling state.
 */
static int cpu_budget_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct semidrive_budget_cooling_device *cooling_device = cdev->devdata;
	*state = cooling_device->cooling_state;
	return 0;
}

/**
 * cpu_budget_set_cur_state - callback function to set the current devices cooling state.
 */
static int cpu_budget_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct semidrive_budget_cooling_device *cooling_device = cdev->devdata;
	return cpu_budget_apply_cooling(cooling_device, state);
}

/* Bind cpufreq callbacks to pvt cooling ap0 ops */
static struct thermal_cooling_device_ops const semidrive_cpu_cooling_ops = {
	.get_max_state = cpu_budget_get_max_state,
	.get_cur_state = cpu_budget_get_cur_state,
	.set_cur_state = cpu_budget_set_cur_state,
};

/**
 * semidrive_budget_cooling_get_cpumask 
 */
static int semidrive_budget_cooling_get_cpumask(void)
{
	int cluster,i;
	unsigned int min = 0, max = 0;
	struct cpufreq_policy policy;
	if (cpulist_parse("0-5", &budget_cdev->cluster_cpus[0])) {
			pr_err("Failed to parse ap0 cluster0 cpu mask!\n");
			return -EBUSY;
		}
	budget_cdev->cluster_num = 1;
	for(cluster = 0; cluster < budget_cdev->cluster_num; min = 0, max = 0, cluster ++){
		for_each_cpu(i, &budget_cdev->cluster_cpus[cluster]) {
			if (!cpufreq_get_policy(&policy, i))
				continue;
			if (min == 0 && max == 0) {
				min = policy.cpuinfo.min_freq;
				max = policy.cpuinfo.max_freq;
			} else {
				if (min != policy.cpuinfo.min_freq ||
					max != policy.cpuinfo.max_freq){
						pr_err("different freq, return.\n");
						return -EBUSY;
					}
			}
		}
	}
	return 0;
}

static int semidrive_budget_cooling_parse(struct platform_device *pdev)
{
	struct device_node *np = NULL;
	int ret = 0;
	np = pdev->dev.of_node;
	if (!of_device_is_available(np)) {
		pr_err("%s: budget cooling is disable", __func__);
		return -EPERM;
	}
	budget_cdev = kzalloc(sizeof(*budget_cdev), GFP_KERNEL);
	if (IS_ERR_OR_NULL(budget_cdev)) {
		pr_err("cooling_dev: not enough memory for budget cooling data\n");
		return -ENOMEM;
	}
	if (of_property_read_u32(np, "cluster_num", &budget_cdev->cluster_num)) {
		pr_err("%s: get cluster_num failed", __func__);
		ret =  -EBUSY;
		goto parse_fail;
	}
	if (of_property_read_u32(np, "state_cnt", &budget_cdev->state_num)) {
		pr_err("%s: get state_cnt failed", __func__);
		ret =  -EBUSY;
		goto parse_fail;
	}
	ret = semidrive_budget_cooling_get_cpumask();
	if(ret)
		goto parse_fail;
	return 0;
parse_fail:
	kfree(budget_cdev);
	return ret;
}

static int semidrive_budget_cooling_probe(struct platform_device *pdev)
{
	s32 err = 0;
	struct thermal_cooling_device *cool_dev;
	pr_info("semidrive budget cooling probe start !\n");
	if (pdev->dev.of_node) {
		/* get dt parse */
		err = semidrive_budget_cooling_parse(pdev);
	}else{
		pr_err("semidrive budget cooling device tree err!\n");
		return -EBUSY;
	}
	if(err){
		pr_err("semidrive budget cooling device tree parse err!\n");
		return -EBUSY;
	}
	budget_cdev->dev = &pdev->dev;
	budget_cdev->cpufreq = semidrive_cpufreq_cooling_register(budget_cdev);
	cool_dev = thermal_of_cooling_device_register(pdev->dev.of_node, SEMIDRIVE_BUDGET_COOLING_NAME,
						budget_cdev, &semidrive_cpu_cooling_ops);
	if (!cool_dev)
		goto fail;
	budget_cdev->cool_dev = cool_dev;
	budget_cdev->cooling_state = 0;
	dev_set_drvdata(&pdev->dev, budget_cdev);
	pr_info("CPU budget cooling register Success\n");
	return 0;
fail:
	semidrive_cpufreq_cooling_unregister(budget_cdev);
	kfree(budget_cdev);
	return -EBUSY;
}

static int semidrive_budget_cooling_remove(struct platform_device *pdev)
{
	thermal_cooling_device_unregister(budget_cdev->cool_dev);
	semidrive_cpufreq_cooling_unregister(budget_cdev);
	kfree(budget_cdev);
	pr_info("CPU budget cooling unregister Success\n");
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id semidrive_budget_cooling_of_match[] = {
	{ .compatible = "semidrive,budget_cooling", },
	{ },
};
MODULE_DEVICE_TABLE(of, semidrive_budget_cooling_of_match);
#else
#endif

static struct platform_driver semidrive_budget_cooling_driver = {
	.probe  = semidrive_budget_cooling_probe,
	.remove = semidrive_budget_cooling_remove,
	.driver = {
		.name   = SEMIDRIVE_BUDGET_COOLING_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(semidrive_budget_cooling_of_match),
	}
};

static int __init semidrive_budget_cooling_driver_init(void)
{
	return platform_driver_register(&semidrive_budget_cooling_driver);
}

static void __exit semidrive_budget_cooling_driver_exit(void)
{
	platform_driver_unregister(&semidrive_budget_cooling_driver);
}

fs_initcall_sync(semidrive_budget_cooling_driver_init);
module_exit(semidrive_budget_cooling_driver_exit);
MODULE_DESCRIPTION("semidrive budget cooling driver");
MODULE_AUTHOR("david");
MODULE_LICENSE("GPL v2");

