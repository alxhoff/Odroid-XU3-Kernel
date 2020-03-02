#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/ktime.h>
#include <linux/cdev.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/kernel_stat.h>
#include <linux/hrtimer.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/netdevice.h>
#include <linux/cdev.h>

#include "ina231-misc.h"
#define CREATE_TRACE_POINTS
#include "syslogger_trace.h"

#include "EGLsyslog.h"

#define FIRST_MINOR     0
#define MINOR_CNT   1

#define KERNEL_ERROR_MSG(...) \             
    do { if (1) printk(KERN_ERR __VA_ARGS__); } while (0)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Hildenbrand");
MODULE_AUTHOR("Alex Hoffman");
MODULE_DESCRIPTION("Module for fast logging/tracing of system properties");

static bool __read_mostly log_opengl_frames = true;
module_param(log_opengl_frames, bool, S_IRUGO);
MODULE_PARM_DESC(log_opengl_frames, "Log timing for each OpenGL frame");

static bool __read_mostly log_cpu_info = true;
module_param(log_cpu_info, bool, S_IRUGO);
MODULE_PARM_DESC(log_cpu_info, "Log CPU system/user/idle time and state");

static bool __read_mostly log_cpu_freq = true;
module_param(log_cpu_freq, bool, S_IRUGO);
MODULE_PARM_DESC(log_cpu_freq, "Log CPU frequency for each first CPU in a policy group");

static bool __read_mostly log_ina231 = true;
module_param(log_ina231, bool, S_IRUGO);
MODULE_PARM_DESC(log_ina231, "Log Power consumption via ina231 sensors.");

static bool __read_mostly log_mali = true;
module_param(log_mali, bool, S_IRUGO);
MODULE_PARM_DESC(log_mali, "Log mali GPU information.");

static bool __read_mostly log_net_stats = true;
module_param(log_net_stats, bool, S_IRUGO);
MODULE_PARM_DESC(log_net_stats, "Log network interface rx/tx stats.");

static bool __read_mostly log_exynos_temp = true;
module_param(log_exynos_temp, bool, S_IRUGO);
MODULE_PARM_DESC(log_exynos_temp, "Log CPU and GPU temperature on Exynos boards.");

static unsigned int __read_mostly cpu = 0;
module_param(cpu, uint, S_IRUGO);
MODULE_PARM_DESC(cpu, "CPU to run/pin the logging thread on.");

static unsigned int __read_mostly interval = 100;
module_param(interval, uint, S_IRUGO);
MODULE_PARM_DESC(interval, "Interval between measurements in ms.");

static int param_set_enabled(const char *val, const struct kernel_param *kp);
struct kernel_param_ops enabled_ops = {
	.set = param_set_enabled,
	.get = param_get_bool,
};
static bool __read_mostly enabled = false;
module_param_cb(enabled, &enabled_ops, &enabled, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enabled, "Enabled/Disable logging dynamically.");

struct task_struct *logging_thread;
struct hrtimer timer;

u64 sum_time;
u64 max_time;
int nr_runs;

struct file *a15_ina231;
struct file *a7_ina231;
struct file *mem_ina231;
struct file *gpu_ina231;

struct file *mali_load;
struct file *mali_freq;

struct file *exynos_temp;

struct net_device *net_dev;

static dev_t dev;
static struct cdev c_dev;
static struct class *cl;

static int syslog_EGL_open(struct inode *i, struct file *f){return 0;}
static int syslog_EGL_close(struct inode *i, struct file *f){return 0;}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int syslog_EGL_ioctl(struct inode *i, struct file *f, 
        unsigned int cmd, unsigned long arg);
#else 
static long syslog_EGL_ioctl(struct file *f, unsigned int cmd, unsigned long arg);
#endif

static struct file_operations syslog_EGL_fops =
{
    .owner = THIS_MODULE,
    .open = syslog_EGL_open,
    .release = syslog_EGL_close,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = syslog_EGL_ioctl,
#else 
    .unlocked_ioctl = syslog_EGL_ioctl,
#endif
};

static int param_set_enabled(const char *val, const struct kernel_param *kp)
{
	bool old = enabled;
	int ret;

	if (THIS_MODULE->state != MODULE_STATE_LIVE)
		return -EINVAL;

	ret = param_set_bool(val, kp);
	if (ret < 0 )
		return ret;

	if (old == enabled)
		return 0;

	if (enabled) {
		printk("Enabling sys_logger.\n");
		hrtimer_start(&timer, ktime_set(0, interval * 1000000UL),
		              HRTIMER_MODE_REL_PINNED);
	} else
		printk("Disabling sys_logger.\n");
	return ret;
}

static int IOctlInit(void)
{
    int ret;
    struct device *dev_ret;

    if((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, EGL_SYSLOGGER_NAME)))
        return ret;

    cdev_init(&c_dev, &syslog_EGL_fops);

    if((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0)
        return ret;

    if(IS_ERR( cl = class_create(THIS_MODULE, EGL_SYSLOGGER_NAME "char")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }

    if(IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, EGL_SYSLOGGER_NAME)))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);

    }

    return 0;
}

static void IOctlExit(void)
{
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);
}

static void __log_opengl_frame(struct EGLLogFrame *lf)
{
    trace_opengl_frame(lf->frame_ts, lf->inter_frame_period);
}

static void __log_cpu_info(void)
{
	u64 system, user, idle;
	bool online;
	int cpu;

	for_each_possible_cpu(cpu) {
		online = cpu_online(cpu);

		system = kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
		user = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];

		trace_cpu_info(cpu, online, system, user, idle);
	}
}

static inline void __log_cpu_freq_cpu(int cpu)
{
	struct cpufreq_policy *policy;
	unsigned int freq;

	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		freq = policy->cur;
		cpufreq_cpu_put(policy);
	} else
		freq = 0;
	trace_cpu_freq(cpu, freq);
}

static void __log_cpu_freq(void)
{
	__log_cpu_freq_cpu(0);
	__log_cpu_freq_cpu(4);
}

static struct file *file_open_rdonly(const char *path)
{
	struct file *f;

	f = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(f))
		return NULL;

	return f;
}

static struct file *open_ina231(const char *path)
{
	mm_segment_t old_fs = get_fs();
	struct ina231_iocreg reg;
	struct file *f;
	int ret;

	f = file_open_rdonly(path);
	if (!f)
		return NULL;

	set_fs(KERNEL_DS);
	if (!f->f_op || !f->f_op->unlocked_ioctl)
		goto out_error;

	/* enable if necessary */
	ret = f->f_op->unlocked_ioctl(f, INA231_IOCGSTATUS, (long unsigned int) &reg);
	if (ret)
		goto out_error;

	if (!reg.enable) {
		reg.enable = true;
		ret = f->f_op->unlocked_ioctl(f, INA231_IOCSSTATUS, (long unsigned int) &reg);
		if (ret)
			goto out_error;
	}
	set_fs(old_fs);
	return f;
out_error:
	set_fs(old_fs);
	filp_close(f, NULL);
	return NULL;
}

static unsigned int file_read_uint(struct file *f)
{
	mm_segment_t old_fs = get_fs();
	static char buf[10];
	unsigned int val = 0;
	loff_t pos = 0;
	int ret;

	if (!f)
		return 0;

	set_fs(KERNEL_DS);
	ret = vfs_read(f, buf, sizeof(buf) - 1, &pos);
	set_fs(old_fs);
	if (ret > 0) {
		buf[ret] = 0;
		ret = kstrtouint(buf, 10, &val);
	}

	return ret ? 0 : val;
}

static void file_close(struct file *f)
{
	if (!f)
		return;

	filp_close(f, NULL);
}

static unsigned int read_uW_ina231(struct file *f)
{
	mm_segment_t old_fs = get_fs();
	struct ina231_iocreg reg;
	int ret;

	if (!f)
		return 0;

	set_fs(KERNEL_DS);
	ret = f->f_op->unlocked_ioctl(f, INA231_IOCGREG, (long unsigned int) &reg);
	set_fs(old_fs);
	if (ret)
		return 0;
	return reg.cur_uW;
}

static void __log_ina231(void)
{
	trace_ina231(read_uW_ina231(a15_ina231),
		     read_uW_ina231(a7_ina231),
		     read_uW_ina231(mem_ina231),
		     read_uW_ina231(gpu_ina231));
}

static void __log_mali(void)
{
	trace_mali(file_read_uint(mali_load),
		   file_read_uint(mali_freq));
}

static int file_read(struct file *f, char *buf, int len)
{
	mm_segment_t old_fs = get_fs();
	loff_t pos = 0;
	int ret;

	if (!f)
		return -EINVAL;

	set_fs(KERNEL_DS);
	ret = vfs_read(f, buf, len - 1, &pos);
	set_fs(old_fs);
	if (ret > 0)
		buf[ret] = 0;

	return ret ? 0 : -EINVAL;
}

static void __log_exynos_temp(void)
{
	/* Unfortunately, the exynos driver only allows to directly query
	 * the maximum temperature accross all 5 sensors via
	 * (exynos_thermal_get_value()). To get the specific values, we
	 * have to go via sysfs.
	 */
	static unsigned int temp[5];
	static char buf[100];
	static char num[10];
	char *pos;
	int i, j, ret;

	if (file_read(exynos_temp, buf, sizeof(buf)))
		return;

	for (i = 0, pos = buf; i < 5; i++, pos++) {
		pos = strchr(pos, ':');
		if (!pos)
			return;
		pos += 2;
		for (j = 0; j < (sizeof(num) - 1); j++) {
			if (*pos < '0' || *pos > '9')
				break;
			if (pos > &buf[sizeof(buf) - 1])
				return;
			num[j] = *(pos++);
		}
		num[j] = 0;
		ret = kstrtouint(num, 10, &temp[i]);
		if (ret)
			return;
	}
	trace_exynos_temp(temp[0], temp[1], temp[2], temp[3], temp[4]);
}

static void __log_iteration_start(void)
{
	struct timespec raw, real;

	/* Using the uptime clock for trace-cmd (to minimize overhead on every
	 * event), this allows to map events to actual mon/real times.
	 */
    getrawmonotonic(&raw); 
    /** raw.tv_sec += 27; */
	getnstimeofday(&real);
	trace_iteration(&raw, &real);
}

static void __log_net_stats(void)
{
	static struct rtnl_link_stats64 stats;

	if (net_dev && dev_get_stats(net_dev, &stats))
		trace_net_stats(&stats);
}

static int log_func(void *data)
{
	static bool was_enabled;
	s64 start, time;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (!enabled && was_enabled) {
			trace_enabled(false);
			was_enabled = false;
		}
		if (kthread_should_stop()) {
			if (was_enabled)
				trace_enabled(false);
			break;
		}
		schedule();

		if (enabled && !was_enabled) {
			trace_enabled(true);
			was_enabled = true;
		}

		start = ktime_to_ns(ktime_get());
		__log_iteration_start();
		if (log_cpu_info)
			__log_cpu_info();
		if (log_cpu_freq)
			__log_cpu_freq();
		if (log_ina231)
			__log_ina231();
		if (log_mali)
			__log_mali();
		if (log_exynos_temp)
			__log_exynos_temp();
		if (log_net_stats)
			__log_net_stats();
		time = ktime_to_ns(ktime_get()) - start;
		sum_time += time;
		if (time > max_time)
			max_time = time;
		nr_runs++;
	}

	return 0;
}

enum hrtimer_restart logger_wakeup(struct hrtimer *timer)
{
	ktime_t now = ktime_get();

	wake_up_process(logging_thread);

	if (!enabled)
		return HRTIMER_NORESTART;
	hrtimer_forward(timer, now, ktime_set(0, interval * 1000000UL));
	return HRTIMER_RESTART;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int syslog_EGL_ioctl(struct inode *i, struct file *f, 
        unsigned int cmd, unsigned long arg)
#else 
static long syslog_EGL_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
#endif
{
    static struct EGLLogFrame lf;

    switch(cmd){
        case IOCTL_EGL_LOG_FRAME:
            if(copy_from_user(&lf, (struct EGLLogFrame *)arg, sizeof(struct EGLLogFrame))){
                KERNEL_ERROR_MSG("Syslog|ERROR: Copy from used failed\n");
                return -EACCES;
            }
            __log_opengl_frame(&lf);         
        break;
        default: return -EINVAL;
    }
}

static int __init init(void)
{
	if (!cpu_online(cpu)) {
		printk("The CPU %d cannot be used.\n", cpu);
		return -EINVAL;
	}

	if (log_ina231) {
		a15_ina231 = open_ina231("/dev/sensor_arm");
		a7_ina231 = open_ina231("/dev/sensor_kfc");
		mem_ina231 = open_ina231("/dev/sensor_mem");
		gpu_ina231 = open_ina231("/dev/sensor_g3d");
	}
	if (log_mali) {
		mali_load = file_open_rdonly("/sys/bus/platform/drivers/mali/11800000.mali/utilization");
		mali_freq = file_open_rdonly("/sys/bus/platform/drivers/mali/11800000.mali/clock");
	}
	if (log_exynos_temp)
		exynos_temp = file_open_rdonly("/sys/devices/10060000.tmu/temp");
	if (log_net_stats)
		net_dev = dev_get_by_name(&init_net, "eth0");

	logging_thread = kthread_create(&log_func, NULL, "sys_logger");
	if (!logging_thread)
		return -ENOMEM;
	kthread_bind(logging_thread, cpu);

	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	timer.function = logger_wakeup;
	if (enabled)
		hrtimer_start(&timer, ktime_set(0, interval * 1000000UL), HRTIMER_MODE_REL_PINNED);

    IOctlInit();

	return 0;
}

static void __exit cleanup(void)
{
	hrtimer_cancel(&timer);
	kthread_stop(logging_thread);

	file_close(a15_ina231);
	file_close(a7_ina231);
	file_close(mem_ina231);
	file_close(gpu_ina231);
	file_close(mali_load);
	file_close(mali_freq);
	file_close(exynos_temp);
	if (net_dev)
		dev_put(net_dev);

	if (nr_runs)
		do_div(sum_time, nr_runs);
	printk("Average runtime: %lld ns\n", sum_time);
	printk("Max runtime: %lld ns\n", max_time);

    IOctlExit();
}

module_init(init);
module_exit(cleanup);
