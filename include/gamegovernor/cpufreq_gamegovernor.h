#ifndef _GAMEGOVERNOR_H_
#define _GAMEGOVERNOR_H_

static int IoctlInit(void);
static void IoctlExit(void);

static long MyIOctl( struct file *File,unsigned int cmd, unsigned long arg );
static int MyOpen(struct inode *i, struct file *f);
static int MyClose(struct inode *i, struct file *f);
static ssize_t MyRead (struct file *file, char *buffer, size_t length, loff_t *offset);
static ssize_t MyWrite(struct file *file, const char *buffer, size_t length, loff_t *offset);
static int cpufreq_set(struct cpufreq_policy *policy, unsigned int freq);
void inline process_task(struct task_struct *task);
static struct file_operations ioctlFops;
static int IoctlInit(void);
static void IoctlExit(void);
long inline sched_setaffinity_own(struct task_struct *task, short core_nr);
void inline init_task_struct_expansion(struct task_struct *task);
void inline update_workload_history(struct task_struct *task);
void inline perform_workload_prediction(struct task_struct *task);
void update_timer(timer_struct *tr, uint64_t new_time);
void inline update_autocorr(struct task_struct *task);
int64_t inline convert_a15cpucycles_to_a7(int64_t cycles_in);
void inline perform_task_allocation(struct task_struct *task);
short inline check_space_left_and_assignA7(struct task_struct *task, short core_nr);
short inline get_max_spaceA15(void *pointer);
short inline get_max_spaceA7(void *pointer);
short inline check_space_left_and_assignA15(struct task_struct *task, short core_nr);
int shutdown_a15(void * in);
int enable_a15(void * in);
int write_log(void *in);
int write_thread_name_log(void *in);

#endif // _GAMEGOVERNOR_H_
