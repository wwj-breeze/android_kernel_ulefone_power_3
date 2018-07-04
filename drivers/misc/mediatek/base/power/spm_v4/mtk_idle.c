/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpu.h>

#include <linux/types.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>
#include <linux/tick.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kallsyms.h>

#include <linux/irqchip/mtk-gic.h>

#include <asm/system_misc.h>
#include <mt-plat/sync_write.h>
#include <mach/mtk_gpt.h>
#include <mtk_spm.h>
#include <mtk_spm_dpidle.h>
#include <mtk_spm_idle.h>
#ifdef CONFIG_THERMAL
#include <mach/mtk_thermal.h>
#endif
#include <mtk_idle.h>
#include <mtk_idle_internal.h>
#include <mtk_idle_profile.h>
#include <mtk_spm_reg.h>
#include <mtk_spm_misc.h>
#include <mtk_spm_resource_req.h>
#include <mtk_spm_resource_req_internal.h>

#include "ufs-mtk.h"

#ifdef CONFIG_MTK_DCS
#include <mt-plat/mtk_meminfo.h>
#endif

#include <linux/uaccess.h>
#include <mtk_cpufreq_api.h>
#include "mtk_clk_id_internal.h"

#include "mtk_mcdi_governor.h"

#define FEATURE_ENABLE_SODI2P5

#define IDLE_TAG     "Power/swap "
#define idle_err(fmt, args...)		pr_err(IDLE_TAG fmt, ##args)
#define idle_warn(fmt, args...)		pr_warn(IDLE_TAG fmt, ##args)
#define idle_info(fmt, args...)		pr_debug(IDLE_TAG fmt, ##args)
#define idle_ver(fmt, args...)		pr_debug(IDLE_TAG fmt, ##args)
#define idle_dbg(fmt, args...)		pr_debug(IDLE_TAG fmt, ##args)

#define idle_warn_log(fmt, args...) { \
	if (dpidle_dump_log & DEEPIDLE_LOG_FULL) \
		pr_warn(IDLE_TAG fmt, ##args); \
	}

#define IDLE_GPT GPT4

#define log2buf(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))

#ifndef CONFIG_MTK_ACAO_SUPPORT
static atomic_t is_in_hotplug = ATOMIC_INIT(0);
#else
#define USING_STD_TIMER_OPS
#endif

static bool mt_idle_chk_golden;
static bool mt_dpidle_chk_golden;

#define NR_CMD_BUF		128

void go_to_wfi(void)
{
	isb();
	mb();	/* memory barrier */
	__asm__ __volatile__("wfi" : : : "memory");
}

/* FIXME: early porting */
#if 1
void __attribute__((weak))
bus_dcm_enable(void)
{
	/* FIXME: early porting */
}
void __attribute__((weak))
bus_dcm_disable(void)
{
	/* FIXME: early porting */
}

unsigned int __attribute__((weak))
mt_get_clk_mem_sel(void)
{
	return 1;
}

void __attribute__((weak))
tscpu_cancel_thermal_timer(void)
{

}
void __attribute__((weak))
tscpu_start_thermal_timer(void)
{
	/* FIXME: early porting */
}

void __attribute__((weak)) mtkTTimer_start_timer(void)
{

}

void __attribute__((weak)) mtkTTimer_cancel_timer(void)
{

}

bool __attribute__((weak)) mtk_gpu_sodi_entry(void)
{
	return false;
}

bool __attribute__((weak)) mtk_gpu_sodi_exit(void)
{
	return false;
}

bool __attribute__((weak)) spm_mcdi_can_enter(void)
{
	return false;
}

bool __attribute__((weak)) spm_get_sodi3_en(void)
{
	return false;
}

bool __attribute__((weak)) spm_get_sodi_en(void)
{
	return false;
}

int __attribute__((weak)) hps_del_timer(void)
{
	return 0;
}

int __attribute__((weak)) hps_restart_timer(void)
{
	return 0;
}

void __attribute__((weak)) msdc_clk_status(int *status)
{
	*status = 0x1;
}

unsigned int __attribute__((weak)) spm_go_to_dpidle(u32 spm_flags, u32 spm_data, u32 log_cond, u32 operation_cond)
{
	go_to_wfi();

	return WR_NONE;
}

void __attribute__((weak)) spm_enable_sodi(bool en)
{

}

void __attribute__((weak)) spm_sodi_mempll_pwr_mode(bool pwr_mode)
{

}

unsigned int __attribute__((weak)) spm_go_to_sodi3(u32 spm_flags, u32 spm_data, u32 sodi_flags)
{
	return WR_UNKNOWN;
}

unsigned int __attribute__((weak)) spm_go_to_sodi(u32 spm_flags, u32 spm_data, u32 sodi_flags)
{
	return WR_UNKNOWN;
}

bool __attribute__((weak)) go_to_mcidle(int cpu)
{
	return false;
}

void __attribute__((weak)) spm_mcdi_switch_on_off(enum spm_mcdi_lock_id id, int mcdi_en)
{

}

int __attribute__((weak)) is_teei_ready(void)
{
	return 1;
}

unsigned long __attribute__((weak)) localtimer_get_counter(void)
{
	return 0;
}

int __attribute__((weak)) localtimer_set_next_event(unsigned long evt)
{
	return 0;
}

uint32_t __attribute__((weak)) clk_buf_bblpm_enter_cond(void)
{
	return -1;
}
#endif

static bool idle_by_pass_secure_cg;
static unsigned int idle_block_mask[NR_TYPES][NF_CG_STA_RECORD];

/* DeepIdle */
static unsigned int     dpidle_time_criteria = 26000; /* 2ms */
static unsigned long    dpidle_cnt[NR_CPUS] = {0};
static unsigned long    dpidle_f26m_cnt[NR_CPUS] = {0};
static unsigned long    dpidle_block_cnt[NR_REASONS] = {0};
static bool             dpidle_by_pass_cg;
bool                    dpidle_by_pass_pg;
static bool             dpidle_gs_dump_req;
static unsigned int     dpidle_gs_dump_delay_ms = 10000; /* 10 sec */
static unsigned int     dpidle_gs_dump_req_ts;
static unsigned int     dpidle_dump_log = DEEPIDLE_LOG_REDUCED;
static unsigned int     dpidle_run_once;
static bool             dpidle_force_vcore_lp_mode;

/* SODI3 */

static unsigned int     soidle3_pll_block_mask[NR_PLLS] = {0x0};
static unsigned long    soidle3_cnt[NR_CPUS] = {0};
static unsigned long    soidle3_block_cnt[NR_REASONS] = {0};

static bool             soidle3_by_pass_cg;
static bool             soidle3_by_pass_pll;
static bool             soidle3_by_pass_en;
static u32              sodi3_flags = SODI_FLAG_REDUCE_LOG;
static int              sodi3_by_uptime_count;
static bool             sodi3_force_vcore_lp_mode;

/* SODI */
static unsigned long    soidle_cnt[NR_CPUS] = {0};
static unsigned long    soidle_block_cnt[NR_REASONS] = {0};
static bool             soidle_by_pass_cg;
bool                    soidle_by_pass_pg;
static bool             soidle_by_pass_en;
static u32              sodi_flags = SODI_FLAG_REDUCE_LOG;
static int              sodi_by_uptime_count;

/* MCDI */
static unsigned int     mcidle_time_criteria = 3000; /* 3ms */
static unsigned long    mcidle_cnt[NR_CPUS] = {0};
static unsigned long    mcidle_block_cnt[NR_CPUS][NR_REASONS] = { {0}, {0} };

/* Slow Idle */
static unsigned long    slidle_cnt[NR_CPUS] = {0};
static unsigned long    slidle_block_cnt[NR_REASONS] = {0};

/* Regular Idle */
static unsigned long    rgidle_cnt[NR_CPUS] = {0};

/* idle_notifier */
static RAW_NOTIFIER_HEAD(mtk_idle_notifier);

int mtk_idle_notifier_register(struct notifier_block *n)
{
	int ret = 0;
	int index = 0;
#ifdef CONFIG_KALLSYMS
	char namebuf[128] = {0};
	const char *symname = NULL;

	symname = kallsyms_lookup((unsigned long)n->notifier_call,
			NULL, NULL, NULL, namebuf);
	if (symname) {
		pr_err("[mt_idle_ntf] <%02d>%08lx (%s)\n",
			index++, (unsigned long)n->notifier_call, symname);
	} else {
		pr_err("[mt_idle_ntf] <%02d>%08lx\n",
			index++, (unsigned long)n->notifier_call);
	}
#else
	pr_err("[mt_idle_ntf] <%02d>%08lx\n",
			index++, (unsigned long)n->notifier_call);
#endif

	ret = raw_notifier_chain_register(&mtk_idle_notifier, n);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_register);

void mtk_idle_notifier_unregister(struct notifier_block *n)
{
	raw_notifier_chain_unregister(&mtk_idle_notifier, n);
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_unregister);

void mtk_idle_notifier_call_chain(unsigned long val)
{
	raw_notifier_call_chain(&mtk_idle_notifier, val, NULL);
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_call_chain);

#ifndef USING_STD_TIMER_OPS
/* Workaround of static analysis defect*/
int idle_gpt_get_cnt(unsigned int id, unsigned int *ptr)
{
	unsigned int val[2] = {0};
	int ret = 0;

	ret = gpt_get_cnt(id, val);
	*ptr = val[0];

	return ret;
}

int idle_gpt_get_cmp(unsigned int id, unsigned int *ptr)
{
	unsigned int val[2] = {0};
	int ret = 0;

	ret = gpt_get_cmp(id, val);
	*ptr = val[0];

	return ret;
}
#endif

static bool next_timer_criteria_check(unsigned int timer_criteria)
{
	unsigned int timer_left = 0;
	bool ret = true;

#ifdef USING_STD_TIMER_OPS
	struct timespec t;

	t = ktime_to_timespec(tick_nohz_get_sleep_length());
	timer_left = t.tv_sec * USEC_PER_SEC + t.tv_nsec / NSEC_PER_USEC;

	if (timer_left < timer_criteria)
		ret = false;
#else
#ifdef CONFIG_SMP
	timer_left = localtimer_get_counter();

	if ((int)timer_left < timer_criteria ||
			((int)timer_left) < 0)
		ret = false;
#else
	unsigned int timer_cmp = 0;

	gpt_get_cnt(GPT1, &timer_left);
	gpt_get_cmp(GPT1, &timer_cmp);

	if ((timer_cmp - timer_left) < timer_criteria)
		ret = false;
#endif
#endif

	return ret;
}

static void timer_setting_before_wfi(bool f26m_off)
{
#ifndef USING_STD_TIMER_OPS
#ifdef CONFIG_SMP
	unsigned int timer_left = 0;

	timer_left = localtimer_get_counter();

	if ((int)timer_left <= 0)
		gpt_set_cmp(IDLE_GPT, 1); /* Trigger idle_gpt Timeout imediately */
	else {
		if (f26m_off)
			gpt_set_cmp(IDLE_GPT, div_u64(timer_left, 406.25));
	else
		gpt_set_cmp(IDLE_GPT, timer_left);
	}

	if (f26m_off)
		gpt_set_clk(IDLE_GPT, GPT_CLK_SRC_RTC, GPT_CLK_DIV_1);

	start_gpt(IDLE_GPT);
#else
	gpt_get_cnt(GPT1, &timer_left);
#endif
#endif
}

static void timer_setting_after_wfi(bool f26m_off)
{
#ifndef USING_STD_TIMER_OPS
#ifdef CONFIG_SMP
	if (gpt_check_and_ack_irq(IDLE_GPT)) {
		localtimer_set_next_event(1);
		if (f26m_off)
			gpt_set_clk(IDLE_GPT, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1);
	} else {
		/* waked up by other wakeup source */
		unsigned int cnt, cmp;

		idle_gpt_get_cnt(IDLE_GPT, &cnt);
		idle_gpt_get_cmp(IDLE_GPT, &cmp);
		if (unlikely(cmp < cnt)) {
			idle_err("[%s]GPT%d: counter = %10u, compare = %10u\n",
					__func__, IDLE_GPT + 1, cnt, cmp);
			/* BUG(); */
		}

		if (f26m_off) {
			localtimer_set_next_event((cmp - cnt) * 1625 / 4);
			gpt_set_clk(IDLE_GPT, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1);
		} else {
		localtimer_set_next_event(cmp - cnt);
		}
		stop_gpt(IDLE_GPT);
	}
#endif
#endif
}

#if defined(SPM_SODI_PROFILE_TIME) || defined(SPM_SODI3_PROFILE_TIME)
#define PROFILE_DATA_NUMBER		(NR_PIDX*2)
static unsigned int idle_profile[NR_TYPES][PROFILE_DATA_NUMBER];
void idle_latency_profile(unsigned int idle_type, int idx)
{
	unsigned int cur_count = 0;
	unsigned int *data;

	data = &idle_profile[idle_type][0];

	gpt_get_cnt(GPT2, &cur_count);

	if (idx % 2 == 0)
		data[idx/2] = cur_count;
	else
		data[idx/2] = cur_count > data[idx/2] ?
			(cur_count - data[idx/2]) : (data[idx/2] - cur_count);
}

static char profile_log[4096] = { 0 };
#define plog(fmt, args...)	log2buf(p, profile_log, fmt, ##args)

void idle_latency_dump(unsigned int idle_type)
{
	unsigned int i;
	unsigned int *data;
	int len = 0;
	char *p = profile_log;

	data = &idle_profile[idle_type][0];

	plog("idle_latency %d (%u/%u),", idle_type,
		mt_cpufreq_get_cur_freq(0), mt_cpufreq_get_cur_freq(1));

	for (i = 0; i < PROFILE_DATA_NUMBER; i++)
		plog("%u%s", data[i], (i == PROFILE_DATA_NUMBER - 1) ? "":",");

	len = p - profile_log;

	idle_err("%s\n", profile_log);
}
#endif /* defined(SPM_SODI_PROFILE_TIME) || defined(SPM_SODI3_PROFILE_TIME) */

static unsigned int idle_spm_lock;
static DEFINE_SPINLOCK(idle_spm_spin_lock);

void idle_lock_spm(enum idle_lock_spm_id id)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_spm_spin_lock, flags);
	idle_spm_lock |= (1 << id);
	spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}

void idle_unlock_spm(enum idle_lock_spm_id id)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_spm_spin_lock, flags);
	idle_spm_lock &= ~(1 << id);
	spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}

static unsigned int idle_ufs_lock;
static DEFINE_SPINLOCK(idle_ufs_spin_lock);

void idle_lock_by_ufs(unsigned int lock)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_ufs_spin_lock, flags);
	idle_ufs_lock = lock;
	spin_unlock_irqrestore(&idle_ufs_spin_lock, flags);
}

static unsigned int idle_gpu_lock;
static DEFINE_SPINLOCK(idle_gpu_spin_lock);

void idle_lock_by_gpu(unsigned int lock)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_gpu_spin_lock, flags);
	idle_gpu_lock = lock;
	spin_unlock_irqrestore(&idle_gpu_spin_lock, flags);
}

/*
 * SODI3 part
 */
static DEFINE_SPINLOCK(soidle3_condition_mask_spin_lock);

static void enable_soidle3_by_mask(int grp, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&soidle3_condition_mask_spin_lock, flags);
	idle_condition_mask[IDLE_TYPE_SO3][grp] &= ~mask;
	spin_unlock_irqrestore(&soidle3_condition_mask_spin_lock, flags);
}

static void disable_soidle3_by_mask(int grp, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&soidle3_condition_mask_spin_lock, flags);
	idle_condition_mask[IDLE_TYPE_SO3][grp] |= mask;
	spin_unlock_irqrestore(&soidle3_condition_mask_spin_lock, flags);
}

static void enable_soidle3_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	WARN_ON(INVALID_GRP_ID(grp));
	enable_soidle3_by_mask(grp, mask);
}

static void disable_soidle3_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	WARN_ON(INVALID_GRP_ID(grp));
	disable_soidle3_by_mask(grp, mask);
}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
static bool mtk_idle_cpu_criteria(void)
{
#ifndef CONFIG_MTK_ACAO_SUPPORT
	return ((atomic_read(&is_in_hotplug) == 1) || (num_online_cpus() != 1)) ? false : true;
#else
	/* single core check will be checked mcdi driver for acao case */
	return true;
#endif
}
#endif

static bool soidle3_can_enter(int cpu, int reason)
{
	/* check previous common criterion */
	if (reason == BY_CLK) {
		if (soidle3_by_pass_cg == 0) {
			if (idle_block_mask[IDLE_TYPE_SO3][NR_GRPS])
				goto out;
		}
		reason = NR_REASONS;
	} else if (reason < NR_REASONS)
		goto out;

	#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (soidle3_by_pass_en == 0) {
		if ((spm_get_sodi_en() == 0) || (spm_get_sodi3_en() == 0)) {
			/* if SODI is disabled, SODI3 is also disabled */
			reason = BY_OTH;
			goto out;
		}
	}

	if (!mtk_idle_disp_is_pwm_rosc()) {
		reason = BY_PWM;
		goto out;
	}

	if (soidle3_by_pass_pll == 0) {
		if (!mtk_idle_check_pll(soidle3_pll_condition_mask, soidle3_pll_block_mask)) {
			reason = BY_PLL;
			goto out;
		}
	/* FIXME: */
#if 0
		/* check if univpll is used (sspm not included) */
		if (univpll_is_used()) {
			reason = BY_PLL;
			goto out;
		}
#endif
	}
	#endif

	/* do not enter sodi3 at first 30 seconds */
	if (sodi3_by_uptime_count != -1) {
		struct timespec uptime;
		unsigned long val;

		get_monotonic_boottime(&uptime);
		val = (unsigned long)uptime.tv_sec;
		if (val <= 30) {
			sodi3_by_uptime_count++;
			reason = BY_OTH;
			goto out;
		} else {
			idle_warn("SODI3: blocking by uptime, count = %d\n", sodi3_by_uptime_count);
			sodi3_by_uptime_count = -1;
		}
	}

	/* Check Secure CGs - after other SODI3 criteria PASS */
	if (!idle_by_pass_secure_cg) {
		if (!mtk_idle_check_secure_cg(idle_block_mask)) {
			reason = BY_CLK;
			goto out;
		}
	}

out:
	return mtk_idle_select_state(IDLE_TYPE_SO3, reason);
}

void soidle3_before_wfi(int cpu)
{
	timer_setting_before_wfi(true);
}

void soidle3_after_wfi(int cpu)
{
	timer_setting_after_wfi(true);

	soidle3_cnt[cpu]++;
}

/*
 * SODI part
 */
static DEFINE_SPINLOCK(soidle_condition_mask_spin_lock);

static void enable_soidle_by_mask(int grp, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&soidle_condition_mask_spin_lock, flags);
	idle_condition_mask[IDLE_TYPE_SO][grp] &= ~mask;
	spin_unlock_irqrestore(&soidle_condition_mask_spin_lock, flags);
}

static void disable_soidle_by_mask(int grp, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&soidle_condition_mask_spin_lock, flags);
	idle_condition_mask[IDLE_TYPE_SO][grp] |= mask;
	spin_unlock_irqrestore(&soidle_condition_mask_spin_lock, flags);
}

static void enable_soidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	WARN_ON(INVALID_GRP_ID(grp));
	enable_soidle_by_mask(grp, mask);
	/* enable the settings for SODI3 at the same time */
	enable_soidle3_by_mask(grp, mask);
}

static void disable_soidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	WARN_ON(INVALID_GRP_ID(grp));
	disable_soidle_by_mask(grp, mask);
	/* disable the settings for SODI3 at the same time */
	disable_soidle3_by_mask(grp, mask);
}

static bool soidle_can_enter(int cpu, int reason)
{
	/* check previous common criterion */
	if (reason == BY_CLK) {
		if (soidle_by_pass_cg == 0) {
			if (idle_block_mask[IDLE_TYPE_SO][NR_GRPS])
				goto out;
		}
		reason = NR_REASONS;
	} else if (reason < NR_REASONS)
		goto out;

	#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (soidle_by_pass_en == 0) {
		if (spm_get_sodi_en() == 0) {
			reason = BY_OTH;
			goto out;
		}
	}
	#endif

	/* do not enter sodi at first 20 seconds */
	if (sodi_by_uptime_count != -1) {
		struct timespec uptime;
		unsigned long val;

		get_monotonic_boottime(&uptime);
		val = (unsigned long)uptime.tv_sec;
		if (val <= 20) {
			sodi_by_uptime_count++;
			reason = BY_OTH;
			goto out;
		} else {
			idle_warn("SODI: blocking by uptime, count = %d\n", sodi_by_uptime_count);
			sodi_by_uptime_count = -1;
		}
	}

	/* Check Secure CGs - after other SODI criteria PASS */
	if (!idle_by_pass_secure_cg) {
		if (!mtk_idle_check_secure_cg(idle_block_mask)) {
			reason = BY_CLK;
			goto out;
		}
	}

out:
	return mtk_idle_select_state(IDLE_TYPE_SO, reason);
}

void soidle_before_wfi(int cpu)
{
	timer_setting_before_wfi(false);
}

void soidle_after_wfi(int cpu)
{
	timer_setting_after_wfi(false);

	soidle_cnt[cpu]++;
}

/*
 * multi-core idle part
 */
static DEFINE_MUTEX(mcidle_locked);
static bool mcidle_can_enter(int cpu, int reason)
{
	/* reset reason */
	reason = NR_REASONS;

#ifdef CONFIG_ARM64
	if (num_online_cpus() == 1) {
		reason = BY_CPU;
		goto mcidle_out;
	}
#else
	if (num_online_cpus() == 1) {
		reason = BY_CPU;
		goto mcidle_out;
	}
#endif

	if (spm_mcdi_can_enter() == 0) {
		reason = BY_OTH;
		goto mcidle_out;
	}

	if (!next_timer_criteria_check(mcidle_time_criteria)) {
		reason = BY_TMR;
		goto mcidle_out;
	}

mcidle_out:
	if (reason < NR_REASONS) {
		mcidle_block_cnt[cpu][reason]++;
		return false;
	}

	return true;
}

void mcidle_before_wfi(int cpu)
{
}

int mcdi_xgpt_wakeup_cnt[NR_CPUS];
void mcidle_after_wfi(int cpu)
{
}

/*
 * deep idle part
 */
static DEFINE_SPINLOCK(dpidle_condition_mask_spin_lock);

static void enable_dpidle_by_mask(int grp, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&dpidle_condition_mask_spin_lock, flags);
	idle_condition_mask[IDLE_TYPE_DP][grp] &= ~mask;
	spin_unlock_irqrestore(&dpidle_condition_mask_spin_lock, flags);
}

static void disable_dpidle_by_mask(int grp, unsigned int mask)
{
	unsigned long flags;

	spin_lock_irqsave(&dpidle_condition_mask_spin_lock, flags);
	idle_condition_mask[IDLE_TYPE_DP][grp] |= mask;
	spin_unlock_irqrestore(&dpidle_condition_mask_spin_lock, flags);
}

static void enable_dpidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	WARN_ON(INVALID_GRP_ID(grp));
	enable_dpidle_by_mask(grp, mask);
}

static void disable_dpidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	WARN_ON(INVALID_GRP_ID(grp));
	disable_dpidle_by_mask(grp, mask);
}

static bool dpidle_can_enter(int cpu, int reason)
{
	dpidle_profile_time(DPIDLE_PROFILE_CAN_ENTER_START);

	/* check previous common criterion */
	if (reason == BY_CLK) {
		if (dpidle_by_pass_cg == 0) {
			if (idle_block_mask[IDLE_TYPE_DP][NR_GRPS])
				goto out;
		}
	} else if (reason < NR_REASONS)
		goto out;

	reason = NR_REASONS;

	/* Check Secure CGs - after other deepidle criteria PASS */
	if (!idle_by_pass_secure_cg) {
		if (!mtk_idle_check_secure_cg(idle_block_mask)) {
			reason = BY_CLK;
			goto out;
		}
	}

out:
	dpidle_profile_time(DPIDLE_PROFILE_CAN_ENTER_END);

	return mtk_idle_select_state(IDLE_TYPE_DP, reason);
}

/*
 * slow idle part
 */
static DEFINE_MUTEX(slidle_locked);

static void enable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	idle_condition_mask[IDLE_TYPE_SL][grp] &= ~mask;
	mutex_unlock(&slidle_locked);
}

static void disable_slidle_by_mask(int grp, unsigned int mask)
{
	mutex_lock(&slidle_locked);
	idle_condition_mask[IDLE_TYPE_SL][grp] |= mask;
	mutex_unlock(&slidle_locked);
}

void enable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	WARN_ON(INVALID_GRP_ID(grp));
	enable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_slidle_by_bit);

void disable_slidle_by_bit(int id)
{
	int grp = id / 32;
	unsigned int mask = 1U << (id % 32);

	WARN_ON(INVALID_GRP_ID(grp));
	disable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_slidle_by_bit);

static bool slidle_can_enter(int cpu, int reason)
{
	/* check previous common criterion */
	/* reset reason if reason is not BY_CLK */
	if (reason == BY_CLK) {
		if (idle_block_mask[IDLE_TYPE_SL][NR_GRPS])
			goto out;
	}

	reason = NR_REASONS;

	#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (!mtk_idle_cpu_criteria()) {
		reason = BY_CPU;
		goto out;
	}
	#endif

out:
	if (reason < NR_REASONS) {
		slidle_block_cnt[reason]++;
		return false;
	} else {
		return true;
	}
}

static void slidle_before_wfi(int cpu)
{
	/* struct mtk_irq_mask mask; */
	bus_dcm_enable();
}

static void slidle_after_wfi(int cpu)
{
	bus_dcm_disable();
	slidle_cnt[cpu]++;
}

static void go_to_slidle(int cpu)
{
	slidle_before_wfi(cpu);

	mb();	/* memory barrier */
	__asm__ __volatile__("wfi" : : : "memory");

	slidle_after_wfi(cpu);
}


/*
 * regular idle part
 */
static bool rgidle_can_enter(int cpu, int reason)
{
	return true;
}

static void rgidle_before_wfi(int cpu)
{

}

static void rgidle_after_wfi(int cpu)
{
	rgidle_cnt[cpu]++;
}

static noinline void go_to_rgidle(int cpu)
{
	rgidle_before_wfi(cpu);

	go_to_wfi();

	rgidle_after_wfi(cpu);
}

/*
 * idle task flow part
 */

/*
 * xxidle_handler return 1 if enter and exit the low power state
 */

static u32 slp_spm_SODI3_flags = {
	SPM_FLAG_DIS_INFRA_PDN |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
#endif
	SPM_FLAG_SODI_OPTION |
	SPM_FLAG_ENABLE_SODI3
};

static u32 slp_spm_SODI_flags = {
	SPM_FLAG_DIS_INFRA_PDN |
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
#endif
	SPM_FLAG_SODI_OPTION
};

u32 slp_spm_deepidle_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	SPM_FLAG_DIS_INFRA_PDN |
	/* SPM_FLAG_DIS_DDRPHY_PDN | */
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
#endif
	SPM_FLAG_DEEPIDLE_OPTION
};

unsigned int ufs_cb_before_xxidle(void)
{
	unsigned int op_cond = 0;
	bool bblpm_check = false;

#if defined(CONFIG_MTK_UFS_BOOTING)
	bool ufs_in_hibernate = false;

	ufs_in_hibernate = !ufs_mtk_deepidle_hibern8_check();
	op_cond = ufs_in_hibernate ? DEEPIDLE_OPT_XO_UFS_ON_OFF : 0;
#endif

	bblpm_check = !clk_buf_bblpm_enter_cond();
	op_cond |= bblpm_check ? DEEPIDLE_OPT_CLKBUF_BBLPM : 0;

	return op_cond;
}

void ufs_cb_after_xxidle(void)
{
#if defined(CONFIG_MTK_UFS_BOOTING)
	ufs_mtk_deepidle_leave();
#endif
}

unsigned int soidle_pre_handler(void)
{
	unsigned int op_cond = 0;

	op_cond = ufs_cb_before_xxidle();

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifndef CONFIG_MTK_ACAO_SUPPORT
	hps_del_timer();
#endif
#endif

#ifdef CONFIG_THERMAL
	/* cancel thermal hrtimer for power saving */
	mtkTTimer_cancel_timer();
#endif
	return op_cond;
}

void soidle_post_handler(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifndef CONFIG_MTK_ACAO_SUPPORT
	hps_restart_timer();
#endif
#endif

#ifdef CONFIG_THERMAL
	/* restart thermal hrtimer for update temp info */
	mtkTTimer_start_timer();
#endif

	ufs_cb_after_xxidle();
}

static unsigned int dpidle_pre_process(int cpu)
{
	unsigned int op_cond = 0;

	dpidle_profile_time(DPIDLE_PROFILE_ENTER_UFS_CB_BEFORE_XXIDLE_START);

	op_cond = ufs_cb_before_xxidle();

	dpidle_profile_time(DPIDLE_PROFILE_ENTER_UFS_CB_BEFORE_XXIDLE_END);

	mtk_idle_notifier_call_chain(NOTIFY_DPIDLE_ENTER);

	dpidle_profile_time(DPIDLE_PROFILE_IDLE_NOTIFIER_END);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifndef CONFIG_MTK_ACAO_SUPPORT
	hps_del_timer();
#endif

#ifdef CONFIG_THERMAL
	/* cancel thermal hrtimer for power saving */
	mtkTTimer_cancel_timer();
#endif
#endif

	timer_setting_before_wfi(false);

	dpidle_profile_time(DPIDLE_PROFILE_TIMER_DEL_END);

	return op_cond;
}

static void dpidle_post_process(int cpu)
{
	dpidle_profile_time(DPIDLE_PROFILE_TIMER_RESTORE_START);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	timer_setting_after_wfi(false);

#ifndef CONFIG_MTK_ACAO_SUPPORT
	hps_restart_timer();
#endif

#ifdef CONFIG_THERMAL
	/* restart thermal hrtimer for update temp info */
	mtkTTimer_start_timer();
#endif
#endif

	dpidle_profile_time(DPIDLE_PROFILE_TIMER_RESTORE_END);

	mtk_idle_notifier_call_chain(NOTIFY_DPIDLE_LEAVE);

	dpidle_profile_time(DPIDLE_PROFILE_UFS_CB_AFTER_XXIDLE_START);

	ufs_cb_after_xxidle();

	dpidle_profile_time(DPIDLE_PROFILE_UFS_CB_AFTER_XXIDLE_END);

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	spm_dpidle_notify_sspm_after_wfi_async_wait();
#endif

	dpidle_profile_time(DPIDLE_PROFILE_NOTIFY_SSPM_AFTER_WFI_ASYNC_WAIT_END);

	dpidle_cnt[cpu]++;
}

static bool (*idle_can_enter[NR_TYPES])(int cpu, int reason) = {
	dpidle_can_enter,
	soidle3_can_enter,
	soidle_can_enter,
	mcidle_can_enter,
	slidle_can_enter,
	rgidle_can_enter,
};

/* Mapping idle_switch to CPUidle C States */
static int idle_stat_mapping_table[NR_TYPES] = {
	CPUIDLE_STATE_DP,
	CPUIDLE_STATE_SO3,
	CPUIDLE_STATE_SO,
	CPUIDLE_STATE_MC,
	CPUIDLE_STATE_SL,
	CPUIDLE_STATE_RG
};

int mtk_idle_select(int cpu)
{
	int i = NR_TYPES - 1;
	int reason = NR_REASONS;
	unsigned int gpu_locked;
	unsigned long flags = 0;
#if defined(CONFIG_MTK_UFS_BOOTING)
	unsigned int ufs_locked;
#endif
#ifdef CONFIG_MTK_DCS
	int ch = 0, ret = -1;
	enum dcs_status dcs_status;
	bool dcs_lock_get = false;
#endif

	profile_so_start(PIDX_SELECT);
	profile_so3_start(PIDX_SELECT);
	dpidle_profile_time(DPIDLE_PROFILE_IDLE_SELECT_START);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	/* check if firmware loaded or not */
	if (!spm_load_firmware_status()) {
		reason = BY_FRM;
		goto get_idle_idx_2;
	}
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_SMP
	/* check cpu status */
	if (!mtk_idle_cpu_criteria()) {
		reason = BY_CPU;
		goto get_idle_idx_2;
	}
#endif
#endif

#ifndef CONFIG_MTK_ACAO_SUPPORT
	/* only check for non-acao case */
	if (cpu % 4) {
		reason = BY_CPU;
		goto get_idle_idx_2;
	}
#endif

	if (spm_get_resource_usage() == SPM_RESOURCE_ALL) {
		reason = BY_OTH;
		goto get_idle_idx_2;
	}

#if defined(CONFIG_MTK_UFS_BOOTING)
	spin_lock_irqsave(&idle_ufs_spin_lock, flags);
	ufs_locked = idle_ufs_lock;
	spin_unlock_irqrestore(&idle_ufs_spin_lock, flags);

	if (ufs_locked) {
		reason = BY_UFS;
		goto get_idle_idx_2;
	}
#endif

	/* Check if GPU blocks deepidle/SODI */
	spin_lock_irqsave(&idle_gpu_spin_lock, flags);
	gpu_locked = idle_gpu_lock;
	spin_unlock_irqrestore(&idle_gpu_spin_lock, flags);

	if (gpu_locked) {
		reason = BY_GPU;
		goto get_idle_idx_2;
	}

	/* check idle_spm_lock */
	if (idle_spm_lock) {
		reason = BY_VTG;
		goto get_idle_idx_2;
	}

	/* teei ready */
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_MICROTRUST_TEE_SUPPORT
	if (!is_teei_ready()) {
		reason = BY_OTH;
		goto get_idle_idx_2;
	}
#endif
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	/* cg check */
	memset(idle_block_mask, 0,
		NR_TYPES * NF_CG_STA_RECORD * sizeof(unsigned int));
	if (!mtk_idle_check_cg(idle_block_mask)) {
		reason = BY_CLK;
		goto get_idle_idx_2;
	}
#endif

#ifdef CONFIG_MTK_DCS
	/* check if DCS channel switching */
	ret = dcs_get_dcs_status_trylock(&ch, &dcs_status);
	if (ret) {
		reason = BY_DCS;
		goto get_idle_idx_2;
	}

	dcs_lock_get = true;

#endif

get_idle_idx_2:
	/* check if criteria check fail in common part */
	for (i = 0; i < NR_TYPES; i++) {
		if (idle_switch[i]) {
			/* call each idle scenario check functions */
			if (idle_can_enter[i](cpu, reason))
				break;
		}
	}

	/* Prevent potential out-of-bounds vulnerability */
	i = (i >= NR_TYPES) ? NR_TYPES : i;

#ifdef CONFIG_MTK_DCS
	if (dcs_lock_get)
		dcs_get_dcs_status_unlock();
#endif

	profile_so_end(PIDX_SELECT);
	profile_so3_end(PIDX_SELECT);
	dpidle_profile_time(DPIDLE_PROFILE_IDLE_SELECT_END);

	profile_so_start(PIDX_SELECT_TO_ENTER);
	profile_so3_start(PIDX_SELECT_TO_ENTER);

	return i;
}

int mtk_idle_select_base_on_menu_gov(int cpu, int menu_select_state)
{
	int i;
	int state = CPUIDLE_STATE_RG;

	if (menu_select_state < 0)
		return menu_select_state;

	mtk_idle_dump_cnt_in_interval();

	i = mtk_idle_select(cpu);

	/* residency requirement of ALL C state is satisfied */
	if (menu_select_state == CPUIDLE_STATE_SO3) {
		state = idle_stat_mapping_table[i];
	/* SODI3.0 residency requirement does NOT satisfied */
	} else if (menu_select_state >= CPUIDLE_STATE_SO && menu_select_state <= CPUIDLE_STATE_DP) {
		if (i == IDLE_TYPE_SO3)
			i = idle_switch[IDLE_TYPE_SO] ? IDLE_TYPE_SO : IDLE_TYPE_RG;

		state = idle_stat_mapping_table[i];
	/* DPIDLE, SODI3.0, and SODI residency requirement does NOT satisfied */
	} else {
		state = CPUIDLE_STATE_RG;
	}

	return state;
}

int dpidle_enter(int cpu)
{
	int ret = CPUIDLE_STATE_DP;
	u32 operation_cond = 0;

	/* don't check lock dependency */
	lockdep_off();

	dpidle_profile_time(DPIDLE_PROFILE_ENTER);

	mtk_idle_ratio_calc_start(IDLE_TYPE_DP, cpu);

	operation_cond |= dpidle_pre_process(cpu);

	if (dpidle_gs_dump_req) {
		unsigned int current_ts = idle_get_current_time_ms();

		if ((current_ts - dpidle_gs_dump_req_ts) >= dpidle_gs_dump_delay_ms) {
			idle_warn("dpidle dump LP golden\n");

			dpidle_gs_dump_req = 0;
			operation_cond |= DEEPIDLE_OPT_DUMP_LP_GOLDEN;
		}
	}

	spm_go_to_dpidle(slp_spm_deepidle_flags, (u32)cpu, dpidle_dump_log, operation_cond);

	dpidle_post_process(cpu);

	mtk_idle_ratio_calc_stop(IDLE_TYPE_DP, cpu);

	dpidle_profile_time(DPIDLE_PROFILE_LEAVE);
	dpidle_show_profile_time();

	lockdep_on();

	/* For test */
	if (dpidle_run_once)
		idle_switch[IDLE_TYPE_DP] = 0;

	return ret;
}
EXPORT_SYMBOL(dpidle_enter);

int soidle3_enter(int cpu)
{
	int ret = CPUIDLE_STATE_SO3;
	unsigned long long soidle3_time = 0;
	static unsigned long long soidle3_residency;

	/* don't check lock dependency */
	lockdep_off();

	profile_so3_end(PIDX_SELECT_TO_ENTER);

	profile_so3_start(PIDX_ENTER_TOTAL);

	if (sodi3_flags & SODI_FLAG_RESIDENCY)
		soidle3_time = idle_get_current_time_ms();

	mtk_idle_ratio_calc_start(IDLE_TYPE_SO3, cpu);

	profile_so3_start(PIDX_IDLE_NOTIFY_ENTER);
	mtk_idle_notifier_call_chain(NOTIFY_SOIDLE3_ENTER);
	profile_so3_end(PIDX_IDLE_NOTIFY_ENTER);

#ifdef DEFAULT_MMP_ENABLE
	mmprofile_log_ex(sodi_mmp_get_events()->sodi_enable, MMPROFILE_FLAG_START, 0, 0);
#endif /* DEFAULT_MMP_ENABLE */

	spm_go_to_sodi3(slp_spm_SODI3_flags, (u32)cpu, sodi3_flags);

	/* Clear SODI_FLAG_DUMP_LP_GS in sodi3_flags */
	sodi3_flags &= (~SODI_FLAG_DUMP_LP_GS);

#ifdef DEFAULT_MMP_ENABLE
	mmprofile_log_ex(sodi_mmp_get_events()->sodi_enable, MMPROFILE_FLAG_END, 0, spm_read(SPM_PASR_DPD_3));
#endif /* DEFAULT_MMP_ENABLE */

	profile_so3_start(PIDX_IDLE_NOTIFY_LEAVE);
	mtk_idle_notifier_call_chain(NOTIFY_SOIDLE3_LEAVE);
	profile_so3_end(PIDX_IDLE_NOTIFY_LEAVE);

	mtk_idle_ratio_calc_stop(IDLE_TYPE_SO3, cpu);

	if (sodi3_flags & SODI_FLAG_RESIDENCY) {
		soidle3_residency += idle_get_current_time_ms() - soidle3_time;
		idle_dbg("SO3: soidle3_residency = %llu\n", soidle3_residency);
	}

	profile_so3_end(PIDX_LEAVE_TOTAL);

	/* dump latency profiling result */
	profile_so3_dump();

	lockdep_on();

	return ret;
}
EXPORT_SYMBOL(soidle3_enter);

int soidle_enter(int cpu)
{
	int ret = CPUIDLE_STATE_SO;
	unsigned long long soidle_time = 0;
	static unsigned long long soidle_residency;

	/* don't check lock dependency */
	lockdep_off();

	profile_so_end(PIDX_SELECT_TO_ENTER);

	profile_so_start(PIDX_ENTER_TOTAL);

	if (sodi_flags & SODI_FLAG_RESIDENCY)
		soidle_time = idle_get_current_time_ms();

	mtk_idle_ratio_calc_start(IDLE_TYPE_SO, cpu);

	profile_so_start(PIDX_IDLE_NOTIFY_ENTER);
	mtk_idle_notifier_call_chain(NOTIFY_SOIDLE_ENTER);
	profile_so_end(PIDX_IDLE_NOTIFY_ENTER);

#ifdef DEFAULT_MMP_ENABLE
	mmprofile_log_ex(sodi_mmp_get_events()->sodi_enable, MMPROFILE_FLAG_START, 0, 0);
#endif /* DEFAULT_MMP_ENABLE */

	spm_go_to_sodi(slp_spm_SODI_flags, (u32)cpu, sodi_flags);

	/* Clear SODI_FLAG_DUMP_LP_GS in sodi_flags */
	sodi_flags &= (~SODI_FLAG_DUMP_LP_GS);

#ifdef DEFAULT_MMP_ENABLE
	mmprofile_log_ex(sodi_mmp_get_events()->sodi_enable, MMPROFILE_FLAG_END, 0, spm_read(SPM_PASR_DPD_3));
#endif /* DEFAULT_MMP_ENABLE */

	profile_so_start(PIDX_IDLE_NOTIFY_LEAVE);
	mtk_idle_notifier_call_chain(NOTIFY_SOIDLE_LEAVE);
	profile_so_end(PIDX_IDLE_NOTIFY_LEAVE);

	mtk_idle_ratio_calc_stop(IDLE_TYPE_SO, cpu);

	if (sodi_flags & SODI_FLAG_RESIDENCY) {
		soidle_residency += idle_get_current_time_ms() - soidle_time;
		idle_dbg("SO: soidle_residency = %llu\n", soidle_residency);
	}

	profile_so_end(PIDX_LEAVE_TOTAL);

	/* dump latency profiling result */
	profile_so_dump();

	lockdep_on();

	return ret;
}
EXPORT_SYMBOL(soidle_enter);

int mcidle_enter(int cpu)
{
	int ret = CPUIDLE_STATE_MC;

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	go_to_mcidle(cpu);
	mcidle_cnt[cpu] += 1;
#endif

	return ret;
}
EXPORT_SYMBOL(mcidle_enter);

int slidle_enter(int cpu)
{
	int ret = CPUIDLE_STATE_SL;

	go_to_slidle(cpu);

	return ret;
}
EXPORT_SYMBOL(slidle_enter);

int rgidle_enter(int cpu)
{
	int ret = CPUIDLE_STATE_RG;

#ifdef CONFIG_MTK_ACAO_SUPPORT
#endif

	mtk_idle_ratio_calc_start(IDLE_TYPE_RG, cpu);

	idle_refcnt_inc();

	go_to_rgidle(cpu);

	idle_refcnt_dec();

	mtk_idle_ratio_calc_stop(IDLE_TYPE_RG, cpu);

	return ret;
}
EXPORT_SYMBOL(rgidle_enter);

/*
 * debugfs
 */
static char dbg_buf[4096] = { 0 };
static char cmd_buf[512] = { 0 };

#undef mt_idle_log
#define mt_idle_log(fmt, args...)	log2buf(p, dbg_buf, fmt, ##args)

/* idle_state */
static int _idle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int idle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _idle_state_open, inode->i_private);
}

static ssize_t idle_state_read(struct file *filp,
			       char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbg_buf;

	p[0] = '\0';
	mt_idle_log("********** idle state dump **********\n");

	for (i = 0; i < nr_cpu_ids; i++) {
		mt_idle_log("dpidle_cnt[%d]=%lu, dpidle_26m[%d]=%lu, soidle3_cnt[%d]=%lu, soidle_cnt[%d]=%lu, ",
			i, dpidle_cnt[i], i, dpidle_f26m_cnt[i], i, soidle3_cnt[i], i, soidle_cnt[i]);
		mt_idle_log("mcidle_cnt[%d]=%lu, slidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
			i, mcidle_cnt[i], i, slidle_cnt[i], i, rgidle_cnt[i]);
	}

	mt_idle_log("\n********** variables dump **********\n");
	for (i = 0; i < NR_TYPES; i++)
		mt_idle_log("%s_switch=%d, ", mtk_get_idle_name(i), idle_switch[i]);

	mt_idle_log("\n");
	mt_idle_log("idle_ratio_en = %u\n", mtk_idle_get_ratio_status());
	mt_idle_log("twam_handler:%s (clk:%s)\n",
					(mtk_idle_get_twam()->running)?"on":"off",
					(mtk_idle_get_twam()->speed_mode)?"speed":"normal");

	mt_idle_log("bypass_secure_cg = %u\n", idle_by_pass_secure_cg);

	mt_idle_log("\n********** idle command help **********\n");
	mt_idle_log("status help:   cat /sys/kernel/debug/cpuidle/idle_state\n");
	mt_idle_log("switch on/off: echo switch mask > /sys/kernel/debug/cpuidle/idle_state\n");
	mt_idle_log("idle ratio profile: echo ratio 1/0 > /sys/kernel/debug/cpuidle/idle_state\n");

	mt_idle_log("soidle3 help:  cat /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("soidle help:   cat /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("dpidle help:   cat /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("mcidle help:   cat /sys/kernel/debug/cpuidle/mcidle_state\n");
	mt_idle_log("slidle help:   cat /sys/kernel/debug/cpuidle/slidle_state\n");
	mt_idle_log("rgidle help:   cat /sys/kernel/debug/cpuidle/rgidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t idle_state_write(struct file *filp,
				const char __user *userbuf, size_t count, loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int idx;
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %x", cmd, &param) == 2) {
		if (!strcmp(cmd, "switch")) {
			for (idx = 0; idx < NR_TYPES; idx++)
				idle_switch[idx] = (param & (1U << idx)) ? 1 : 0;
		} else if (!strcmp(cmd, "ratio")) {
			if (param == 1)
				mtk_idle_enable_ratio_calc();
			else
				mtk_idle_disable_ratio_calc();
		} else if (!strcmp(cmd, "spmtwam_clk")) {
			mtk_idle_get_twam()->speed_mode = param;
		} else if (!strcmp(cmd, "spmtwam_sel")) {
			mtk_idle_get_twam()->sel = param;
		} else if (!strcmp(cmd, "spmtwam")) {
#if !defined(CONFIG_FPGA_EARLY_PORTING)
			idle_dbg("spmtwam_event = %d\n", param);
			if (param >= 0)
				mtk_idle_twam_enable((u32)param);
			else
				mtk_idle_twam_disable();
#endif
		} else if (!strcmp(cmd, "bypass_secure_cg")) {
			idle_by_pass_secure_cg = param;
		}
		return count;
	}

	return -EINVAL;
}

static const struct file_operations idle_state_fops = {
	.open = idle_state_open,
	.read = idle_state_read,
	.write = idle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* mcidle_state */
static int _mcidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int mcidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _mcidle_state_open, inode->i_private);
}

static ssize_t mcidle_state_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int len = 0;
	int cpus, reason;
	char *p = dbg_buf;

	p[0] = '\0';
	mt_idle_log("*********** deep idle state ************\n");
	mt_idle_log("mcidle_time_criteria=%u\n", mcidle_time_criteria);

	for (cpus = 0; cpus < nr_cpu_ids; cpus++) {
		mt_idle_log("cpu:%d\n", cpus);
		for (reason = 0; reason < NR_REASONS; reason++) {
			mt_idle_log("[%d]mcidle_block_cnt[%s]=%lu\n",
				reason, mtk_get_reason_name(reason), mcidle_block_cnt[cpus][reason]);
		}
		mt_idle_log("\n");
	}

	mt_idle_log("\n********** mcidle command help **********\n");
	mt_idle_log("mcidle help:   cat /sys/kernel/debug/cpuidle/mcidle_state\n");
	mt_idle_log("switch on/off: echo [mcidle] 1/0 > /sys/kernel/debug/cpuidle/mcidle_state\n");
	mt_idle_log("modify tm_cri: echo time value(dec) > /sys/kernel/debug/cpuidle/mcidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t mcidle_state_write(struct file *filp,
				  const char __user *userbuf,
				  size_t count,
				  loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "mcidle"))
			idle_switch[IDLE_TYPE_MC] = param;
		else if (!strcmp(cmd, "time"))
			mcidle_time_criteria = param;

		return count;
	} else if (!kstrtoint(cmd_buf, 10, &param) == 1) {
		idle_switch[IDLE_TYPE_MC] = param;

		return count;
	}

	return -EINVAL;
}

static const struct file_operations mcidle_state_fops = {
	.open = mcidle_state_open,
	.read = mcidle_state_read,
	.write = mcidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* dpidle_state */
static int _dpidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int dpidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _dpidle_state_open, inode->i_private);
}

static ssize_t dpidle_state_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, k, len = 0;
	char *p = dbg_buf;

	p[0] = '\0';
	mt_idle_log("*********** deep idle state ************\n");
	mt_idle_log("dpidle_time_criteria=%u\n", dpidle_time_criteria);

	for (i = 0; i < NR_REASONS; i++)
		mt_idle_log("[%d]dpidle_block_cnt[%s]=%lu\n", i, mtk_get_reason_name(i), dpidle_block_cnt[i]);
	mt_idle_log("\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%02d]dpidle_condition_mask[%-8s]=0x%08x\t\tdpidle_block_mask[%-8s]=0x%08x\n", i,
				mtk_get_cg_group_name(i), idle_condition_mask[IDLE_TYPE_DP][i],
				mtk_get_cg_group_name(i), idle_block_mask[IDLE_TYPE_DP][i]);
	}

	mt_idle_log("dpidle pg_stat=0x%08x\n", idle_block_mask[IDLE_TYPE_DP][NR_GRPS + 1]);

	mt_idle_log("dpidle_blocking_stat=\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%-8s] - ", mtk_get_cg_group_name(i));

		for (k = 0; k < 32; k++) {
			if (dpidle_blocking_stat[i][k] != 0)
				mt_idle_log("%-2d: %d", k, dpidle_blocking_stat[i][k]);
		}
		mt_idle_log("\n");
	}
	for (i = 0; i < NR_GRPS; i++)
		for (k = 0; k < 32; k++)
			dpidle_blocking_stat[i][k] = 0;

	mt_idle_log("dpidle_by_pass_cg=%u\n", dpidle_by_pass_cg);
	mt_idle_log("dpidle_by_pass_pg=%u\n", dpidle_by_pass_pg);
	mt_idle_log("dpidle_dump_log = %u\n", dpidle_dump_log);
	mt_idle_log("([0]: Reduced, [1]: Full, [2]: resource_usage\n");
	mt_idle_log("force VCORE lp mode = %u\n", dpidle_force_vcore_lp_mode);

	mt_idle_log("\n*********** dpidle command help  ************\n");
	mt_idle_log("dpidle help:   cat /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("switch on/off: echo [dpidle] 1/0 > /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("cpupdn on/off: echo cpupdn 1/0 > /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("en_dp_by_bit:  echo enable id > /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("dis_dp_by_bit: echo disable id > /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("modify tm_cri: echo time value(dec) > /sys/kernel/debug/cpuidle/dpidle_state\n");
	mt_idle_log("bypass cg:     echo bypass 1/0 > /sys/kernel/debug/cpuidle/dpidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t dpidle_state_write(struct file *filp,
									const char __user *userbuf,
									size_t count,
									loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "dpidle"))
			idle_switch[IDLE_TYPE_DP] = param;
		else if (!strcmp(cmd, "enable"))
			enable_dpidle_by_bit(param);
		else if (!strcmp(cmd, "once"))
			dpidle_run_once = param;
		else if (!strcmp(cmd, "disable"))
			disable_dpidle_by_bit(param);
		else if (!strcmp(cmd, "time"))
			dpidle_time_criteria = param;
		else if (!strcmp(cmd, "bypass"))
			dpidle_by_pass_cg = param;
		else if (!strcmp(cmd, "bypass_pg")) {
			dpidle_by_pass_pg = param;
			idle_warn("bypass_pg = %d\n", dpidle_by_pass_pg);
		} else if (!strcmp(cmd, "golden")) {
			dpidle_gs_dump_req = param;

			if (dpidle_gs_dump_req)
				dpidle_gs_dump_req_ts = idle_get_current_time_ms();
		} else if (!strcmp(cmd, "golden_delay_ms")) {
			dpidle_gs_dump_delay_ms = (param >= 0) ? param : 0;
		} else if (!strcmp(cmd, "profile_sampling"))
			dpidle_set_profile_sampling(param);
		else if (!strcmp(cmd, "dump_profile_result"))
			(param) ? dpidle_show_profile_result() : 0;
		else if (!strcmp(cmd, "log"))
			dpidle_dump_log = param;
		else if (!strcmp(cmd, "force_vcore"))
			dpidle_force_vcore_lp_mode = param;

		return count;
	} else if (!kstrtoint(cmd_buf, 10, &param) == 1) {
		idle_switch[IDLE_TYPE_DP] = param;

		return count;
	}

	return -EINVAL;
}

static const struct file_operations dpidle_state_fops = {
	.open = dpidle_state_open,
	.read = dpidle_state_read,
	.write = dpidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* soidle3_state */
static int _soidle3_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int soidle3_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _soidle3_state_open, inode->i_private);
}

static ssize_t soidle3_state_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbg_buf;

	p[0] = '\0';
	mt_idle_log("*********** soidle3 state ************\n");

	for (i = 0; i < NR_REASONS; i++)
		mt_idle_log("[%d]soidle3_block_cnt[%s]=%lu\n", i, mtk_get_reason_name(i), soidle3_block_cnt[i]);
	mt_idle_log("\n");

	for (i = 0; i < NR_PLLS; i++) {
		mt_idle_log("[%02d]soidle3_pll_condition_mask[%-8s]=0x%08x\t\tsoidle3_pll_block_mask[%-8s]=0x%08x\n", i,
			mtk_get_pll_group_name(i), soidle3_pll_condition_mask[i],
			mtk_get_pll_group_name(i), soidle3_pll_block_mask[i]);
	}
	mt_idle_log("\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%02d]soidle3_condition_mask[%-8s]=0x%08x\t\tsoidle3_block_mask[%-8s]=0x%08x\n", i,
			mtk_get_cg_group_name(i), idle_condition_mask[IDLE_TYPE_SO3][i],
			mtk_get_cg_group_name(i), idle_block_mask[IDLE_TYPE_SO3][i]);
	}

	mt_idle_log("soidle3 pg_stat=0x%08x\n", idle_block_mask[IDLE_TYPE_SO3][NR_GRPS + 1]);
	mt_idle_log("soidle3_bypass_pll=%u\n", soidle3_by_pass_pll);
	mt_idle_log("soidle3_bypass_cg=%u\n", soidle3_by_pass_cg);
	mt_idle_log("soidle3_bypass_en=%u\n", soidle3_by_pass_en);
	mt_idle_log("sodi3_flags=0x%x\n", sodi3_flags);
	mt_idle_log("sodi3_force_vcore_lp_mode=%d\n", sodi3_force_vcore_lp_mode);

	mt_idle_log("\n*********** soidle3 command help  ************\n");
	mt_idle_log("soidle3 help:  cat /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("switch on/off: echo [soidle3] 1/0 > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("en_dp_by_bit:  echo enable id > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("dis_dp_by_bit: echo disable id > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("bypass pll:    echo bypass_pll 1/0 > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("bypass cg:     echo bypass 1/0 > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("bypass en:     echo bypass_en 1/0 > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("sodi3 flags:   echo sodi3_flags value > /sys/kernel/debug/cpuidle/soidle3_state\n");
	mt_idle_log("\t[0] reduce log, [1] residency, [2] resource usage\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t soidle3_state_write(struct file *filp,
									const char __user *userbuf,
									size_t count,
									loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "soidle3"))
			idle_switch[IDLE_TYPE_SO3] = param;
		else if (!strcmp(cmd, "enable"))
			enable_soidle3_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_soidle3_by_bit(param);
		else if (!strcmp(cmd, "bypass_pll")) {
			soidle3_by_pass_pll = param;
			idle_dbg("bypass_pll = %d\n", soidle3_by_pass_pll);
		} else if (!strcmp(cmd, "bypass")) {
			soidle3_by_pass_cg = param;
			idle_dbg("bypass = %d\n", soidle3_by_pass_cg);
		} else if (!strcmp(cmd, "bypass_en")) {
			soidle3_by_pass_en = param;
			idle_dbg("bypass_en = %d\n", soidle3_by_pass_en);
		} else if (!strcmp(cmd, "sodi3_flags")) {
			sodi3_flags = param;
			idle_dbg("sodi3_flags = 0x%x\n", sodi3_flags);
		} else if (!strcmp(cmd, "sodi3_force_vcore_lp_mode")) {
			sodi3_force_vcore_lp_mode = param ? true : false;
			idle_dbg("sodi3_force_vcore_lp_mode = %d\n", sodi3_force_vcore_lp_mode);
		}
		return count;
	} else if (!kstrtoint(cmd_buf, 10, &param) == 1) {
		idle_switch[IDLE_TYPE_SO3] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations soidle3_state_fops = {
	.open = soidle3_state_open,
	.read = soidle3_state_read,
	.write = soidle3_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* soidle_state */
static int _soidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int soidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _soidle_state_open, inode->i_private);
}

static ssize_t soidle_state_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbg_buf;

	p[0] = '\0';
	mt_idle_log("*********** soidle state ************\n");

	for (i = 0; i < NR_REASONS; i++)
		mt_idle_log("[%d]soidle_block_cnt[%s]=%lu\n", i, mtk_get_reason_name(i), soidle_block_cnt[i]);
	mt_idle_log("\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%02d]soidle_condition_mask[%-8s]=0x%08x\t\tsoidle_block_mask[%-8s]=0x%08x\n", i,
			mtk_get_cg_group_name(i), idle_condition_mask[IDLE_TYPE_SO][i],
			mtk_get_cg_group_name(i), idle_block_mask[IDLE_TYPE_SO][i]);
	}

	mt_idle_log("soidle pg_stat=0x%08x\n", idle_block_mask[IDLE_TYPE_SO][NR_GRPS + 1]);

	mt_idle_log("soidle_bypass_cg=%u\n", soidle_by_pass_cg);
	mt_idle_log("soidle_by_pass_pg=%u\n", soidle_by_pass_pg);
	mt_idle_log("soidle_bypass_en=%u\n", soidle_by_pass_en);
	mt_idle_log("sodi_flags=0x%x\n", sodi_flags);

	mt_idle_log("\n*********** soidle command help  ************\n");
	mt_idle_log("soidle help:   cat /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("switch on/off: echo [soidle] 1/0 > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("en_dp_by_bit:  echo enable id > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("dis_dp_by_bit: echo disable id > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("bypass cg:     echo bypass 1/0 > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("bypass en:     echo bypass_en 1/0 > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("sodi flags:    echo sodi_flags value > /sys/kernel/debug/cpuidle/soidle_state\n");
	mt_idle_log("\t[0] reduce log, [1] residency, [2] resource usage\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t soidle_state_write(struct file *filp,
									const char __user *userbuf,
									size_t count,
									loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(cmd_buf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "soidle"))
			idle_switch[IDLE_TYPE_SO] = param;
		else if (!strcmp(cmd, "enable"))
			enable_soidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_soidle_by_bit(param);
		else if (!strcmp(cmd, "bypass")) {
			soidle_by_pass_cg = param;
			idle_dbg("bypass = %d\n", soidle_by_pass_cg);
		} else if (!strcmp(cmd, "bypass_pg")) {
			soidle_by_pass_pg = param;
			idle_warn("bypass_pg = %d\n", soidle_by_pass_pg);
		} else if (!strcmp(cmd, "bypass_en")) {
			soidle_by_pass_en = param;
			idle_dbg("bypass_en = %d\n", soidle_by_pass_en);
		} else if (!strcmp(cmd, "sodi_flags")) {
			sodi_flags = param;
			idle_dbg("sodi_flags = 0x%x\n", sodi_flags);
		}
		return count;
	} else if (!kstrtoint(cmd_buf, 10, &param) == 1) {
		idle_switch[IDLE_TYPE_SO] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations soidle_state_fops = {
	.open = soidle_state_open,
	.read = soidle_state_read,
	.write = soidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* slidle_state */
static int _slidle_state_open(struct seq_file *s, void *data)
{
	return 0;
}

static int slidle_state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _slidle_state_open, inode->i_private);
}

static ssize_t slidle_state_read(struct file *filp, char __user *userbuf, size_t count,
				 loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbg_buf;

	p[0] = '\0';
	mt_idle_log("*********** slow idle state ************\n");

	for (i = 0; i < NR_REASONS; i++)
		mt_idle_log("[%d]slidle_block_cnt[%s]=%lu\n", i, mtk_get_reason_name(i), slidle_block_cnt[i]);
	mt_idle_log("\n");

	for (i = 0; i < NR_GRPS; i++) {
		mt_idle_log("[%02d]slidle_condition_mask[%-8s]=0x%08x\t\tslidle_block_mask[%-8s]=0x%08x\n", i,
			mtk_get_cg_group_name(i), idle_condition_mask[IDLE_TYPE_SL][i],
			mtk_get_cg_group_name(i), idle_block_mask[IDLE_TYPE_SL][i]);
	}

	mt_idle_log("\n********** slidle command help **********\n");
	mt_idle_log("slidle help:   cat /sys/kernel/debug/cpuidle/slidle_state\n");
	mt_idle_log("switch on/off: echo [slidle] 1/0 > /sys/kernel/debug/cpuidle/slidle_state\n");

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t slidle_state_write(struct file *filp, const char __user *userbuf,
				  size_t count, loff_t *f_pos)
{
	char cmd[NR_CMD_BUF];
	int param;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	if (sscanf(userbuf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "slidle"))
			idle_switch[IDLE_TYPE_SL] = param;
		else if (!strcmp(cmd, "enable"))
			enable_slidle_by_bit(param);
		else if (!strcmp(cmd, "disable"))
			disable_slidle_by_bit(param);

		return count;
	} else if (!kstrtoint(userbuf, 10, &param) == 1) {
		idle_switch[IDLE_TYPE_SL] = param;
		return count;
	}

	return -EINVAL;
}

static const struct file_operations slidle_state_fops = {
	.open = slidle_state_open,
	.read = slidle_state_read,
	.write = slidle_state_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* CG/PLL/MTCMOS register dump */
static int _reg_dump_open(struct seq_file *s, void *data)
{
	return 0;
}

static int reg_dump_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _reg_dump_open, inode->i_private);
}

static ssize_t reg_dump_read(struct file *filp, char __user *userbuf, size_t count, loff_t *f_pos)
{
	int len = 0;
	char *p = dbg_buf;

	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t reg_dump_write(struct file *filp,
									const char __user *userbuf,
									size_t count,
									loff_t *f_pos)
{
	count = min(count, sizeof(cmd_buf) - 1);

	return count;
}

static const struct file_operations reg_dump_fops = {
	.open = reg_dump_open,
	.read = reg_dump_read,
	.write = reg_dump_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* debugfs entry */
static struct dentry *root_entry;

static int mtk_cpuidle_debugfs_init(void)
{
	/* TODO: check if debugfs_create_file() failed */
	/* Initialize debugfs */
	root_entry = debugfs_create_dir("cpuidle", NULL);
	if (!root_entry) {
		idle_err("Can not create debugfs `dpidle_state`\n");
		return 1;
	}

	debugfs_create_file("idle_state", 0644, root_entry, NULL, &idle_state_fops);
	debugfs_create_file("dpidle_state", 0644, root_entry, NULL, &dpidle_state_fops);
	debugfs_create_file("soidle3_state", 0644, root_entry, NULL, &soidle3_state_fops);
	debugfs_create_file("soidle_state", 0644, root_entry, NULL, &soidle_state_fops);
	debugfs_create_file("mcidle_state", 0644, root_entry, NULL, &mcidle_state_fops);
	debugfs_create_file("slidle_state", 0644, root_entry, NULL, &slidle_state_fops);
	debugfs_create_file("reg_dump", 0644, root_entry, NULL, &reg_dump_fops);

	return 0;
}

#ifndef CONFIG_MTK_ACAO_SUPPORT
/* CPU hotplug notifier, for informing whether CPU hotplug is working */
static int mtk_idle_cpu_callback(struct notifier_block *nfb,
				   unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		atomic_inc(&is_in_hotplug);
		break;

	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		atomic_dec(&is_in_hotplug);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block mtk_idle_cpu_notifier = {
	.notifier_call = mtk_idle_cpu_callback,
	.priority   = INT_MAX,
};

static int mtk_idle_hotplug_cb_init(void)
{
	register_cpu_notifier(&mtk_idle_cpu_notifier);

	return 0;
}
#endif

#if defined(CONFIG_MACH_MT6763)
void mtk_spm_dump_debug_info(void)
{
	pr_info("SPM_POWER_ON_VAL0     0x%08x\n", spm_read(SPM_POWER_ON_VAL0));
	pr_info("SPM_POWER_ON_VAL1     0x%08x\n", spm_read(SPM_POWER_ON_VAL1));
	pr_info("PCM_PWR_IO_EN         0x%08x\n", spm_read(PCM_PWR_IO_EN));
	pr_info("PCM_REG0_DATA         0x%08x\n", spm_read(PCM_REG0_DATA));
	pr_info("PCM_REG7_DATA         0x%08x\n", spm_read(PCM_REG7_DATA));
	pr_info("PCM_REG12_DATA        0x%08x\n", spm_read(PCM_REG12_DATA));
	pr_info("PCM_REG13_DATA        0x%08x\n", spm_read(PCM_REG13_DATA));
	pr_info("PCM_REG15_DATA        0x%08x\n", spm_read(PCM_REG15_DATA));
	pr_info("SPM_MAS_PAUSE_MASK_B  0x%08x\n", spm_read(SPM_MAS_PAUSE_MASK_B));
	pr_info("SPM_MAS_PAUSE2_MASK_B 0x%08x\n", spm_read(SPM_MAS_PAUSE2_MASK_B));
	pr_info("SPM_SW_FLAG           0x%08x\n", spm_read(SPM_SW_FLAG));
	pr_info("SPM_DEBUG_FLAG        0x%08x\n", spm_read(SPM_SW_DEBUG));
	pr_info("SPM_PC_TRACE_G0       0x%08x\n", spm_read(SPM_PC_TRACE_G0));
	pr_info("SPM_PC_TRACE_G1       0x%08x\n", spm_read(SPM_PC_TRACE_G1));
	pr_info("SPM_PC_TRACE_G2       0x%08x\n", spm_read(SPM_PC_TRACE_G2));
	pr_info("SPM_PC_TRACE_G3       0x%08x\n", spm_read(SPM_PC_TRACE_G3));
	pr_info("SPM_PC_TRACE_G4       0x%08x\n", spm_read(SPM_PC_TRACE_G4));
	pr_info("SPM_PC_TRACE_G5       0x%08x\n", spm_read(SPM_PC_TRACE_G5));
	pr_info("SPM_PC_TRACE_G6       0x%08x\n", spm_read(SPM_PC_TRACE_G6));
	pr_info("SPM_PC_TRACE_G7       0x%08x\n", spm_read(SPM_PC_TRACE_G7));
	pr_info("DCHA_GATING_LATCH_0   0x%08x\n", spm_read(DCHA_GATING_LATCH_0));
	pr_info("DCHA_GATING_LATCH_5   0x%08x\n", spm_read(DCHA_GATING_LATCH_5));
	pr_info("DCHB_GATING_LATCH_0   0x%08x\n", spm_read(DCHB_GATING_LATCH_0));
	pr_info("DCHB_GATING_LATCH_5   0x%08x\n", spm_read(DCHB_GATING_LATCH_5));
}
#endif

void mtk_idle_gpt_init(void)
{
#ifndef USING_STD_TIMER_OPS
	int err = 0;

	err = request_gpt(IDLE_GPT, GPT_ONE_SHOT, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1,
			  0, NULL, GPT_NOAUTOEN);
	if (err)
		idle_warn("[%s] fail to request GPT %d\n", __func__, IDLE_GPT + 1);
#endif
}

static void mtk_idle_profile_init(void)
{
	mtk_idle_twam_init();
	mtk_idle_block_setting(IDLE_TYPE_DP, dpidle_cnt, dpidle_block_cnt, idle_block_mask[IDLE_TYPE_DP]);
	mtk_idle_block_setting(IDLE_TYPE_SO3, soidle3_cnt, soidle3_block_cnt, idle_block_mask[IDLE_TYPE_SO3]);
	mtk_idle_block_setting(IDLE_TYPE_SO, soidle_cnt, soidle_block_cnt, idle_block_mask[IDLE_TYPE_SO]);
	mtk_idle_block_setting(IDLE_TYPE_SL, slidle_cnt, slidle_block_cnt, idle_block_mask[IDLE_TYPE_SL]);
	mtk_idle_block_setting(IDLE_TYPE_RG, rgidle_cnt, NULL, NULL);
}

void __init mtk_cpuidle_framework_init(void)
{
	idle_ver("[%s]entry!!\n", __func__);

#ifdef CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES
	/* FIXME: disable sodi/dpidle in mduse projects.
	 * k63v1_64_mduse
	 * k63v1_64_op01_mduse
	 * k63v1_64_op02_lwtg_mduse
	 * k63v1_64_op09_lwcg_mduse
	 */
	if (strstr(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES, "_mduse") != NULL) {

		idle_switch[IDLE_TYPE_DP] = 0;
		idle_switch[IDLE_TYPE_SO3] = 0;
		idle_switch[IDLE_TYPE_SO] = 0;
	}
#endif

	iomap_init();
	mtk_cpuidle_debugfs_init();

#ifndef CONFIG_MTK_ACAO_SUPPORT
	mtk_idle_hotplug_cb_init();
#endif

	mtk_idle_gpt_init();

	dpidle_by_pass_pg = false;
	mtk_idle_profile_init();
}
EXPORT_SYMBOL(mtk_cpuidle_framework_init);

module_param(mt_idle_chk_golden, bool, 0644);
module_param(mt_dpidle_chk_golden, bool, 0644);
