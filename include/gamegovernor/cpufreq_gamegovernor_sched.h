#ifndef _GAMEGOVERNOR_SCHED_H_
#define _GAMEGOVERNOR_SCHED_H_

// How many past workloads should be stored?
// WARNING: a high value leads to a huge overhead
#define SIZE_WORKLOAD_HISTORY 21

/*
 * Define struct
 */
typedef struct {
	pid_t pid;
	int64_t workload_history[SIZE_WORKLOAD_HISTORY];
	int64_t autocorr_max;
	int autocorr_shift;
	int64_t prediction;
	int64_t prediction_cycles;
	int64_t cpu_time;
	short allocated_core;
    struct mutex lock;
} task_struct_expansion;

#endif // _GAMEGOVERNOR_SCHED_H_
