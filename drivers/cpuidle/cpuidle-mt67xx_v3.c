/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>

#include <asm/cpuidle.h>
#include <asm/suspend.h>

#include "dt_idle_states.h"

void __attribute__((weak)) mt_cpuidle_framework_init(void) { }

int __attribute__((weak)) rgidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) slidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) mcidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) soidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) dpidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) soidle3_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) mcsodi_enter(int cpu)
{
	return 1;
}

static int mtk_rgidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	rgidle_enter(smp_processor_id());
	return index;
}

static int mtk_slidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	return slidle_enter(smp_processor_id());
}

static int mtk_mcidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	mcidle_enter(smp_processor_id());
	return index;
}

static int mtk_soidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	return soidle_enter(smp_processor_id());
}

static int mtk_dpidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	return dpidle_enter(smp_processor_id());
}

static int mtk_soidle3_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	return soidle3_enter(smp_processor_id());
}

static int mtk_mcsodi_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	return mcsodi_enter(smp_processor_id());
}

static struct cpuidle_driver mt67xx_v3_cpuidle_driver = {
	.name             = "mt67xx_v3_cpuidle",
	.owner            = THIS_MODULE,
	.states[0] = {
		.enter			= mtk_rgidle_enter,
		.exit_latency		= 1,
		.target_residency	= 1,
		.name			= "rgidle",
		.desc			= "wfi"
	},
	.states[1] = {
		.enter			= mtk_slidle_enter,
		.exit_latency		= 3,
		.target_residency	= 20,
		.name			= "Slow idle",
		.desc			= "Slow idle",
	},
	.states[2] = {
		.enter			= mtk_mcsodi_enter,
		.exit_latency		= 50,
		.target_residency	= 200,
		.name			= "MCSODI",
		.desc			= "Multi Core DRAM SR",
	},
	.states[3] = {
		.enter			= mtk_mcidle_enter,
		.exit_latency		= 400,
		.target_residency	= 2000,
#ifdef USING_TICK_BROADCAST
		.flags			= CPUIDLE_FLAG_TIMER_STOP,
#endif
		.name			= "MCDI",
		.desc			= "Multi Core deep idle",
	},
	.states[4] = {
		.enter			= mtk_soidle_enter,
		.exit_latency		= 1200,
		.target_residency	= 4200,
#ifdef USING_TICK_BROADCAST
		.flags			= CPUIDLE_FLAG_TIMER_STOP,
#endif
		.name			= "SODI",
		.desc			= "Screen-ON deep idle",
	},
	.states[5] = {
		.enter			= mtk_dpidle_enter,
		.exit_latency		= 1200,
		.target_residency	= 4200,
#ifdef USING_TICK_BROADCAST
		.flags			= CPUIDLE_FLAG_TIMER_STOP,
#endif
		.name			= "dpidle",
		.desc			= "deep idle",
	},
	.states[6] = {
		.enter			= mtk_soidle3_enter,
		.exit_latency		= 5000,
		.target_residency	= 10500,
#ifdef USING_TICK_BROADCAST
		.flags			= CPUIDLE_FLAG_TIMER_STOP,
#endif
		.name			= "SODI3",
		.desc			= "Screen-ON deep idle - v3",
	},
	.state_count = 7,
	.safe_state_index = 0,
};

#if !defined(CONFIG_MTK_FPGA)

static const struct of_device_id mt67xx_v3_idle_state_match[] __initconst = {
	{ .compatible = "arm,idle-state" },
	{ },
};

/*
 * arm64_idle_init
 *
 * Registers the arm64 specific cpuidle driver with the cpuidle
 * framework. It relies on core code to parse the idle states
 * and initialize them using driver data structures accordingly.
 */
int __init mt67xx_v3_cpuidle_init(void)
{
	int cpu, ret;
	struct cpuidle_driver *drv = &mt67xx_v3_cpuidle_driver;
	struct cpuidle_device *dev;

	/*
	 * Initialize idle states data, starting at index 1.
	 * This driver is DT only, if no DT idle states are detected (ret == 0)
	 * let the driver initialization fail accordingly since there is no
	 * reason to initialize the idle driver if only wfi is supported.
	 */
#if 0
	ret = dt_init_idle_driver(drv, mt67xx_v2_idle_state_match, 1);
	if (ret <= 0)
		return ret ? : -ENODEV;
#endif

	ret = cpuidle_register_driver(drv);
	if (ret) {
		pr_err("Failed to register cpuidle driver\n");
		return ret;
	}
	/*
	 * Call arch CPU operations in order to initialize
	 * idle states suspend back-end specific data
	 */
	for_each_possible_cpu(cpu) {
		ret = arm_cpuidle_init(cpu);

		/*
		 * Skip the cpuidle device initialization if the reported
		 * failure is a HW misconfiguration/breakage (-ENXIO).
		 */
		if (ret == -ENXIO)
			continue;

		if (ret) {
			pr_err("CPU %d failed to init idle CPU ops\n", cpu);
			goto out_fail;
		}

		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (!dev)
			goto out_fail;
		dev->cpu = cpu;

		ret = cpuidle_register_device(dev);
		if (ret) {
			pr_err("Failed to register cpuidle device for CPU %d\n",
			       cpu);
			kfree(dev);
			goto out_fail;
		}
	}

	return 0;
out_fail:
	while (--cpu >= 0) {
		dev = per_cpu(cpuidle_devices, cpu);
		cpuidle_unregister_device(dev);
		kfree(dev);
	}

	cpuidle_unregister_driver(drv);

	return ret;
}
#else
int __init mt67xx_v3_cpuidle_init(void)
{
	return cpuidle_register(&mt67xx_v3_cpuidle_driver, NULL);
}
#endif

device_initcall(mt67xx_v3_cpuidle_init);
