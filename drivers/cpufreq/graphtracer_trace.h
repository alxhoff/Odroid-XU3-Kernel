#define _GRAPHTRACER_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM sys_logger
#define TRACE_INCLUDE_PATH /home/alxhoff/Work/Optigame/android_builds/voodik/Android_7.1/android_source_xu3_Android7.1/kernel/hardkernel/odroidxu3/drivers/cpufreq
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE graphtracer_trace

#include <linux/tracepoint.h>



#endif /* _TRACE_LOGGER_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
