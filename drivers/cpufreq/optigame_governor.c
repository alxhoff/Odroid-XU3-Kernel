
#include <linux/kobject.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/percpu-defs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/of_i2c.h>
#include <linux/tick.h>
#include <linux/types.h>
#include <linux/cpu.h>

#include <linux/cpumask.h>
#include <linux/moduleparam.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <asm/cputime.h>

#ifdef CONFIG_ANDROID
#include <asm/uaccess.h>
#include <linux/syscalls.h>
#include <linux/android_aid.h>
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
#include <mach/cpufreq.h>
#endif
#include "cpu_load_metric.h"

#include "chrome_governor_kernel_write.h"
#include "optigame_governor.h"
#include "optigame_governor_sched.h"
#include "optigame_governor_config.h"
#include "optigame_governor_stats.h"

//PROTOS
static int cpufreq_optigame_governor_speedchange_task(void *data);
static void cpufreq_optigame_governor_timer_start(
    struct cpufreq_optigame_governor_tunables *tunables, int cpu);
//

DEFINE_PER_CPU(struct cpufreq_optigame_governor_coreinfo, og_coreinfo);

#define KERNEL_DEBUG_MSG(...) \
            do { printk(KERN_INFO __VA_ARGS__); } while (0)

#define DEFAULT_TIMER_RATE (20 * USEC_PER_MSEC)
#define DEFAULT_TARGET_LOAD 90
static unsigned int default_target_loads_og[] = { DEFAULT_TARGET_LOAD };

static cpumask_t speedchange_cpumask_og;

//LOCKING
static spinlock_t speedchange_cpumask_lock_og;
static struct mutex gov_lock_og;

//GOVERNOR STRUCTS
struct og_gov *og_gov_objects = NULL;
struct cpufreq_optigame_governor_tunables *common_tunables_og = NULL;
static struct cpufreq_optigame_governor_tunables *tuned_parameters_og = NULL;
struct cpufreq_optigame_governor_cpuinfo og_cpuinfo = {
    .BIG_state = true,
    .shutdown_cpu = CPU_NONE,
    .stats = {0},
};

static bool gov_started = 0;

//SYSFS
   /*
    * Create show/store routines
    * - sys: One gov instance for complete SYSTEM
    * - pol: One gov instance per struct cpufreq_policy
    */
#define show_gov_pol_sys(file_name)                 \
   static ssize_t show_##file_name##_gov_sys               \
   (struct kobject *kobj, struct attribute *attr, char *buf)       \
   {                                   \
       return show_##file_name(common_tunables_og, buf);           \
   }                                   \
   
   
#define store_gov_pol_sys(file_name)                    \
   static ssize_t store_##file_name##_gov_sys              \
   (struct kobject *kobj, struct attribute *attr, const char *buf,     \
       size_t count)                           \
   {                                   \
       return store_##file_name(common_tunables_og, buf, count);       \
   }                                   \
   
#define show_store_gov_pol_sys(file_name)               \
   show_gov_pol_sys(file_name);                        \
   store_gov_pol_sys(file_name)
   
#define gov_sys_attr_rw(_name)                      \
   static struct global_attr _name##_gov_sys =             \
   __ATTR(_name, 0644, show_##_name##_gov_sys, store_##_name##_gov_sys)
#define gov_pol_attr_rw(_name)                      \
   static struct freq_attr _name##_gov_pol =               \
   __ATTR(_name, 0644, show_##_name##_gov_pol, store_##_name##_gov_pol)
#define gov_sys_pol_attr_rw(_name)                  \
   gov_sys_attr_rw(_name); \

#define prepare_attribute(_name)                    \
    show_store_gov_pol_sys(_name)\
    gov_sys_pol_attr_rw(_name)

//access functions and attributes
static ssize_t show_timer_rate(
		struct cpufreq_optigame_governor_tunables *tunables, char *buf) {
	return sprintf(buf, "%lu\n", tunables->timer_rate);
}

static ssize_t store_timer_rate(
		struct cpufreq_optigame_governor_tunables *tunables, const char *buf,
		size_t count) {
	int ret;
	unsigned long val;

	ret = strict_strtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->timer_rate = val;
	return count;
}

prepare_attribute(timer_rate)
;

static ssize_t show_io_is_busy(
		struct cpufreq_optigame_governor_tunables *tunables, char *buf) {
	return sprintf(buf, "%u\n", tunables->io_is_busy);
}

static ssize_t store_io_is_busy(
		struct cpufreq_optigame_governor_tunables *tunables, const char *buf,
		size_t count) {
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->io_is_busy = val;
	return count;
}

prepare_attribute(io_is_busy)
;

static ssize_t show_DT_get_string_prop(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    struct device_node *np;
    np = of_find_node_by_name(NULL, "cpufreq");

    const char* ps;
    of_property_read_string(np, "cpufreq,teststring", &ps);

    return sprintf(buf, "%s\n", ps);
}

static ssize_t show_DT_get_a15_ina_driver_name(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    struct device_node *np;
    np = of_find_node_by_name(NULL, "cpufreq");
   
    struct device_node *ina;
    ina = of_parse_phandle(np, "cpufreq,a15_ina", 0);

    //get i2c client from node
    struct i2c_client *cl;
    cl = of_find_i2c_device_by_node(ina);

    //get driver name from client
    //i2c_client->driver -> i2c_driver
    //i2c_driver.driver -> device_driver
    return sprintf(buf, "%s\n", cl->driver->driver.name);
}

static DEVICE_ATTR(DT_get_string_prop, S_IRWXUGO, show_DT_get_string_prop, NULL);
static DEVICE_ATTR(DT_get_a15_ina_driver_name, S_IRWXUGO, show_DT_get_a15_ina_driver_name, NULL);

static struct attribute *optigame_governor_sysfs_attributes[] = {
    &dev_attr_DT_get_string_prop,
    &dev_attr_DT_get_a15_ina_driver_name,
    &timer_rate_gov_sys.attr,
    &io_is_busy_gov_sys.attr,
    //TODO ADD FSM HERE
    NULL,
};

static struct attribute_group optigame_governor_sysfs_attribute_group = {
    .attrs = optigame_governor_sysfs_attributes,
    .name = NULL,
};

const char *optigame_governor_sysfs[] = {
    "timer_rate",
    "io_is_busy"
};

#ifdef CONFIG_ANDROID
static void change_sysfs_owner(struct cpufreq_policy *policy)
{
	char buf[NAME_MAX];
	mm_segment_t oldfs;
	int i;
	char *path = kobject_get_path(cpufreq_global_kobject,
			GFP_KERNEL);

	oldfs = get_fs();
	set_fs(get_ds());

	for (i = 0; i < ARRAY_SIZE(optigame_governor_sysfs); i++) {
		snprintf(buf, sizeof(buf), "/sys%s/og_governor/%s", path,
				optigame_governor_sysfs[i]);
		sys_chown(buf, AID_SYSTEM, AID_SYSTEM);
	}

	set_fs(oldfs);
	kfree(path);
}
#else
static inline void change_sysfs_owner(struct cpufreq_policy *policy) {
}
#endif

static struct attribute_group *get_sysfs_attr(void){
    return &optigame_governor_sysfs_attribute_group;
}

static int cpufreq_governor_optigame_sysfs_init(void){
    int error = 0;
            KERNEL_DEBUG_MSG(" [OPTIGOV] sysfs_init:0\n");
    if(!og_gov_objects) return -EINVAL;

    //sysfs
    if(!og_gov_objects->kobj){
            KERNEL_DEBUG_MSG(" [OPTIGOV] sysfs_init:1\n");
            KERNEL_DEBUG_MSG(" [OPTIGOV] sysfs_init:1\n");
        og_gov_objects->kobj = 
            kobject_create_and_add("OG_governor", cpufreq_global_kobject);
        if(!og_gov_objects->kobj){
            KERNEL_DEBUG_MSG(" [OPTIGOV] sysfs_init:1.1\n");
            pr_err("%s: SYSFS_INIT: gov kobj kzalloc failed\n", 
                    __func__);
            return -ENOMEM;
        }
    }else{
        //TODO check and potentially release/reinit kobject
    }

    //attributes
    if(!og_gov_objects->attr_group) {
            KERNEL_DEBUG_MSG(" [OPTIGOV] sysfs_init:2\n");
        //og_gov_objects->attr_group = get_sysfs_attr();
        og_gov_objects->attr_group = &optigame_governor_sysfs_attribute_group;
    }

    //top level sysfs attributes
    error = sysfs_create_group(og_gov_objects->kobj, 
            og_gov_objects->attr_group);
    if(error){
            KERNEL_DEBUG_MSG(" [OPTIGOV] sysfs_init:3\n");
        pr_err("%s: SYSFS_INIT: og gov attr group create failed\n", 
            __func__);
        kfree(common_tunables_og);
        kobject_put(og_gov_objects->kobj);
        return error;
    }
            KERNEL_DEBUG_MSG(" [OPTIGOV] sysfs_init:4\n");
    return 0;
}

/*********************************************************
 ***************************CORE**************************
 ********************************************************/

static int cpufreq_governor_optigame_events(struct cpufreq_policy *policy,
        unsigned int event) {
    int error = 0;
	struct cpufreq_optigame_governor_coreinfo *pcpu = 0;
	struct cpufreq_optigame_governor_tunables *tunables = common_tunables_og;
    struct cpufreq_frequency_table *freq_table;
	char speedchange_task_name[TASK_NAME_LEN];
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

    //catch unsupported CPUS
    if(policy->cpu > BIGC3){ 
        KERNEL_DEBUG_MSG(" [OPTIGOV] CPU larger than BIGC3\n");
        return -EINVAL;
    }
    if(policy->cpu > littleC3 && og_cpuinfo.BIG_state == false){
        KERNEL_DEBUG_MSG(" [OPTIGOV] BIG cpu even while BIG is off\n");
        return -EINVAL;
    }
		
    KERNEL_DEBUG_MSG(
		" [GOVERNOR] optigame_Governor:  IN EVENT %d\n", event);
    switch(event){
        case CPUFREQ_GOV_POLICY_INIT:{
            if(!og_gov_objects){
                og_gov_objects = 
                    (struct og_gov*) kzalloc(sizeof(struct og_gov), GFP_KERNEL);
                if(!og_gov_objects){
                    pr_err("%s: GOV_OBJECT_INIT: kzalloc failed\n", __func__);
                    return -ENOMEM;
                }   
            }

            if(!og_cpuinfo.stats.initd){ /** Get ina sensor handles from DT */
                optigame_stats_init(&og_cpuinfo.stats);
            }

		    if (common_tunables_og && og_sched_getManagedCores() != 0) {
                //tunables already initialized
                tunables->usage_count++;
                //load tunables into policy's governor's data
                policy->governor_data = common_tunables_og;
                og_sched_addCoresToManaged(0x0F << (policy->cpu / 4) * 4);
                return 0;
            }
            if(og_sched_getManagedCores() == 0){
                og_sched_addCoresToManaged(0x0F << (policy->cpu/4 ) * 4);
                tunables = (struct cpufreq_optigame_governor_tunables*)
                    kzalloc(sizeof(struct cpufreq_optigame_governor_tunables), GFP_KERNEL);
                if(!tunables){
                    pr_err("%s: POLICY_INIT: kzalloc failed\n", __func__);
                    return -ENOMEM;
                }
                //previous governor tunables
                if(!tuned_parameters_og){ //no previous tunables, load default
                    tunables->target_loads = default_target_loads_og;
                    tunables->timer_rate = DEFAULT_TIMER_RATE;
                }else{ 
                    memcpy(tunables, tuned_parameters_og, sizeof(*tunables));
                    kfree(tuned_parameters_og);
                }
                tunables->usage_count = 1;
                //TODO understand this VVVV
                tunables->policy = &policy->policy; //update handle for get cpufreq_policy 
                common_tunables_og = tunables;
                
                spin_lock_init(&tunables->target_loads_lock);
                
                //sysfs
                if(og_gov_objects) {
                    int error = 0;
                    if(!og_gov_objects) return -EINVAL;

                    //sysfs
                    if(!og_gov_objects->kobj){
                        og_gov_objects->kobj = 
                            kobject_create_and_add("OG_governor", cpufreq_global_kobject);
                        if(!og_gov_objects->kobj){
                            pr_err("%s: SYSFS_INIT: gov kobj kzalloc failed\n", __func__);
                            return -ENOMEM;
                        }
                    }else{
                    //TODO check and potentially release/reinit kobject
                    }

                    //attributes
                    if(!og_gov_objects->attr_group) {
                        KERNEL_DEBUG_MSG(" [OPTIGOV] sysfs_init:2\n");
                        og_gov_objects->attr_group = 
                            &optigame_governor_sysfs_attribute_group;
                    }

                    //top level sysfs attributes
                    error = sysfs_create_group(og_gov_objects->kobj, 
                        og_gov_objects->attr_group);
                    if(error){
                        pr_err("%s: SYSFS_INIT: og gov attr group create failed\n", __func__);
                        kfree(tunables);
                        kobject_put(og_gov_objects->kobj);
                        return error;
                    }
                       
                    change_sysfs_owner(policy); 
                }

                policy->governor_data = tunables;
            }
        }
            break;
        case CPUFREQ_GOV_START:{
            unsigned int j;
            
            mutex_lock(&gov_lock_og); //take control of gov
		    freq_table = cpufreq_frequency_get_table(policy->cpu);

		    for_each_cpu(j, policy->cpus) {
			    pcpu = &per_cpu(og_coreinfo, j);        //get cpu info for cpu
			    pcpu->policy = policy;              //set cpu policy from passed arg
			    pcpu->target_freq = policy->cur;    //set target freq to current freq 
			    pcpu->freq_table = freq_table;      //save freq table
			    pcpu->floor_freq = pcpu->target_freq;//min freq for cpu 
			    down_write(&pcpu->enable_sem); 
			    pcpu->governor_enabled = 1;
			    up_write(&pcpu->enable_sem);
		    }
            
            if(og_sched_getManagedCores() == CPU_BIGlittle && tunables && gov_started == 0) {
                
                //task set up
                snprintf(speedchange_task_name, TASK_NAME_LEN,
                        "optigame_governor%d\n", policy->cpu); 
                               
                tunables->speedchange_task = kthread_create( //create task
                        cpufreq_optigame_governor_speedchange_task, NULL,
                        speedchange_task_name);
                if(IS_ERR(tunables->speedchange_task)){
                    mutex_unlock(&gov_lock_og);
                    return PTR_ERR(tunables->speedchange_task);
                }
			
                sched_setscheduler_nocheck(tunables->speedchange_task,
                    SCHED_FIFO,	&param);    //set scheduling policy 

			    get_task_struct(common_tunables_og->speedchange_task); //increment usage 
			    og_sched_setAffinity(tunables->speedchange_task, CPU_little);
            
                wake_up_process(tunables->speedchange_task); //run process
                
                down_write(&pcpu->enable_sem);
                cpufreq_optigame_governor_timer_start(tunables, 0); //breaking kernel
                up_write(&pcpu->enable_sem);

                gov_started = 1;
            } 
            mutex_unlock(&gov_lock_og);
            
        }
            break;
        case CPUFREQ_GOV_LIMITS:
             
            break;
        case CPUFREQ_GOV_STOP:{
	        unsigned int j;	

            og_sched_removeCoresFromManaged(0x0F << (policy->cpu / 4) * 4);
            
            mutex_lock(&gov_lock_og);
            for_each_cpu(j, policy->cpus){
                pcpu = &per_cpu(og_coreinfo, j);
                down_write(&pcpu->enable_sem);
                pcpu->governor_enabled = 0;
                up_write(&pcpu->enable_sem);
            }

            mutex_unlock(&gov_lock_og);
            
            //TODO little core clean up
		    // only clean up if CPU is A7
		    if (policy->cpu == littleC0) {
			    mutex_lock(&gov_lock_og);

			    for_each_cpu(j, policy->cpus) {
				    pcpu = &per_cpu(og_coreinfo, j);
				    down_write(&pcpu->enable_sem);
				    // we have initialized timers for all cores, so we have to delete them all
				    del_timer_sync(&pcpu->cpu_timer);
				    up_write(&pcpu->enable_sem);
			    }
			
                // Make sure that the task is stopped only once.
			    // Remember: We have started only one task.
			    if (j == 0 && tunables && tunables->speedchange_task) {
				    kthread_stop(tunables->speedchange_task);
				    put_task_struct(tunables->speedchange_task);
				    tunables->speedchange_task = NULL;
			    }
			
                mutex_unlock(&gov_lock_og);

			    //if (og_sched_getManagedCores() == 0){
				    //chrome_governor_ioctl_exit();
				    //og_touch_ioctl_exit();
			    //}
            }
        }
            break;
        case CPUFREQ_GOV_POLICY_EXIT:
		    // only clean up if CPU is little 
		    if (policy->cpu == littleC0) {
			    KERNEL_DEBUG_MSG("Entering exit routine, usage count: %d\n",
					tunables->usage_count);
			    if (og_sched_getManagedCores() == 0) {
				    if (policy->governor->initialized == 1) {
					    //unregister_hotcpu_notifier(&cpufreq_notifier_block);
					    //				idle_notifier_unregister(&cpufreq_interactive_idle_nb);
				    }

                    //TODO sysfs clean up
                    
				    tuned_parameters_og = kzalloc(sizeof(*tunables), GFP_KERNEL);
				    if (!tuned_parameters_og) {
					    pr_err("%s: POLICY_EXIT: kzalloc failed\n", __func__);
					    return -ENOMEM;
				    }
				    memcpy(tuned_parameters_og, tunables, sizeof(*tunables));
				    kfree(tunables);
				    common_tunables_og = NULL;
				//				KERNEL_DEBUG_MSG("Cleaned up all data\n");
			    }

			    policy->governor_data = NULL;
		    }
            break;
        default:
            break;
    }
    return 0;
}

/*********************************************************
 **************************TIMER**************************
 ********************************************************/

/**
 * Re-arm the timer once it is expired.
 * Also re-arm the timer in case it is still running.
 * This is useful when the system is e.g. in scrolling state where
 * we have timer and ioctl calls simultaneously.
 *
 * @param data - required parameter, not used
 **/
void cpufreq_optigame_governor_timer_resched(unsigned long expires) {
	unsigned long flags;
	// load cpu 0
	struct cpufreq_optigame_governor_coreinfo *pcpu = &per_cpu(og_coreinfo, 0);
	struct cpufreq_optigame_governor_tunables *tunables =
			pcpu->policy->governor_data;

	// rearm the timer
	if (!tunables->speedchange_task)
		return;

	if (!timer_pending(&pcpu->cpu_timer)) {
		spin_lock_irqsave(&pcpu->load_lock, flags);
		expires = jiffies + usecs_to_jiffies(tunables->timer_rate);
		mod_timer_pinned(&pcpu->cpu_timer, expires); //update active timer expiry
		spin_unlock_irqrestore(&pcpu->load_lock, flags);
	} else {
		spin_lock_irqsave(&pcpu->load_lock, flags);
		expires = jiffies + usecs_to_jiffies(tunables->timer_rate);
		mod_timer_pending(&pcpu->cpu_timer, expires); //update inactive timer expiry without reactivating
		spin_unlock_irqrestore(&pcpu->load_lock, flags);
	}
}

/**
 * Timer which is called when power management should be
 * re-evaluated
 *
 * @param data - required parameter, not used
 **/
void cpufreq_optigame_governor_timer(unsigned long data) {
	struct cpufreq_optigame_governor_coreinfo *pcpu = &per_cpu(og_coreinfo, data);
	struct cpufreq_optigame_governor_tunables *tunables =
			pcpu->policy->governor_data;

//	KERNEL_DEBUG_MSG(" [GOVERNOR] Timer expired\n");

	if (!down_read_trylock(&pcpu->enable_sem))      //take enable semaphore
		return;
	if (!pcpu->governor_enabled)
		goto exit;

//	KERNEL_DEBUG_MSG(" [GOVERNOR] Timer expired\n");
    wake_up_process(tunables->speedchange_task);    //wake up task to execute once

	cpufreq_optigame_governor_timer_resched(tunables->timer_rate);
    exit: up_read(&pcpu->enable_sem);               //release enable semaphore
	return;
}

/**
 * Sets timer's expiry to current time plus one period interval then starts the
 * timer of the target CPU
 **/
static void cpufreq_optigame_governor_timer_start(
		struct cpufreq_optigame_governor_tunables *tunables, int cpu) {
	
    struct cpufreq_optigame_governor_coreinfo *pcpu = &per_cpu(og_coreinfo, cpu); //timer handle
	unsigned long expires = jiffies + usecs_to_jiffies(tunables->timer_rate); //update timer expiry

	if (!tunables->speedchange_task) {
		KERNEL_DEBUG_MSG(
				" [GOVERNOR] optigame_Governor: No tunables->speedchange task \n");
		return;
	}

	pcpu->cpu_timer.expires = expires;
    //TODO check VV
    add_timer_on(&pcpu->cpu_timer, cpu); //add timer to each cpu
}

/*********************************************************
 **************************TASK***************************
 ********************************************************/

void og_pm_coordinator(void) 
{

}

static int cpufreq_optigame_governor_speedchange_task(void *data) {
	//cpumask_t tmp_mask;
	unsigned long flags, init = 0;

    unsigned int test_util = 0, test_freq = 0;

	while (!kthread_should_stop()) { //if task should run, deinit handled in gov stop
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&speedchange_cpumask_lock_og, flags); //obtain task lock

		if (cpumask_empty(&speedchange_cpumask_og)) {
			spin_unlock_irqrestore(&speedchange_cpumask_lock_og, flags);
			schedule();         //request scheduler to run

			if (kthread_should_stop())
				break;

			spin_lock_irqsave(&speedchange_cpumask_lock_og, flags); //take
		}
		//KERNEL_DEBUG_MSG(" [GOVERNOR] Optigame_Governor: in speed change task");
		set_current_state(TASK_RUNNING);
		//tmp_mask = speedchange_cpumask_og;
		cpumask_clear(&speedchange_cpumask_og);
		spin_unlock_irqrestore(&speedchange_cpumask_lock_og, flags); //restore

		// perform power management
        // get stats
        optigame_stats_get(&og_cpuinfo.stats);
        optigame_stats_debug_print(&og_cpuinfo.stats);

        //coordinate ;)
		og_pm_coordinator();
		cpufreq_optigame_governor_timer_resched(common_tunables_og->timer_rate);
	}

	return 0;
}

/*********************************************************
 ***********************MODULE CORE***********************
 ********************************************************/

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_OPTIGAME
static
#endif
struct cpufreq_governor cpufreq_gov_optigame = { .name = "og-governor",
		.governor = cpufreq_governor_optigame_events, .owner = THIS_MODULE };


static int __init cpufreq_gov_optigame_init(void)
{
	unsigned int i;
	struct cpufreq_optigame_governor_coreinfo *pcpu;

	KERNEL_DEBUG_MSG(" [GOVERNOR] Optigame_Governor: entering init routine\n");

	/* Initalize per-cpu timers */
	// do we really need per-core timers?
	// The function iterates through all eight cores
	for_each_possible_cpu(i) {
		pcpu = &per_cpu(og_coreinfo, i);
        //TODO should this be deferrable?
		init_timer_deferrable(&pcpu->cpu_timer);
		pcpu->cpu_timer.function = cpufreq_optigame_governor_timer;
		pcpu->cpu_timer.data = i;
		//		init_timer(&pcpu->cpu_slack_timer);
		spin_lock_init(&pcpu->load_lock);
		init_rwsem(&pcpu->enable_sem);
		KERNEL_DEBUG_MSG(" [GOVERNOR] Optigame_Governor: init, show i: %d \n", i);
	}

	spin_lock_init(&speedchange_cpumask_lock_og);
	mutex_init(&gov_lock_og);

	return cpufreq_register_governor(&cpufreq_gov_optigame);
}

static void __exit cpufreq_gov_optigame_exit(void)
{
	//og_touch_unregister_notify(&og_touch_nb);
	cpufreq_unregister_governor(&cpufreq_gov_optigame);
}

MODULE_AUTHOR("Alex Hoffman <alex.hoffman@tum.de>");
MODULE_DESCRIPTION("CPUfreq governor 'optigame-governor'");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_OPTIGAME
fs_initcall(cpufreq_gov_optigame_init);
#else
module_init( cpufreq_gov_optigame_init);
#endif
module_exit( cpufreq_gov_optigame_exit);
