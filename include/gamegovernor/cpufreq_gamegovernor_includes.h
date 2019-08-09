#ifndef _GAMEGOVERNOR_INCLUDES_H_
#define _GAMEGOVERNOR_INCLUDES_H_

#include <linux/types.h>
#include <gamegovernor/cpufreq_gamegovernor_eglapi.h>

#ifdef UNIT_TEST
	#include <sys/types.h>
	#include <stdint.h>
	int64_t div64_s64(int64_t a, int64_t b);
#else
    #include <gamegovernor/cpufreq_gamegovernor_sched.h>

/*
 * Defines for configuration of governor
 */

// Define to enable logging
#define DO_LOGGING

#ifdef DO_LOGGING
	#define LOGFILE "/data/local/gamegovernor/Governor_LOG"
	#pragma message( "Compiled with logging flag!")
#endif // DO_LOGGING

// Define to enable powergating
#define POWERGATING_GAMEOPTIMIZED_GOV

// Experimental handling of current thread
//#define HANDLE_CURRENT

// Define for debugging purposes
//#define GOV_PRINT_SPACES

// Define to enable thread name logging
//#define THREAD_NAME_LOGGING

#ifdef THREAD_NAME_LOGGING
	#define LOGFILE_THREAD_NAME "/data/local/gamegovernor/Thread_Name_LOG"
	#pragma message("Compiled with thread name logging flag!")
#endif // THREAD_NAME_LOGGING

#endif // UNIT_TEST

// Migration factor between A7 and A15 cores
// Determined by benchmarking
#define CPUCYCLE_COVERSION_FACTOR 201

// Set parameters of frame error controller
#define AGGRESSIVENESS_FACTOR_UP 20
#define AGGRESSIVENESS_FACTOR_DOWN 10
#define FRAME_HIT_REDUCTION_FACTOR 5

// Set frame rate tolerance
#define FRAME_RATE_UPPER_BOUND 33
#define FRAME_RATE_LOWER_BOUND 27

// Set how many frames without BIG cores have to be rendered
// before shutting down the A15
#define SHUTDOWN_LIMIT 10

// Choose log levels
#define REPORT_DEBUG    1
#define REPORT_ERROR    1
#define REPORT_VERBOSE  1
#define REPORT_WARNING 	1

// Character device numbers and name
#define FIRST_MINOR 0
#define MINOR_CNT 1
#define IOCTL_MAJOR 240
#define GAMEGOVERNOR_CHAR_DEVICE_NAME "gamegovernor_device"

/*
 * Function prototypes
 */
extern void autocorr( const int64_t *data_in, int64_t *outcorr_out, int tau, int length );
int64_t corr_own(const int64_t *data_in, int a, int length);
int64_t mean(const int64_t *data_in, int length);
int64_t WMA( const int64_t *cpu_time_history, int length );
void get_max(const int64_t *a, int length, int64_t *max, int *pos);
int64_t WMA_hybrid_predictor(const int64_t *data, int length, int64_t autocorr_max, int pos);

/*
 * Define logging macros
 */
#define KERNEL_DEBUG_MSG(...) \
            do { if (REPORT_DEBUG) printk(KERN_INFO __VA_ARGS__); } while (0)

#define KERNEL_ERROR_MSG(...) \
            do { if (REPORT_ERROR) printk(KERN_ERR __VA_ARGS__); } while (0)

#define KERNEL_LOGGG_MSG(...) \
            do { if (REPORT_LOGGG) printk(KERN_ERR __VA_ARGS__); } while (0)

#define KERNEL_VERBOSE_MSG(...) \
            do { if (REPORT_VERBOSE) printk(KERN_INFO __VA_ARGS__); } while (0)

#define KERNEL_WARNING_MSG(...) \
            do { if (REPORT_WARNING) printk(KERN_WARNING __VA_ARGS__); } while (0)


/*
 * Define types
 */
typedef struct { // timer
	uint64_t time;                  //holds the time of the last update
	const uint64_t update_interval; //in ns
	short timer_expired;            //var to check if timer has expired (1==yes)
} timer_struct;

typedef struct { // logging struct
	int64_t nr;
	int64_t a7_freq;
	int64_t a15_freq;
	uint64_t frame_rate;
	uint64_t time_in;
	int64_t fr_error;
	short cpu4_online;
	short cpu5_online;
	short cpu6_online;
	short cpu7_online;
	int number_allocation_chances;
	int nr_tasks_on_cpu[8];
} log_struct;

#endif // _GAMEGOVERNOR_INCLUDES_H_
