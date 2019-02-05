#ifndef OPTIGAME_GOVERNOR_SCHED
#define OPTIGAME_GOVERNOR_SCHED

#include "optigame_governor_config.h"
#include "optigame_governor_stats.h"

#define IS_BIG_LITTLE   1
#define CPU_CORE_COUNT  4

enum CPU {
	CPU_NONE        = 0x00, 
    CPU_little      = 0x0F, 
    CPU_BIG         = 0xF0, 
    CPU_BIGlittle   = 0xFF
};

enum CORE {
    littleC0    = 0,
    littleC1    = 1,
    littleC2    = 2,
    littleC3    = 3,
    BIGC0       = 4,
    BIGC1       = 5,
    BIGC2       = 6,
    BIGC3       = 7
};

struct cpufreq_optigame_governor_cpuinfo {
    bool BIG_state;
    uint8_t shutdown_cpu;
    
    struct optigame_stats stats;
};

int og_sched_coreManaged(enum CORE core);
int og_sched_cpuManaged(enum CPU cpu);
void og_sched_addCoresToManaged(uint8_t cpus_bitmask);
void og_sched_removeCoresFromManaged(uint8_t cpus_bitmask);
uint8_t og_sched_getManagedCores(void);

int og_sched_setAffinity(struct task_struct *task, enum CORE core);

extern struct cpufreq_optigame_governor_cpuinfo og_cpuinfo; 


#endif
