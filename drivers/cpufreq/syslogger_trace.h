#if !defined(_TRACE_LOGGER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_LOGGER_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM sys_logger
#define TRACE_INCLUDE_PATH ../../drivers/cpufreq
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE syslogger_trace

#include <linux/tracepoint.h>

TRACE_EVENT(enabled, TP_PROTO(bool enabled), TP_ARGS(enabled),
	    TP_STRUCT__entry(__field(bool, enabled)),
	    TP_fast_assign(__entry->enabled = enabled;),
	    TP_printk("Enabled: %d", __entry->enabled));

TRACE_EVENT(iteration, TP_PROTO(struct timespec *raw, struct timespec *real),
	    TP_ARGS(raw, real),
	    TP_STRUCT__entry(__field(u64, raw) __field(u64, real)),
	    TP_fast_assign(__entry->raw = timespec_to_ns(raw);
			   __entry->real = timespec_to_ns(real);),
	    TP_printk("raw: %llu, real: %llu", __entry->raw, __entry->real));

TRACE_EVENT(opengl_frame,
	    TP_PROTO(unsigned int ts, unsigned int period, unsigned int a15,
		     unsigned int a7, unsigned int mem, unsigned int gpu),
	    TP_ARGS(ts, period, a15, a7, mem, gpu),
	    TP_STRUCT__entry(
		    __field(unsigned int, ts) __field(unsigned int, period)
			    __field(unsigned int, a15) __field(unsigned int, a7)
				    __field(unsigned int, mem)
					    __field(unsigned int, gpu)),
	    TP_fast_assign(__entry->ts = ts; __entry->period = period;
			   __entry->a15 = a15; __entry->a7 = a7;
			   __entry->mem = mem; __entry->gpu = gpu),
	    TP_printk("ts: %u, period: %u, a15: %u, a7: %u, mem: %u, gpu: %u",
		      __entry->ts, __entry->period, __entry->a15, __entry->a7,
		      __entry->mem, __entry->gpu));

TRACE_EVENT(
	cpu_info,
	TP_PROTO(unsigned char cpu, bool online, uint64_t system, uint64_t user,
		 uint64_t idle),
	TP_ARGS(cpu, online, system, user, idle),
	TP_STRUCT__entry(__field(unsigned char, cpu) __field(bool, online)
				 __field(uint64_t, system)
					 __field(uint64_t, user)
						 __field(uint64_t, idle)),
	TP_fast_assign(__entry->cpu = cpu; __entry->online = online;
		       __entry->system = system; __entry->user = user;
		       __entry->idle = idle;),
	TP_printk("cpu: %u, online: %u, system: %llu, user: %llu, idle: %llu",
		  __entry->cpu, __entry->online, __entry->system, __entry->user,
		  __entry->idle));

TRACE_EVENT(cpu_freq, TP_PROTO(unsigned char cpu, unsigned int freq),
	    TP_ARGS(cpu, freq),
	    TP_STRUCT__entry(__field(unsigned char, cpu)
				     __field(unsigned int, freq)),
	    TP_fast_assign(__entry->cpu = cpu; __entry->freq = freq;),
	    TP_printk("cpu: %u, frequency: %u", __entry->cpu, __entry->freq));

TRACE_EVENT(ina231,
	    TP_PROTO(unsigned int a15, unsigned int a7, unsigned int mem,
		     unsigned int gpu),
	    TP_ARGS(a15, a7, mem, gpu),
	    TP_STRUCT__entry(__field(unsigned int, a15) __field(unsigned int,
								a7)
				     __field(unsigned int, mem)
					     __field(unsigned int, gpu)),
	    TP_fast_assign(__entry->a15 = a15; __entry->a7 = a7;
			   __entry->mem = mem; __entry->gpu = gpu;),
	    TP_printk("a15: %u, a7: %u, mem: %u, gpu: %u", __entry->a15,
		      __entry->a7, __entry->mem, __entry->gpu));

TRACE_EVENT(mali, TP_PROTO(unsigned int load, unsigned int freq),
	    TP_ARGS(load, freq),
	    TP_STRUCT__entry(__field(unsigned int, load)
				     __field(unsigned int, freq)),
	    TP_fast_assign(__entry->load = load; __entry->freq = freq;),
	    TP_printk("load: %u, freq: %u", __entry->load, __entry->freq));

TRACE_EVENT(exynos_temp,
	    TP_PROTO(unsigned int t0, unsigned int t1, unsigned int t2,
		     unsigned int t3, unsigned int t4),
	    TP_ARGS(t0, t1, t2, t3, t4),
	    TP_STRUCT__entry(__field(unsigned int, t0) __field(unsigned int, t1)
				     __field(unsigned int, t2)
					     __field(unsigned int, t3)
						     __field(unsigned int, t4)),
	    TP_fast_assign(__entry->t0 = t0; __entry->t1 = t1; __entry->t2 = t2;
			   __entry->t3 = t3; __entry->t4 = t4;),
	    TP_printk("A15-0: %u, A15-1: %u, A15-2: %u, A15-3: %u, GPU: %u",
		      __entry->t0, __entry->t1, __entry->t2, __entry->t3,
		      __entry->t4));

TRACE_EVENT(net_stats, TP_PROTO(struct rtnl_link_stats64 *stats),
	    TP_ARGS(stats),
	    TP_STRUCT__entry(__field(u64, rx_packets) __field(u64, rx_bytes)
				     __field(u64, tx_packets)
					     __field(u64, tx_bytes)),
	    TP_fast_assign(__entry->rx_packets = stats->rx_packets;
			   __entry->rx_bytes = stats->rx_bytes;
			   __entry->tx_packets = stats->tx_packets;
			   __entry->tx_bytes = stats->tx_bytes;),
	    TP_printk("rx: %llu (%llu), tx: %llu (%llu)", __entry->rx_packets,
		      __entry->rx_bytes, __entry->tx_packets,
		      __entry->tx_bytes));

#endif /* _TRACE_LOGGER_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
