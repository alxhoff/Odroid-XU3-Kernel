#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <asm/segment.h>

/** #include <platform/mali_kbase_platform.h> */
/** #include <mali_kbase.h> */

#include "optigame_governor_stats.h"

int optigame_stats_check_ina_enable(struct ina231_sensor *sen)
{
    if(sen->pd)
        return sen->pd->enable;
    return 0;
}

unsigned int optigame_stats_get_curuW(struct ina231_sensor *sen)
{
    unsigned int cur_uW;

    if(optigame_stats_check_ina_enable(sen)){
        mutex_lock(&sen->mutex);
        cur_uW = sen->cur_uW;
        mutex_unlock(&sen->mutex);

        return cur_uW;
    }
    return 0;
}

int optigame_stats_get_ina(struct optigame_stats *stats)
{
    if(stats->ina_big)
        stats->ina_big_uW = optigame_stats_get_curuW(stats->ina_big);
    else{
        KERNEL_DEBUG_MSG(" [OPTIGOV] stats get big failed\n");
        return -ENOENT;
    }

    if(stats->ina_little)
        stats->ina_little_uW = optigame_stats_get_curuW(stats->ina_little);
    else{
        KERNEL_DEBUG_MSG(" [OPTIGOV] stats get little failed\n");
        return -ENOENT;
    }

    if(stats->ina_mem)
        stats->ina_mem_uW = optigame_stats_get_curuW(stats->ina_mem);
    else{
        KERNEL_DEBUG_MSG(" [OPTIGOV] stats get mem failed\n");
        return -ENOENT;
    }

    if(stats->ina_gpu)
        stats->ina_gpu_uW = optigame_stats_get_curuW(stats->ina_gpu);
    else{
        KERNEL_DEBUG_MSG(" [OPTIGOV] stats get gpu failed\n");
        return -ENOENT;
    }

    return 0;
}

int optigame_stats_get_gpu(struct optigame_stats *stats)
{
    unsigned long flags;
    struct exynos_context *platform;
   
    if(stats->kbdev)
        platform = (struct exynos_context *)stats->kbdev->platform_context;
    else
        return -ENOENT;

    if (!platform) {
        GPU_LOG(DVFS_ERROR, "platform context (0x%p) is not initialized \
                within %s\n", platform, __FUNCTION__);
        return -ENOENT;
    }

    spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
    stats->mali_norm_load = platform->norm_utilisation;
    stats->mali_load = platform->utilization;
    stats->mali_freq = platform->freq_for_normalisation;
    spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

    return 0;
}

int optigame_stats_get(struct optigame_stats *stats)
{
    int ret = 0;

    ret = optigame_stats_get_ina(stats);

    if(ret){
        KERNEL_DEBUG_MSG(" [OPTIGOV] stats get ina failed\n");
        return ret;
    }

    ret = optigame_stats_get_gpu(stats);

    if(ret)
        KERNEL_DEBUG_MSG(" [OPTIGOV] stats get gpu failed\n");

    return ret;
}

int optigame_stats_gpu_init(struct optigame_stats *stats)
{
    struct device_node *np;  /** Get cpufreq DT bridge */
    np = of_find_node_by_name(NULL, "cpufreq");
    if(!np) return -ENOENT;

    if(!stats->gpu){
        struct device_node *gpu;
        gpu = of_parse_phandle(np, "cpufreq,gpu", 0);
        if(!gpu) return -ENOENT;

        stats->gpu = of_find_device_by_node(gpu);
        if(!stats->gpu) return -ENOENT;

        of_node_put(gpu); /** Clean up device_node pointer */

        stats->kbdev  =  dev_get_drvdata(&stats->gpu->dev);
        if(!stats->kbdev) return -ENOENT;

        return 0;
    }

    if(np)
        of_node_put(np);

    return -EINVAL;
}

int optigame_stats_ina_init(struct optigame_stats *stats){
    struct device_node *np, *ina;
    struct i2c_client *cl;
    
    np = of_find_node_by_name(NULL, "cpufreq"); /** Get DT bridge node */
    if(!np) return -ENOENT;

    if(!stats->ina_big){
        ina = of_parse_phandle(np, "cpufreq,a15_ina", 0);
        if(!ina) return -ENOENT;

        cl = of_find_i2c_device_by_node(ina); /** Get i2c client */
        if(!cl) return -ENOENT;

        of_node_put(ina);

        stats->ina_big = dev_get_drvdata(&cl->dev);
        if(!stats->ina_big) return -ENOENT;
    }
    
    if(!stats->ina_little){
        ina = of_parse_phandle(np, "cpufreq,a7_ina", 0);
        if(!ina) return -ENOENT;

        cl = of_find_i2c_device_by_node(ina); 
        if(!cl) return -ENOENT;

        of_node_put(ina);

        stats->ina_little = dev_get_drvdata(&cl->dev);
        if(!stats->ina_little) return -ENOENT;
    }

    if(!stats->ina_gpu){
        ina = of_parse_phandle(np, "cpufreq,gpu_ina", 0);
        if(!ina) return -ENOENT;

        cl = of_find_i2c_device_by_node(ina);
        if(!cl) return -ENOENT;

        of_node_put(ina);

        stats->ina_gpu = dev_get_drvdata(&cl->dev);
        if(!stats->ina_gpu) return -ENOENT;
    }

    if(!stats->ina_mem){
        ina = of_parse_phandle(np, "cpufreq,mem_ina", 0);
        if(!ina) return -ENOENT;

        cl = of_find_i2c_device_by_node(ina);
        if(!cl) return -ENOENT;

        of_node_put(ina);

        stats->ina_mem = dev_get_drvdata(&cl->dev);
        if(!stats->ina_mem) return -ENOENT;
    }

    if(np) 
        of_node_put(np);
    
    return 0;
}

int optigame_stats_init(struct optigame_stats *stats)
{
    int ret = 0;

    ret = optigame_stats_gpu_init(stats);

    if(ret) return ret;

    ret = optigame_stats_ina_init(stats);

    if(ret) return ret;

    stats->initd = true;
    KERNEL_DEBUG_MSG(" [OPTIGOV] stats init'd\n");

    return ret;
}

int optigame_stats_debug_print(struct optigame_stats *stats)
{
    int ret = 0;
    if(stats){
        if(stats->initd){
            ret = optigame_stats_get(stats);

            if(ret) return ret;

            KERNEL_DEBUG_MSG(" [OPTIGOV] stats debug\n");
            KERNEL_DEBUG_MSG(" [OPTIGOV] mali_freq  : %u\n", 
                    stats->mali_freq);
            KERNEL_DEBUG_MSG(" [OPTIGOV] mali_load  : %u\n", 
                    stats->mali_load);
            KERNEL_DEBUG_MSG(" [OPTIGOV] mali_n_load: %u\n", 
                    stats->mali_norm_load);
            KERNEL_DEBUG_MSG(" [OPTIGOV] big uW     : %i\n", 
                    stats->ina_big_uW);
            KERNEL_DEBUG_MSG(" [OPTIGOV] little uW  : %i\n", 
                    stats->ina_little_uW);
            KERNEL_DEBUG_MSG(" [OPTIGOV] mem uW     : %i\n",
                    stats->ina_mem_uW);
            KERNEL_DEBUG_MSG(" [OPTIGOV] gpu uW     : %i\n",
                    stats->ina_gpu_uW);
            return 0;
        }
        KERNEL_DEBUG_MSG(" [OPTIGOV] debug stats not init'd\n");
        return -EAGAIN;
    }
    KERNEL_DEBUG_MSG(" [OPTIGOV] debug stats inval\n");
    return -EINVAL;
}

