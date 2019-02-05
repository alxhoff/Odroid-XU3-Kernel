#ifndef __OPTIGAME_GOVERNOR_STATS_H__
#define __OPTIGAME_GOVERNOR_STATS_H__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/kernel_stat.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/netdevice.h>

#include <linux/platform_data/ina231.h>

#include <platform/mali_kbase_platform.h>
#include <mali_kbase.h>

#include "chrome_governor_kernel_write.h"

struct optigame_stats {
    bool initd;

    unsigned int mali_freq;
    unsigned int mali_load;
    unsigned int mali_norm_load;

    unsigned int ina_big_uW;
    unsigned int ina_little_uW;
    unsigned int ina_mem_uW;
    unsigned int ina_gpu_uW;
    
    struct ina231_sensor *ina_big;
    struct ina231_sensor *ina_little;
    struct ina231_sensor *ina_mem;
    struct ina231_sensor *ina_gpu;

    struct platform_device *gpu;
    struct kbase_device *kbdev;
};

int optigame_stats_get(struct optigame_stats *stats);
int optigame_stats_init(struct optigame_stats *stats);
int optigame_stats_debug_print(struct optigame_stats *stats);

//#include "mali_kbase.h"

//void optigame_mali_get_stats(void);

#endif /* __OPTIGAME_GOVERNOR_STATS_H__ */
