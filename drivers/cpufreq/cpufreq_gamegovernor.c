/*
 *  linux/drivers/cpufreq/cpufreq_gamegovernor.c
 *
 *  Copyright (C) 2015 - 2016 Dominik Füß <dominik.fuess@tum.de>
 *  Copyright (C) 2019 Philipp van Kempen <philipp.van-kempen@tum.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/*
 * Switch for enabling detailed debugging
 */
#define DO_DEBUG

/*
 * Causes all strings printed from the file to have the module name prepended
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

/*
 * #include-Block
 */
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/init.h>

#include <linux/fs.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/unistd.h>

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/random.h>
#include <linux/slab.h>

/*
 * GameGovernor-related includes
 */
#include <gamegovernor/cpufreq_gamegovernor_includes.h>
#include <gamegovernor/cpufreq_gamegovernor.h>


/*
 * Global variables for character file
 */
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;

/*
 * CPU accounting variables
 */
static short number_cpus=0; // between 0 and 2
static int a7ismanaged=0;
static int a15ismanaged=0;

/*
 * Temporary variables
 */
#ifdef DO_LOGGING
int64_t nr_write_threads=0;  // For Governor Log
#endif // DO_LOGGING
#ifdef THREAD_NAME_LOGGING
int64_t nr_write_threads2=0; // For Thread Name Log
#endif // THREAD_NAME_LOGGING

#ifdef DO_DEBUG
/*
 * Count function calls and number threads
 * (needed for debugging)
 */
static int thread_count=0;
#endif // DO_DEBUG
static int number_allocation_chances=0;

/*
 * Mutex definitions
 * (Mostly preventing kernel panics...)
 */
static DEFINE_MUTEX(hotplug_mutex);
#ifdef DO_LOGGING
DEFINE_MUTEX(logfile_mutex);
DEFINE_MUTEX(logstruct_mutex);
#endif // DO_LOGGING
#ifdef THREAD_NAME_LOGGING
DEFINE_MUTEX(logfile_thread_name_mutex);
DEFINE_MUTEX(logstruct_thread_name_mutex);
#endif // THREAD_NAME_LOGGING

/*
 * Variables for task allocation
 */
unsigned int a15frequency=1200e3; // current frequency (in kHz!)
unsigned int a7frequency=1000e3;  //        ""
int64_t a7space[4]={0, 0, 0, 0};  // space left (in cpu cycles) per core
int64_t a15space[4]={0, 0, 0, 0}; //                ""
const int64_t a7available_frequencies[5]={  // available frequencies (in MHz!)
    1000, 1100, 1200, 1300, 1400
};
const int64_t a15available_frequencies[9]={ //            ""
    1200, 1300, 1400, 1500,  1600, 1700, 1800, 1900, 2000
};
short curr_frequ_a7_nr;  // Number of target frequency (range: 0-4)
                         // 'real' frequency -> a7available_frequencies[curr_frequ_a7_nr]
short curr_frequ_a15_nr; //nr of target frequency (range: 0-8)
                         // 'real' frequency -> a15available_frequencies[curr_frequ_a15_nr]
int64_t a7_space_increase_per_frequency=0;  // As frequency always increases in steps of 0.1 GHz
                                            // the space increase (in cycles) is constant and
                                            // can be computed once to save time
int64_t a15_space_increase_per_frequency=0; //               ""

/*
 * A15 hotplugging related variables
 */
int nr_tasks_a15; // counting assigned threads
short shut_down_core=0; // =1 during shutdown and boot of A15
short a15_core_online[4]={1}; // per core state
short A15_online=1; // start with big cores on
int shutdown_counter_a15=0; // reduce hotplugging overhead

/*
 * Logging Variables
 */

#ifdef DO_LOGGING
struct file *fp_loggin_file=NULL;
short logging_file_open=0;
log_struct log_str= { // //struct to be written in the logfile
    .nr=0,
    .a7_freq=0,
    .a15_freq=0,
    .frame_rate=0,
    .time_in=0,
    .fr_error=0,
    .cpu4_online=7,
    .cpu5_online=7,
    .cpu6_online=7,
    .cpu7_online=7,
};
#endif // DO_LOGGING

#ifdef THREAD_NAME_LOGGING
struct file *fp_thread_name_logging=NULL;
short logging_file_thread_name_open=0;
#endif // THREAD_NAME_LOGGING
/*
 * Initialize Timer
 */
static timer_struct autocorr_timer = {
    .time=0,               // time of the last update
    .update_interval=10e9, // update every X ns
    .timer_expired=1       // trigger variable
};

//---------------------------------------------------------------------
// CPUFreq - Functions
//---------------------------------------------------------------------

/*
 * Handle Governor Events
 * (mostly start and stop)
 */
static int cpufreq_governor_gamegovernor(struct cpufreq_policy *policy, unsigned int event)
{
#if defined DO_LOGGING || defined THREAD_NAME_LOGGING
    mm_segment_t old_fs; // file system used for logfiles
#endif // DO_LOGGING || THREAD_NAME_LOGGING

    switch (event) {
        // START event called per CPU
        case CPUFREQ_GOV_START:
            KERNEL_DEBUG_MSG("GOV|DEBUG: START Event called for CPU: %d, CPU online: %d\n", policy->cpu, cpu_online(policy->cpu));

            /*
             * shut_down_core is set if the cores are shutdown or rebooted. This causes a call of the
             * governor function with the start and stop events.
             */
            if (shut_down_core==1) {
                if (policy->cpu==4) {
                    number_cpus++;
                }
                break; // ignore ismanaged
            }

            /*
             * Set the is managed flag for the respective cpu
             */
            if (policy->cpu==0 || policy->cpu==1 || policy->cpu==2 || policy->cpu==3) {
                a7ismanaged=1;
            }
            else if (policy->cpu==4 || policy->cpu==5 || policy->cpu==6 || policy->cpu==7) {
                a15ismanaged=1;
            }
            else {
                KERNEL_ERROR_MSG("GOV|ERROR: Unknown CPU-Nr.: %u \n", policy->cpu);
            }

            /*
             * Increment number of used CPUs
             */
            number_cpus++;

            /*
             * Initialize char device during the first call
             */
            if (number_cpus==1) {
                if (IoctlInit()!=0) {
                    KERNEL_ERROR_MSG("GOV|ERROR: could not initialize char device\n");
                }
                else {
                    KERNEL_DEBUG_MSG("GOV|DEBUG: char device, successfully initialized\n");
                }
            }

#ifdef DO_LOGGING
            /*
             * Open logfile
             */
            old_fs = get_fs();
            set_fs(KERNEL_DS);

            mutex_lock(&logfile_mutex);
            if (logging_file_open==0) { // Init file
                fp_loggin_file = filp_open(LOGFILE, (O_CREAT | O_TRUNC | O_WRONLY), (S_IRWXU | S_IRWXG | S_IRWXO));
            }
            mutex_unlock(&logfile_mutex);

            if (fp_loggin_file == NULL) { // Error handling
                KERNEL_ERROR_MSG("GOV|ERROR: Can't open Logging File\n");
            }
            else {
                if (logging_file_open==0) {
                    KERNEL_DEBUG_MSG("GOV|DEBUG: Opened Logging File\n");
                    logging_file_open=1;
                }
            }

            set_fs(old_fs);
#endif

#ifdef THREAD_NAME_LOGGING
            /*
             * Open logfile
             */
            old_fs=get_fs();
            set_fs(KERNEL_DS);

            mutex_lock(&logfile_thread_name_mutex);
            if (logging_file_thread_name_open==0) { // Init File
                fp_thread_name_logging=filp_open(LOGFILE_THREAD_NAME, (O_CREAT | O_TRUNC | O_WRONLY), (S_IRWXU | S_IRWXG | S_IRWXO));
            }
            mutex_unlock(&logfile_thread_name_mutex);

            if (fp_thread_name_logging==NULL) { // Error Handling
                KERNEL_ERROR_MSG("GOV|ERROR: Can't open Thread Name Logging File\n");
            }
            else {
                if (logging_file_thread_name_open==0) {
                    KERNEL_DEBUG_MSG("GOV|DEBUG: Opened Thread Name Logging File\n");
                    logging_file_thread_name_open=1;
                }
            }

            set_fs(old_fs);
#endif

            /*
             * Start with minimum frequency
             */
            KERNEL_DEBUG_MSG("GOV|DEBUG: Setting to %u kHz because of event %u\n", policy->min, event);
            __cpufreq_driver_target(policy, policy->min, CPUFREQ_RELATION_L);

            KERNEL_VERBOSE_MSG("GOV|VERBOSE: GameGovernor started\n");

            break;
        // LIMITS Event
        case CPUFREQ_GOV_LIMITS:
            break; // can be ignored

        // STOP Event called per CPU
        case CPUFREQ_GOV_STOP:

            KERNEL_DEBUG_MSG("GOV|DEBUG: STOP Event called for CPU: %d, CPU online: %d\n", policy->cpu, cpu_online(policy->cpu));


            /*
             * Reset ismanaged flag
             */
            if ((policy->cpu==0 || policy->cpu==1 || policy->cpu==2 || policy->cpu==3) && cpu_online(policy->cpu) && shut_down_core == 0) {
                a7ismanaged=0;
                number_cpus--; // Decrement because of shutdown
            }
            else if ((policy->cpu==4 || policy->cpu==5 || policy->cpu==6 || policy->cpu==7) && cpu_online(policy->cpu) && shut_down_core == 0) {
                a15ismanaged=0;
                // Error if core shutdown triggered externally
                KERNEL_ERROR_MSG("GOV|ERROR: A15 not managed\n");
                number_cpus--; // Decrement because of shutdown
            }
            else if (shut_down_core == 1) {
                KERNEL_DEBUG_MSG("GOV|DEBUG: STOP MSG because of core shutdown\n");
                if (policy->cpu==4) {
                    number_cpus--; // Decrement because of shutdown
                }
            }
            else { // should be never reached
                KERNEL_ERROR_MSG("GOV|ERROR: Unknown CPU-Nr.: %u\n", policy->cpu);
            }

            /*
             * Deinitialization of char device
             * (if no cpu is managed anymore)
             */
            if (number_cpus==0) {

                IoctlExit(); // close ioctl interface

#ifdef DO_LOGGING
                /*
                 * Close logfile (if open)
                 */
                if (logging_file_open && fp_loggin_file != NULL) {
                    old_fs = get_fs(); // File System
                    set_fs(KERNEL_DS);

                    mutex_lock(&logfile_mutex); // File
                    filp_close(fp_loggin_file, 0);
                    logging_file_open=0;
                    KERNEL_DEBUG_MSG("GOV|DEBUG:Closed Logging File\n");
                    mutex_unlock(&logfile_mutex);

                    set_fs(old_fs); // Done
                } else {
                    KERNEL_WARNING_MSG("GOV|WARNING:Logging File already closed\n");
                }
#endif

#ifdef THREAD_NAME_LOGGING
                /*
                 * Close logfile
                 */
                if (logging_file_thread_name_open && fp_thread_name_logging != NULL) {
                    old_fs = get_fs(); // File System
                    set_fs(KERNEL_DS);

                    mutex_lock(&logfile_thread_name_mutex); // File
                    filp_close(fp_thread_name_logging, 0);
                    logging_file_thread_name_open=0;
                    KERNEL_DEBUG_MSG("GOV|DEBUG:Closed Thread Name Logging File\n");
                    mutex_unlock(&logfile_thread_name_mutex);

                    set_fs(old_fs); // Done
                } else {
                    KERNEL_WARNING_MSG("GOV|WARNING:Thread Logging File already closed\n");
                }
#endif
            }

            KERNEL_VERBOSE_MSG("GOV|VERBOSE: Governor stopped");
            break;
        default:
            break;
    }

    return 0; // always return 0
}


//---------------------------------------------------------------------
// Module Setup
//---------------------------------------------------------------------

/*
 * Create Governor
 */
#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_GAMEGOVERNOR
static
#endif
struct cpufreq_governor cpufreq_gov_gamegovernor = {
    .name		= "gamegovernor",
    .governor	= cpufreq_governor_gamegovernor,
    .owner		= THIS_MODULE,
};

/*
 * Register and Unregister governor
 */
static int __init cpufreq_gov_gamegovernor_init(void)
{
    return cpufreq_register_governor(&cpufreq_gov_gamegovernor);
}
static void __exit cpufreq_gov_gamegovernor_exit(void)
{
    cpufreq_unregister_governor(&cpufreq_gov_gamegovernor);
}

/*
 * Module metadata
 */
MODULE_AUTHOR("Philipp van Kempen <philipp.van-kempen@tum.de>");
MODULE_DESCRIPTION("CPUfreq governor 'GameGovernor'");
MODULE_LICENSE("GPL");

/*
 * Module definitions
 */
#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_GAMEGOVERNOR
fs_initcall(cpufreq_gov_gamegovernor_init);
#else
module_init(cpufreq_gov_gamegovernor_init);
#endif
module_exit(cpufreq_gov_gamegovernor_exit);

//---------------------------------------------------------------------
// IOCTL - Functions 
//---------------------------------------------------------------------

/*
 * Called in response to an external IOCTL call from user space
 */
static long MyIOctl( struct file *File,unsigned int cmd, unsigned long arg )
{
    static short is_init=0; // will be set to 1 after initialization
    // task management variables
    struct task_struct* task, *group_leader_task, *task_buffer;
    // actual frame timing
    uint64_t frame_rate=0;
    static int64_t frame_rate_error=0;
    int64_t error_buff;
    static uint64_t time_buf=0; // keep frame timestamps between calls
    // refrerence frame timing
    static int64_t target_time=0;
    static int64_t target_time_upper_bound=0;
    static int64_t target_time_lower_bound=0;
    // core/processor variables
    struct cpufreq_policy *policy;
    int nr_tasks_on_cpu[8]={0};
    int64_t a7space_decrement=0;
    int64_t a15space_decrement=0;
    static int64_t a7space_init=0;
    static int64_t a15space_init=0;
    // ioctl argument struct
    ioctl_struct_new_frame ioctl_arg;
    // collision handling
    static short iciotl_new_frame_inuse=0;
    // temporary variables (TODO: rename)
    int i;
    int a;


    /*
     * Handle incoming events
     */
    switch (cmd) {
        // Ioctl command from opengl library
        // (A new frame is about to be processed)
        case IOCTL_CMD_NEW_FRAME:

            /*
             * Prevent overlaps in ioctl calls
             */
            if (iciotl_new_frame_inuse==1) {
                KERNEL_WARNING_MSG("GOV|WARNING: IOCTL overlap\n");
                if (copy_from_user(&ioctl_arg, (ioctl_struct_new_frame *)arg, sizeof(ioctl_struct_new_frame))) {
                    KERNEL_ERROR_MSG("GOV|ERROR: Copy from user failed\n");
                    return -EACCES;
                }
                time_buf=ioctl_arg.time; // update timestamp for frame rate
                break; // no processing as last call takes longer than expected
            }
            iciotl_new_frame_inuse=1; // block incoming calls

            /*
             * Copy arguments from user space
             */
            if (copy_from_user(&ioctl_arg, (ioctl_struct_new_frame *)arg, sizeof(ioctl_struct_new_frame))){
                KERNEL_ERROR_MSG("GOV|ERROR: Copy from user failed\n");
                return -EACCES;
            }

            /*
             * Calculate actual frame rate ( precision: 0.1 )
             * FPS*10=10*1e9/(time_now-time_last)
             */
            frame_rate = div64_u64(10*1e9, ioctl_arg.time-time_buf);
            if (frame_rate>1000) {
                /*
                 * Sometimes framerates over 100 do occur
                 * (mostly if game or level is loading)
                 */
                KERNEL_WARNING_MSG("GOV|WARNING: Invalid Frame Rate: %llu\n", frame_rate);
                frame_rate=1000; // handle too high fps like a lower framerate
                time_buf=ioctl_arg.time-div64_u64(10e9, 100);
            }

            /*
             * Calculate frame timing references once
             */
            if (target_time==0) {
                target_time=div_s64(1e9, TARGET_FRAME_RATE);
            }
            if (target_time_upper_bound==0) {
                target_time_upper_bound=div_s64(1e9, FRAME_RATE_UPPER_BOUND);
            }
            if (target_time_lower_bound==0) {
                target_time_lower_bound=div_s64(1e9, FRAME_RATE_LOWER_BOUND);
            }

            /*
             * Update Frame Rate Controller
             */
            if (is_init) {
                /*
                 * Frame Error (in ns)
                 * Formular:
                 *     error_buff=time_frame-time_sleep-time_target
                 *
                 * time_sleep is !=0 if frame rate limitation was active
                 */
                error_buff=(((int64_t)ioctl_arg.time)-time_buf)-(int64_t)ioctl_arg.sleep_time -target_time;

                /*
                 * Handle Frame Hits/Misses
                 */
                if (error_buff < target_time*2) { // If FPS > 7.5 (ignore frame rates below)
                    /*
                     * Deadline Miss
                     * With default settings: 7.5 < FPS < 27
                     */
                    if (error_buff>(target_time - target_time_upper_bound)) {
                        // Increase frame error
                        frame_rate_error+=div_s64(error_buff*AGGRESSIVENESS_FACTOR_UP, 100);
                    }
                    /*
                     * Deadline Hit but too early
                     * With default settings: FPS > 33
                     */
                    else if (error_buff < (target_time - target_time_lower_bound)) {
                        // Decrease frame error (as error_buff is negative)
                        frame_rate_error+=div_s64(error_buff*AGGRESSIVENESS_FACTOR_DOWN, 100);
                    }
                    /*
                     * Deadline Hit
                     * With default settings: 27 < FPS < 33
                     */
                    else {
                        // Decrease frame error by X percent
                        frame_rate_error=div_s64(frame_rate_error * (100 - FRAME_HIT_REDUCTION_FACTOR), 100);
                    }

                    /*
                     * Ensure Error Boundaries
                     */
                    if (frame_rate_error<0) {
                        frame_rate_error=0;
                    }
                    else if (frame_rate_error > target_time) {
                        frame_rate_error=target_time;
                    }

                    /*
                     * Define core space decrements
                     * Conversion between kHz and MHz with 1e3
                     */
                    a7space_decrement= div_s64(frame_rate_error*a7available_frequencies[0], 1e3);
                    a15space_decrement=div_s64(frame_rate_error*a15available_frequencies[0], 1e3);
                    a15space_decrement=convert_a15cpucycles_to_a7(a15space_decrement);
                }
            }

            time_buf=ioctl_arg.time; // save the last time the function was called

            /*
             * Reset global variable before allocation
             */
            number_allocation_chances=0;

            /*
             * Error handling
             */
            if (!(a7ismanaged==1 && a15ismanaged==1)) {
                KERNEL_ERROR_MSG("GOV|ERROR: Not all CPUs are managed: A7: %d, A15: %d \n", a7ismanaged, a15ismanaged);
                goto exit; // skip rest
            }

            /*
             * Update timer
             */
            update_timer(&autocorr_timer, ioctl_arg.time);

            /*
             * Init task allocation varibales
             * (start with minimum)
             */
            curr_frequ_a7_nr=0;
            curr_frequ_a15_nr=0;

            /*
             * Calculate inital core spaces only once
             * space_init[cycles] = freq_min[GHz] / traget_frame_rate[1/s]
             */
            if (a7space_init==0 || a15space_init==0) {
                a7space_init=div_s64(a7available_frequencies[0]*1e6, TARGET_FRAME_RATE);
                a15space_init=div_s64(a15available_frequencies[0]*1e6, TARGET_FRAME_RATE);
                /*
                 * Multiply with a constant factor as A15 cores have a higher IPC
                 */
                a15space_init=convert_a15cpucycles_to_a7(a15space_init);
            }

            /*
             * Compute space_increase_per frequency only once
             */
            if (a7_space_increase_per_frequency==0 || a15_space_increase_per_frequency==0 ) {
                a7_space_increase_per_frequency=div_s64(1e8, TARGET_FRAME_RATE) - div_s64(frame_rate_error, 10);
                // convert a15 cycles
                a15_space_increase_per_frequency=convert_a15cpucycles_to_a7(a7_space_increase_per_frequency);
            }

            /*
             * Init the four space-slots of both processors
             */
            for(i=0; i<4; i++){
                a7space[i]=a7space_init-a7space_decrement;
                a15space[i]=a15space_init-a15space_decrement;
            }

            /*
             * Reset
             */
            nr_tasks_a15=0;
            for(a=0;a<8; a++) {
                nr_tasks_on_cpu[a]=0;
            }

#ifdef DO_DEBUG
            if (!is_init) {
                char buf[TASK_COMM_LEN];
                get_task_comm(buf, current);
                KERNEL_ERROR_MSG("GOV|CURRENT: pid=%i, comm=%s",current->pid, buf);
            }
#endif // DO_DEBUG

            //find the froup leader task
            group_leader_task=current->group_leader;

#ifdef HANDLE_CURRENT
            set_user_nice(current, -19); // give current task a high priority
            process_task(current);
#endif // HANDLE_CURRENT

            /*
             * Iterate through all tasks (except group leader)
             */
            list_for_each_entry_safe(task, task_buffer ,&(group_leader_task->thread_group), thread_group) {
#ifdef HANDLE_CURRENT
                if (task->pid != current->pid) {
                    process_task(task);
                    // update number of threads on CPUs
                    nr_tasks_on_cpu[task->task_informations->allocated_core]+=1;
                }
#else
                process_task(task);
                // update number of threads on CPUs
                nr_tasks_on_cpu[task->task_informations->allocated_core]+=1;
#endif // HANDLE_CURRENT
            }

            /*
             * Process group leader separately as it will
             * not appear in the list_for_each_entry_safe loop
             */
            process_task(group_leader_task);
            nr_tasks_on_cpu[group_leader_task->task_informations->allocated_core]+=1;


            /*
             * Disable A15 if no tasks are assigned to it
             */
            if (nr_tasks_a15==0 && (cpu_online(4) || cpu_online(5) || cpu_online(6) || cpu_online(7))) {
                // shutting down cores takes some time
                //  -> start a new kernel thread
                kthread_run(&shutdown_a15, NULL, "shutdown_thread");
            }

            /*
             * Set the cpu frequencies
             * (in kHz)
             * WARNING:
             *  - always use cpufreq_cpu_put after
             *    cpufreq_cpu_get to release resources
             *  - cpufreq_cpu_get can return NULL
             *    -> handle kernel panic
             */
            policy=cpufreq_cpu_get(0); // A7
            if (policy) {
                cpufreq_set(policy, a7available_frequencies[curr_frequ_a7_nr]*1000);
                cpufreq_cpu_put(policy);
            } else {
                KERNEL_WARNING_MSG("GOV|WARNING: Policy for A7 not available!\n");
            }

            if (cpu_online(4) && cpu_online(5) && cpu_online(6) && cpu_online(7) && A15_online==1) {
                mutex_lock(&hotplug_mutex); // Make sure that A15 can not be turned off
                policy = cpufreq_cpu_get(4); // A15
                if (policy) {
                    cpufreq_set(policy, a15available_frequencies[curr_frequ_a15_nr]*1000);
                    cpufreq_cpu_put(policy);
                } else {
                    KERNEL_WARNING_MSG("GOV|WARNING: Policy for A15 not available!\n");
                }
                mutex_unlock(&hotplug_mutex);
            }

#ifdef DO_LOGGING
            /*
             * Copy data to log struct
             */
            mutex_lock(&logfile_mutex); // block file operations
            log_str.nr++;                      // incrementing frame number
            log_str.a7_freq=a7frequency;       // A7 frequency
            log_str.a15_freq=a15frequency;     // A15 frequency
            log_str.frame_rate=frame_rate;     // Frame rate
            log_str.time_in=ioctl_arg.time;    // Timestamp
            log_str.fr_error=frame_rate_error; // Frame error control value
            log_str.cpu4_online=cpu_online(4); // A15 hotplugged?
            log_str.cpu5_online=cpu_online(5); //      ""
            log_str.cpu6_online=cpu_online(6); //      ""
            log_str.cpu7_online=cpu_online(7); //      ""
            for(a=0; a<8; a++) {               // Number of tasks on each core
                log_str.nr_tasks_on_cpu[a]=nr_tasks_on_cpu[a];
            }
            log_str.number_allocation_chances=number_allocation_chances; // Possible allocs
            mutex_unlock(&logfile_mutex);

            /*
             * Start a new kthread to write the log
             * WARNING:
             *  Disable in production because of overhead
             */
            kthread_run(&write_log, &log_str, "log_thread");
#endif
            /*
             * Init done after first frame
             */
            is_init=1;

#ifdef GOV_PRINT_SPACES
			/*
			 * Format:
			 *  SpaceLeft[cpu0,...,cpu7],SpaceInit[a7,a15],...
			 *  SpaceFreq[a7,a17],SpaceDecr[a7,a15]
			 */
            KERNEL_ERROR_MSG("GOV|SPACES:%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;\n",a7space[0],a7space[1],a7space[2],a7space[3],a15space[0],a15space[1],a15space[2],a15space[3],a7space_init,a15space_init,a7space_init+curr_frequ_a7_nr*a7_space_increase_per_frequency,a15space_init+curr_frequ_a15_nr*a15_space_increase_per_frequency,a7space_decrementa15space_decrement);
#endif // GOV_PRINT_SPACES

exit:
            iciotl_new_frame_inuse=0; // allow new incoming frames
            break;
        default:
            KERNEL_ERROR_MSG("GOV|ERROR: IOCTL CMD not known\n");
            return(-1);
    }
    return(0); // success
}

/*
 * Functions called if char device is opened or closes -> don't do anything
 */
static int MyOpen(struct inode *i, struct file *f)
{
    return 0;
}
static int MyClose(struct inode *i, struct file *f)
{
    return 0;
}
static ssize_t MyRead(struct file *file, char *buffer, size_t length, loff_t *offset)
{
    KERNEL_VERBOSE_MSG("GOV|VERBOSE: device_read(%p,%p,%d)\n", file, buffer, length);
    return 0;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
static ssize_t MyWrite(struct file *file,
        const char *buffer,
        size_t length,
        loff_t *offset)
#else
static int device_write(struct inode *inode,
        struct file *file,
        const char *buffer,
        int length)
#endif
{
    KERNEL_VERBOSE_MSG ("GOV|VERBOSE: Entered message: %s \n", buffer);
    KERNEL_VERBOSE_MSG ("GOV|VERBOSE: device_write(%p,%s,%d)", file, buffer, length);

    return length;
}
static struct file_operations ioctlFops =
{
    .owner = THIS_MODULE,
    .open = MyOpen,
    .release = MyClose,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = MyIOctl,
#else
    .unlocked_ioctl = MyIOctl,
#endif
    .read=MyRead,
    .write=MyWrite,
};
static int IoctlInit(void) // initialization of character device
{
    int ret;
    struct device *dev_ret;

    // alloc region
    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, GAMEGOVERNOR_CHAR_DEVICE_NAME)) < 0){
        return ret;
    }

    // init
    cdev_init(&c_dev, &ioctlFops);

    // add device
    if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0)
    {
        return ret;
    }

    // create class and handle errors
    if (IS_ERR(cl = class_create(THIS_MODULE, GAMEGOVERNOR_CHAR_DEVICE_NAME "char")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }

    // create device and handle errors
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, GAMEGOVERNOR_CHAR_DEVICE_NAME)))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }

    return 0;
}
static void IoctlExit(void) // deinitialization of character device
{
    KERNEL_VERBOSE_MSG("GOV|VERBOSE: Cleanup_module called\n");
    device_destroy(cl, dev); // destroy device
    class_destroy(cl); // destroy class
    cdev_del(&c_dev); // delete device
    unregister_chrdev_region(dev, MINOR_CNT); // give memory back
}

//---------------------------------------------------------------------
//--------------------------End of ioctl interface---------------------
//---------------------------------------------------------------------

/*
 * Helper function for setting the processor frequencies
 */
static int cpufreq_set(struct cpufreq_policy *policy, unsigned int freq)
{
    int ret = -EINVAL;
    KERNEL_VERBOSE_MSG("GOV|VERBOSE: cpufreq_set for cpu %u, freq %u kHz\n", policy->cpu, freq);

    ret = __cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_L);

    if (!ret) { // handle policy
        if (policy->cpu<4 && policy->cpu>=0){ // setting frequency of the A7
            a7frequency=freq;
        }
        else if (policy->cpu>=4 && policy->cpu<8) // setting frequency of the A15
        {
            a15frequency=freq;
        }
        else { // error
            KERNEL_ERROR_MSG("GOV|ERROR: Set invalid core %u to frequency: %u \n", policy->cpu, freq);
        }
    }
    else { // error
        KERNEL_ERROR_MSG("GOV|ERROR: Setting frequency to %u for cpu: %d failed!\n", freq, policy->cpu);
    }

    return ret;
}

/*
 * Task processing procedure
 * INFO:
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 */
void inline process_task(struct task_struct *task)
{
    // Initialize struct if task is new or cloned
    if (task->task_struct_expansion_is_initialized==0) {
        init_task_struct_expansion(task);
    }
	// If task was cloned from another task, do a new initialization
    if (task->task_informations->pid!=task->pid) {
        init_task_struct_expansion(task);
    }

#ifdef HANDLE_CURRENT
	if(task->pid != current->pid) {
#endif // HANDLE_CURRENT
    // Update the workload history of the task
    update_workload_history(task);

    // Update the autocorellation if timer has expired
    if (autocorr_timer.timer_expired==1) {
        update_autocorr(task);
    }

    //perform the workload prediction for the task
    perform_workload_prediction(task);

    //allocate the task to a core and (if needed) increase the cpu frequency (variable)
    perform_task_allocation(task);

#ifdef DO_DEBUG
    thread_count++;
#endif // DO_DEBUG

#ifdef HANDLE_CURRENT
	} else {
    // Do nothing else than allocating on core 0
    sched_setaffinity_own(task, 0); // current task pinned to A7.1
    }
#endif // HANDLE_CURRENT
}

/*
 * Helper function for affinity handling
 * INFO:
 *  core_nr: 0-7
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 */
long inline sched_setaffinity_own(struct task_struct *task, short core_nr)
{
    int count_loop=0; // used for loop stop condition

    // Test for valid core_nr
    if (core_nr>=0 && core_nr<nr_cpu_ids) {

        if (task->task_informations->allocated_core != core_nr) { // core changed
            number_allocation_chances+=1;
        }
        task->task_informations->allocated_core=core_nr;

        // Enable a15 before assigning a task to it
        if (core_nr>3) {

            shutdown_counter_a15=0;

            if (!cpu_online(core_nr)) { // start in parallel
                kthread_run(&enable_a15, NULL, "enable_a15_thread");
            }

            nr_tasks_a15++;

            /*
             * Wait until core is available
             * INFO: max 0.1 second
             */
            while(!cpu_online(core_nr)) {
                count_loop++;
                schedule();
                udelay(100);

                // Check number of iterations to not get an infinity loop!
                if (count_loop>1000) {
                    KERNEL_ERROR_MSG("GOV|ERROR: Timeout for boot of core nr %d (Time > 0.1s)\n Can't assign task!\n", core_nr);
                    return -998; // Timeout error
                }
            }

            if (count_loop>0) {
                KERNEL_WARNING_MSG("GOV|WARNING: Waiting for core took %d iterations\n", count_loop);
            }
        }

        /*
         * Set task affinity using original function
         */
        return sched_setaffinity(task->pid, get_cpu_mask((unsigned int)core_nr));
    }
    else {
        KERNEL_ERROR_MSG("GOV|ERROR: Wrong CPU-Number: %d\n", core_nr);
        return -999; // Input error
    }
}

/*
 * Initialization of expansions of the task_struct
 * INFO:
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 */
void inline init_task_struct_expansion(struct task_struct *task)
{
    int i; // loop variable
    static short core_init=0; // used for core spreading

    /*
     * Allocate memory for the task_struct_expansion struct
     */
    task->task_informations=(task_struct_expansion *)kmalloc(sizeof(task_struct_expansion), GFP_KERNEL);
    if (task->task_informations==NULL) { // handle error
        KERNEL_ERROR_MSG("GOV|ERROR: INIT of Task: %d  FAILED\n", task->pid);
        return;
    }

	#ifdef HANDLE_CURRENT
    /*
     * Init all vars inside the struct
     */
	if(!(task->task_struct_expansion_is_initialized!=0 && task->task_informations->pid!=task->pid && task->pid == current->pid)) {
	#endif // HANDLE_CURRENT
	task->task_informations->allocated_core=core_init;

    core_init++;
    /*
     * Spread the initial core affinity over all cores of the A7
     * WARNING:
     *  As there is at least one new task every frame, oscilations
     *  in the number of tasks per core are very likely
     */
    if(core_init>3){
        core_init=1;
    }

	#ifdef HANDLE_CURRENT
	} else {
	task->task_informations->allocated_core=0; // allocate current task only to A7.1
	}
	#endif // HANDLE_CURRENT

    mutex_init(&task->task_informations->lock);
    task->task_informations->pid=task->pid;             // process id
    task->task_struct_expansion_is_initialized=1;       // true if initialized
    for (i=0; i<SIZE_WORKLOAD_HISTORY; i++){            // workload history
        task->task_informations->workload_history[i]=0;
    }
    task->task_informations->autocorr_max=0;            // max autocorrelation value
    task->task_informations->autocorr_shift=0;          // best  autocorrelation shift
    task->task_informations->prediction=0;              // predicted workload
    task->task_informations->prediction_cycles=0;       // predicted cycles
    task->task_informations->cpu_time=0;                // accumulated cpu time
}

/*
 * Get the workload of a task of the last frame
 * and update the workload history
 * INFO:
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 */
void inline update_workload_history(struct task_struct *task)
{
    int i; // loop variable
    int64_t time_new; // timestamp

    /*
     * Shift history by one
     * TODO: performance impact? circular buffer better?
     */
    for (i=SIZE_WORKLOAD_HISTORY-1; i>0; i--){
        task->task_informations->workload_history[i]=task->task_informations->workload_history[i-1];
    }

    /*
     * Get new total runtime
     * FORMULAR: workload=cpu_time_new-cpu_time_old
     * INFO:
     *  se.sum_exec_runtime -> total runtime in user and kernel space (in ns)
     *  Current cpu time is stored between frames
     */
    time_new=(int64_t)task->se.sum_exec_runtime;
    task->task_informations->workload_history[0]=time_new - task->task_informations->cpu_time;
    task->task_informations->cpu_time=time_new;
}

/*
 * Predict the next workload of a task
 * INFO:
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 */
void inline perform_workload_prediction(struct task_struct *task)
{
    // Prediction using the hybrid WMA predictor
    task->task_informations->prediction=WMA_hybrid_predictor(task->task_informations->workload_history, SIZE_WORKLOAD_HISTORY, task->task_informations->autocorr_max, task->task_informations->autocorr_shift);

    /*
     * Convert the prediction from time to processor cycles
     * FORMULAR: 10^-9s  *  10^3(1/s) => 10^-6
     *           prediction in cycles=(prediction * frequency)/10^6
     */
    if (task->task_informations->allocated_core<4 && task->task_informations->allocated_core>=0) { // Allocated to one of the A7 cores
        task->task_informations->prediction_cycles= div_s64(task->task_informations->prediction*a7frequency, 1e6);
    }
    else if (task->task_informations->allocated_core>=4 && task->task_informations->allocated_core<8) { // Allocated to one of the A15 cores
        task->task_informations->prediction_cycles= div_s64(task->task_informations->prediction*a15frequency, 1e6);
        task->task_informations->prediction_cycles=convert_a15cpucycles_to_a7(task->task_informations->prediction_cycles); // Convert to unified processor cycles
    }
    else { // Input Error
        KERNEL_ERROR_MSG("GOV|ERROR: Task %d was allocated to a invalid core Nr: %d \n", task->pid, task->task_informations->allocated_core);
    }
}

/*
 * Update a timer
 */
void update_timer(timer_struct *tr, uint64_t new_time)
{
    // Reset timer_expired var
    if (tr->timer_expired==1){
        tr->timer_expired=0;
    }

    // Check if timer has expired
    if ((new_time - tr->time) > tr->update_interval){
        tr->timer_expired=1;
        tr->time=new_time;
    }
}

/*
 * update the autocorrelation informations of a task
 * INFO:
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 */
void inline update_autocorr(struct task_struct *task)
{
    int64_t corr[SIZE_WORKLOAD_HISTORY]={-1}; // -1: no data
    int maxpos; // index of best autocorrelation value
    int64_t corrmax; // max autocorrelation value

    // Compute the autocorrelation for X lags
    autocorr(task->task_informations->workload_history, corr, SIZE_WORKLOAD_HISTORY-1, SIZE_WORKLOAD_HISTORY);

    // Get maximum of the autocors and the shift of the maximum
    get_max(&corr[1], SIZE_WORKLOAD_HISTORY-1, &corrmax, &maxpos);
    task->task_informations->autocorr_max=corrmax;
    task->task_informations->autocorr_shift=maxpos;

#ifdef THREAD_NAME_LOGGING
    // WARNING: thread name logging takes a long time -> OVERHEAD
    kthread_run(&write_thread_name_log, task, "thread_name_log_thread");
#endif
}

/*
 * Helper function to convert processor cycles
 * of the a15 to unified processor cycles
 * INFO:
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 */
int64_t inline convert_a15cpucycles_to_a7(int64_t cycles_in)
{
    int64_t cycles_out; // output

    /*
     * Conversion: // new_cycles=old_cycles * 2.01
     * CPUCYCLE_COVERSION_FACTOR is determined by benchmarking
     */
    cycles_out=div_s64((cycles_in*CPUCYCLE_COVERSION_FACTOR), 100);//new_cycles=old_cycles * 1.71

    return cycles_out;
}

/*
 * Allocate the task to a sufficient core and update
 * the cpu-frequency variables accordingly
 * INFO:
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 */
void inline perform_task_allocation(struct task_struct *task)
{
    // Numbers of cores with maximum space left
    short a7core_max_space;
    short a15core_max_space;
    // Store A7 related properties
    short curr_frequ_a7_nr_buff;
    int64_t a7space_buff[4];
    // Loop variabke
    int i;

    /*
     * If prediction is zero: keep the old configuration
     * WARNING:
     *  This can cause that A15 threads stay allocated on the big cores
     *  even after a core shutdown. Beside those showing up in the logs
     *  there is not a problem even if the task becomes active
     */
    if (task->task_informations->prediction_cycles==0){
        return;
    }

    /*
     * Try to assign to A7
     */
    a7core_max_space=get_max_spaceA7(NULL);

    // If the task can't be assigned to the A7, the old values have to be restored
    curr_frequ_a7_nr_buff=curr_frequ_a7_nr; // frequency index
    for(i=0; i<4; i++) { // spaces
        a7space_buff[i]=a7space[i];
    }

    while(1) { // loop until optimal allocation found

        // Check if old A7 core has enough space for task
        if (task->task_informations->allocated_core<4) {
            if (check_space_left_and_assignA7(task, task->task_informations->allocated_core)) {
                return; // success
            }
        }

        // Try to assign to A7 core with max space left
        if (check_space_left_and_assignA7(task, a7core_max_space)) {
            return; //success
        }

        // f=f_max? -> task has to be assigned to the A15
        if (curr_frequ_a7_nr==4) {
            break; // A7 allocation not successfull
        }
        else {
            // (theoretically) increase the frequency of the A7
            curr_frequ_a7_nr++;

            // Increase the A7 space according to
            // frequency change
            for(i=0; i<4; i++) {
                a7space[i]+=a7_space_increase_per_frequency;
            }
        }
    }

    // Reset target frequency as no frequency increase is needed (restore)
    curr_frequ_a7_nr = curr_frequ_a7_nr_buff;
    for(i=0; i<4; i++) {
        a7space[i]=a7space_buff[i];
    }

    /*
     * Assign to A15
     */
    a15core_max_space=get_max_spaceA15(NULL);

    while(1) {
        // Check if old A15 core has enough space for task
        if (task->task_informations->allocated_core>=4) {
            if (check_space_left_and_assignA15(task, task->task_informations->allocated_core)) {
                return; // success
            }
        }

        // Try to assign to core with max space left
        if (check_space_left_and_assignA15(task, a15core_max_space)) {
            return; // success
        }


        // f=f_max?
        if (curr_frequ_a15_nr==8) {
            /*
             * No space found
             * -> assign to core with most space left
             */
            a15space[a15core_max_space-4]=a15space[a15core_max_space-4] - task->task_informations->prediction_cycles;
            sched_setaffinity_own(task, a15core_max_space);
            return; // partial success
        }
        else {
            // Increase frequency as long as possible
            curr_frequ_a15_nr++;

            // Increase the A15 space according to
            // frequency change
            for(i=0; i<4; i++) {
                a15space[i]+=a15_space_increase_per_frequency;
            }
        }
    }
}

/*
 * Check if there is enough space on a core
 * Assign to that core is possible
 * INFO:
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 */
short inline check_space_left_and_assignA7(struct task_struct *task, short core_nr)
{
    if (core_nr>=4 || core_nr < 0) // Input Error
    {
        KERNEL_ERROR_MSG("GOV|ERROR: Wrong A7 core number: %d \n", core_nr);
    }

    // Check space
    if (a7space[core_nr] >= task->task_informations->prediction_cycles) {
        a7space[core_nr] -= task->task_informations->prediction_cycles;
        sched_setaffinity_own(task, core_nr);
        return 1; // success
    }
    else {
        return (0) ;// no success
    }
}

/*
 * Helper function for selectng the optimal A7 core
 * INFO:
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 * TODO: get rid of pointer argument
 */
short inline get_max_spaceA7(void *pointer)
{
#ifdef HANDLE_CURRENT
    int i=2; // loop variable
    short core=1; // return value
#else
    int i=1; // loop variable
    short core=0; // return value
#endif // HANDLE_CURRENT

    // iterate over core spaces
    for (i; i<4; i++ ){
        if (a7space[i] > a7space[core]){
            core = i;
        }
    }

    return core;
}

/*
 * Helper function for selecting the optimal A15 core
 * INFO:
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 * TODO: get rid of pointer argument
 */
short inline get_max_spaceA15(void *pointer)
{
    int i=1; // loop variable
    short core=0; // return value

    // iterate over core spaces
    for (i; i<4; i++){
        if (a15space[i] > a15space[core]){
            core = i;
        }
    }

    return core+4; // +4 for 15 cores
}

/*
 * Assing to proper A15 core
 * INFO:
 *  inline to reduce function call overhead
 *  (optimization possible during compilation)
 *  core number = 4...7
 */
short inline check_space_left_and_assignA15(struct task_struct *task, short core_nr)
{
    if (core_nr<4 || core_nr > 7) { // Input error
        KERNEL_ERROR_MSG("GOV|ERROR: Wrong A15 core number: %d\n", core_nr);
    }

    // Assign if possible
    if (a15space[core_nr-4] >= task->task_informations->prediction_cycles) {
        a15space[core_nr-4] -= task->task_informations->prediction_cycles;
        sched_setaffinity_own(task, core_nr);
        return 1; // success
    }
    else {
        return (0); // no success
    }
}

/*
 * Shutdown A15 cores (if not required)
 * (called as a thread)
 * PARAMETERS:
 *  in (always NULL but required for thread)
 */
int shutdown_a15(void * in)
{
    int a=0; // loop variable (TODO: replace with better named var)
    int ret_shutdown=0; // return value

    shut_down_core=1; // block hotplugging
    shutdown_counter_a15++;
    cpu_hotplug_driver_lock();
    mutex_lock(&hotplug_mutex);

#ifdef CONFIG_HOTPLUG_CPU
#ifdef POWERGATING_GAMEOPTIMIZED_GOV
    /*
     * Only shut down after shutdown_counter_a15
     * exceeds the shutdown limit
     */
    if (shutdown_counter_a15>SHUTDOWN_LIMIT) {
        // Turn of A15 cores
        A15_online=0;

        // Reset counter
        shutdown_counter_a15=0;

        KERNEL_DEBUG_MSG("GOV|DEBUG: DISABLE A15 \n");

        // Loop over all cores
        for (a=4; a<8; a++) {
            if (cpu_online(a)) {
                // Shut down a core
                ret_shutdown += cpu_down(a);
                if (ret_shutdown) { // Shutdown error
                    KERNEL_ERROR_MSG("GOV|ERROR: Can't shut down CPU %d\n", a);
                }
            }
        }
    }
#endif // POWERGATING_GAMEOPTIMIZED_GOV
#endif // CONFIG_HOTPLUG_CPU

    mutex_unlock(&hotplug_mutex); // unblock hutplugging
    cpu_hotplug_driver_unlock();
    shut_down_core=0;
    do_exit(0);

    return ret_shutdown; // 0 -> sucess, >0 -> failed
}

/*
 * Enable all A15 cores
 * (called as a thread)
 * PARAMETERS:
 *  in (always NULL but required for thread)
 * INFO:
 *  __cpuinit section for compiler
 */
int __cpuinit enable_a15(void * in)
{
    int a=0; // loop variable
    int ret_enable=0; // return value

    // block  hotplugging
    A15_online=1;
    shut_down_core=1;
    cpu_hotplug_driver_lock();
    mutex_lock(&hotplug_mutex);

#ifdef CONFIG_HOTPLUG_CPU
#ifdef POWERGATING_GAMEOPTIMIZED_GOV

    KERNEL_DEBUG_MSG("GOV|DEBUG: ENABLE A15 \n");

    // Iterate over all A15 cores
    for (a=4; a<8; a++) {
        if (!cpu_online(a)) {
            // Boot a core
            ret_enable += cpu_up(a);
            if (ret_enable) { // Enable Error
                KERNEL_ERROR_MSG("GOV|ERROR: Can't bring up CPU %d\n", a);
            }
        }
    }

#endif
#endif

    // unblock hotplugging
    mutex_unlock(&hotplug_mutex);
    cpu_hotplug_driver_unlock();
    shut_down_core=0;
    do_exit(0);

    return ret_enable; // 0 -> success, >0 -> failed
}

#ifdef DO_LOGGING
/*
 * Write governor log to a file
 * FORMAT: binary (to be parsed by external tool)
 */
int write_log(void *in)
{
    log_struct log_str_own; // Input scruct
    mm_segment_t old_fs; // File system pointer

    // Error handling
    if (in==NULL || fp_loggin_file==NULL) {
        KERNEL_ERROR_MSG("GOV|ERROR: Can't write log beacuse file is closed\n");
        do_exit(-99);
        return -99; // File Closed Error
    }

    nr_write_threads++;

    // lock/unlock scruct for copy
    mutex_lock(&logstruct_mutex);
    log_str_own=*(log_struct*)in;
    mutex_unlock(&logstruct_mutex);

    // init file system
    old_fs = get_fs();
    set_fs(KERNEL_DS);

    // lock/unlock file for writing binary log
    mutex_lock(&logfile_mutex);
    fp_loggin_file->f_op->write(fp_loggin_file, (char *)&log_str_own, sizeof(log_str_own), &fp_loggin_file->f_pos);
    mutex_unlock(&logfile_mutex);

    set_fs(old_fs); // done

    nr_write_threads--;

    do_exit(0); // success
}
#endif

#ifdef THREAD_NAME_LOGGING
/*
 * Write thread name log to a file
 * FORMAT: plain (huge overhead!)
 */
int write_thread_name_log(void *in)
{
    struct task_struct *ts; // Input struct (POINTER!)
    mm_segment_t old_fs; // File system pointer
    char buf[16]={"\0"}; // Buffer
    int a; // Loop variable
    struct timespec timestamp; // Log time
   getrawmonotonic(&timestamp);

    ts=(struct task_struct*)in; // Copy reference

    // Error handling
    if (in==NULL || fp_thread_name_logging==NULL) {
        KERNEL_ERROR_MSG("GOV|ERROR: Can't write log because file is closed\n");
        do_exit(-99);
        return -99; // File Closed Error
    }

    // Write timestamp in buffer
    sprintf(buf, "%llu; ", (uint64_t)timestamp.tv_sec*(uint64_t)1.0e9+(uint64_t)timestamp.tv_nsec);

    /*
     * As the thread name logs are beeing written every 10 seconds and often
     * more than 100 game threads are active, they have to write to the file
     * one after each other. Concurrent access is restricted by a mutex.
     */
    KERNEL_DEBUG_MSG("GOV|DEBUG: Writing Thread Name Log, Nr Threads waiting: %lld\n", nr_write_threads2);

    nr_write_threads2++;

    // init file system
    old_fs = get_fs();
    set_fs(KERNEL_DS);

    mutex_lock(&ts->task_informations->lock);
    // lock/unclock file for writing a row of data
    mutex_lock(&logfile_thread_name_mutex);
    fp_thread_name_logging->f_op->write(fp_thread_name_logging, buf, strlen(buf), &fp_thread_name_logging->f_pos);
    get_task_comm(buf, ts);
    fp_thread_name_logging->f_op->write(fp_thread_name_logging, buf, TASK_COMM_LEN, &fp_thread_name_logging->f_pos);
	// begin line
    fp_thread_name_logging->f_op->write(fp_thread_name_logging, ts->comm, TASK_COMM_LEN, &fp_thread_name_logging->f_pos);
    fp_thread_name_logging->f_op->write(fp_thread_name_logging, "; ", 2, &fp_thread_name_logging->f_pos);
    // max autocorrelation value
    sprintf(buf, "%lld", ts->task_informations->autocorr_max );
    fp_thread_name_logging->f_op->write(fp_thread_name_logging, buf, strlen(buf), &fp_thread_name_logging->f_pos);
    // 21 past workload values, seperated by a ';'
    for(a=0; a<SIZE_WORKLOAD_HISTORY ; a++){
        fp_thread_name_logging->f_op->write(fp_thread_name_logging, "; ", 2, &fp_thread_name_logging->f_pos);
        sprintf(buf, "%lld", ts->task_informations->workload_history[a]);
        fp_thread_name_logging->f_op->write(fp_thread_name_logging, buf, strlen(buf), &fp_thread_name_logging->f_pos);
    }
    // line end
    fp_thread_name_logging->f_op->write(fp_thread_name_logging, " \n", 2, &fp_thread_name_logging->f_pos);

    // done
    nr_write_threads2--;
    mutex_unlock(&logfile_thread_name_mutex);
	mutex_unlock(&ts->task_informations->lock);
    set_fs(old_fs);

    do_exit(0); // success
}
#endif
