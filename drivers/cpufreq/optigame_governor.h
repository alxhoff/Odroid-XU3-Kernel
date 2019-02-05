#ifndef OPTIGAME_GOVERNOR_H
#define OPTIGAME_GOVERNOR_H

#include <linux/timer.h>
#include <linux/rwsem.h>

struct cpufreq_optigame_governor_coreinfo {
	struct timer_list cpu_timer;
	struct timer_list cpu_slack_timer;
	spinlock_t load_lock; /* protects the next 4 fields */
	u64 time_in_idle;
	u64 time_in_idle_timestamp;
	u64 cputime_speedadj;
	u64 cputime_speedadj_timestamp;
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *freq_table;
	unsigned int target_freq;
	unsigned int floor_freq;
	u64 floor_validate_time;
	u64 hispeed_validate_time;
	struct rw_semaphore enable_sem;
	int governor_enabled;
};

struct cpufreq_optigame_governor_tunables {

    int usage_count;

    //target cpu loads
    
	spinlock_t target_loads_lock;
	unsigned int *target_loads;
    
	/*
	 * The sample rate of the timer used to increase frequency
	 */
	unsigned long timer_rate;
	
    bool io_is_busy;

#define TASK_NAME_LEN 15
	/* realtime thread handles frequency scaling */
	struct task_struct *speedchange_task;
	
    /* handle for get cpufreq_policy */
	unsigned int *policy;

    //TODO populate

};

struct og_gov {
    struct attribute_group *attr_group; 

    struct kobject*     kobj; //sysfs top level object
    struct completion*  kobject_unregister;
};

#endif 
