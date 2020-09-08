/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/of.h>

#define DEFAULT_MIN_FREQ 100000000
#define DEFAULT_MAX_FREQ (~0UL)
static u32 min_freq = DEFAULT_MIN_FREQ;
static u32 max_freq = DEFAULT_MAX_FREQ;
static unsigned int transition_delay_us = 0;
static struct cpufreq_frequency_table *freq_table;

static unsigned long	ref_freq;
static unsigned long	loops_per_jiffy_ref;

static int sdrv_set_target(struct cpufreq_policy *policy, unsigned int index)
{
	unsigned long old_freq, new_freq;

	old_freq = policy->cur;
	new_freq = freq_table[index].frequency;

	if (!ref_freq) {
		ref_freq = old_freq;
		loops_per_jiffy_ref = loops_per_jiffy;
	}

	if (old_freq < new_freq)
		loops_per_jiffy = cpufreq_scale(
			loops_per_jiffy_ref, ref_freq, new_freq);

	clk_set_rate(policy->clk, new_freq * 1000);
	if (new_freq < old_freq)
		loops_per_jiffy = cpufreq_scale(
				loops_per_jiffy_ref, ref_freq, new_freq);

	return 0;
}

static int sdrv_cpufreq_driver_init(struct cpufreq_policy *policy)
{
	unsigned int freq, rate, minfreq;
	struct clk *cpuclk;
	int retval, steps, i;

	if (policy->cpu != 0)
		return -EINVAL;
	cpumask_setall(policy->cpus);
	cpuclk = clk_get(get_cpu_device(policy->cpu), "cpu0");
	if (IS_ERR(cpuclk)) {
		pr_err("cpufreq: could not get CPU clk\n");
		retval = PTR_ERR(cpuclk);
		goto out_err;
	}

	minfreq = (clk_round_rate(cpuclk, min_freq)) / 1000;
	freq = (clk_round_rate(cpuclk, max_freq)) / 1000;
	policy->cpuinfo.transition_latency = 0;
	policy->transition_delay_us = transition_delay_us;

	printk("cpufreq get clk: min %d max %d\n", minfreq, freq);

	steps = fls(freq / minfreq) + 1;
	freq_table = kzalloc(steps * sizeof(struct cpufreq_frequency_table),
			GFP_KERNEL);
	if (!freq_table) {
		retval = -ENOMEM;
		goto out_err_put_clk;
	}

	for (i = 0; i < (steps - 1); i++) {
		rate = clk_round_rate(cpuclk, freq * 1000) / 1000;

		freq_table[i].frequency = rate;

		freq /= 2;
	}

	policy->clk = cpuclk;
	freq_table[steps - 1].frequency = CPUFREQ_TABLE_END;

	retval = cpufreq_table_validate_and_show(policy, freq_table);
	if (!retval) {
		printk("cpufreq: sdrv CPU frequency driver\n");
		return 0;
	}

	kfree(freq_table);
out_err_put_clk:
	clk_put(cpuclk);
out_err:
	return retval;
}

static struct cpufreq_driver sdrv_cpufreq_driver = {
	.name		= "sdrv cpufreq",
	.init		= sdrv_cpufreq_driver_init,
	.verify		= cpufreq_generic_frequency_table_verify,
	.target_index	= sdrv_set_target,
	.get		= cpufreq_generic_get,
	.flags		= CPUFREQ_STICKY,
};

static int __init sdrv_cpufreq_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "semidrive,sdrv-cpufreq");
	if (!np) {
		pr_err("no cpufreq node\n");
		return -ENODEV;
	}
	if (of_device_is_available(np))
	{
		if (of_property_read_u32(np, "min-freq", &min_freq) < 0) {
			pr_err("no valid min-freq, set as default\n");
			min_freq = DEFAULT_MIN_FREQ;
		}
		if (of_property_read_u32(np, "max-freq", &max_freq) < 0) {
			pr_err("no valid max-freq, set as default\n");
			max_freq = DEFAULT_MAX_FREQ;
		}
		if (of_property_read_u32(np, "trans-delay-us", &transition_delay_us) < 0) {
			pr_err("no valid trans-delay-us, set as default\n");
			transition_delay_us = 0;
		}
		pr_info("cpufreq: get prop min %ld, max %ld, delay %d\n", min_freq, max_freq, transition_delay_us);
		return cpufreq_register_driver(&sdrv_cpufreq_driver);
	}
	return -ENODEV;
}
late_initcall(sdrv_cpufreq_init);
