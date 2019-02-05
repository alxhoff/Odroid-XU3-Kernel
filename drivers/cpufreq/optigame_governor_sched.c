
#include <linux/sched.h>

#include "optigame_governor_sched.h"
#include "chrome_governor_kernel_write.h"

/* we have 8 cores. Using a bitfield to manage those cores seems reasonable
 * could use an array of uint8_ts to store the information on managed core but
 * I don't see a improvment that excuses additional amount of memory used.
 * With a little tweaking, this struct bahaves like a normal integer but is more
 * readable than just using an plain integer. */
static struct cores {
	uint8_t littleC0 :1;
	uint8_t littleC1 :1;
	uint8_t littleC2 :1;
	uint8_t littleC3 :1;
	uint8_t BIGC0 :1;
	uint8_t BIGC1 :1;
	uint8_t BIGC2 :1;
	uint8_t BIGC3 :1;
} _managedCores = { 0 };

#define managedCores (*((uint8_t *)&_managedCores))

int og_sched_coreManaged(enum CORE core) {
	return (managedCores & (0x0F << (core / 4))) > 0;
}

int og_sched_cpuManaged(enum CPU cpu) {
	return !((managedCores & cpu) ^ cpu);
}

void og_sched_addCoresToManaged(uint8_t cpus_bitmask) {
	managedCores |= cpus_bitmask;
}

void og_sched_removeCoresFromManaged(uint8_t cpus_bitmask) {
	managedCores &= ~(cpus_bitmask);
}

uint8_t og_sched_getManagedCores(void) {
	return managedCores;
}

int og_sched_setAffinity(struct task_struct *task, enum CORE core) {
	if (!og_sched_coreManaged(core)) {
		KERNEL_ERROR_MSG( "[SCHED] Chrome_Governor: Unmanaged core %u\n",
				core);
		return -EINVAL;
	}

	if (og_cpuinfo.BIG_state == 0 && core > littleC3)
		return -EINVAL;

	if (!cpu_online(core)) {
		KERNEL_ERROR_MSG( "[SCHED] Chrome_Governor: Cannot assign to core"
				"%u\n", core);
		return -EINVAL;
	}

	return sched_setaffinity(task->pid, get_cpu_mask(core));
}

