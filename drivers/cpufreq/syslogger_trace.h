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

TRACE_EVENT(
	opengl_frame,
	TP_PROTO(struct timespec *raw, struct timespec *user_space_ts,
		 u64 period, u64 a15, u64 a7, u64 mem, u64 gpu),
	TP_ARGS(raw, user_space_ts, period, a15, a7, mem, gpu),
	TP_STRUCT__entry(__field(u64, raw) __field(u64, user_space_ts)
				 __field(u64, ts) __field(u64, period)
					 __field(u64, a15) __field(u64, a7)
						 __field(u64, mem)
							 __field(u64, gpu)),
	TP_fast_assign(__entry->raw = timespec_to_ns(raw);
		       __entry->user_space_ts = timespec_to_ns(user_space_ts);
		       __entry->period = period; __entry->a15 = a15;
		       __entry->a7 = a7; __entry->mem = mem;
		       __entry->gpu = gpu),
	TP_printk(
		"raw ts: %llu, user space ts: %llu, period: %llu, a15: %llu, a7: %llu, mem: %llu, gpu: %llu",
		__entry->raw, __entry->user_space_ts, __entry->period,
		__entry->a15, __entry->a7, __entry->mem, __entry->gpu));

TRACE_EVENT(
	cpu_info,
	TP_PROTO(u8 cpu, bool online, uint64_t system, uint64_t user,
		 uint64_t idle),
	TP_ARGS(cpu, online, system, user, idle),
	TP_STRUCT__entry(__field(u8, cpu) __field(bool, online)
				 __field(uint64_t, system)
					 __field(uint64_t, user)
						 __field(uint64_t, idle)),
	TP_fast_assign(__entry->cpu = cpu; __entry->online = online;
		       __entry->system = system; __entry->user = user;
		       __entry->idle = idle;),
	TP_printk("cpu: %u, online: %u, system: %llu, user: %llu, idle: %llu",
		  __entry->cpu, __entry->online, __entry->system, __entry->user,
		  __entry->idle));

TRACE_EVENT(cpu_freq, TP_PROTO(u8 cpu, u64 freq), TP_ARGS(cpu, freq),
	    TP_STRUCT__entry(__field(u8, cpu) __field(u64, freq)),
	    TP_fast_assign(__entry->cpu = cpu; __entry->freq = freq;),
	    TP_printk("cpu: %u, frequency: %u", __entry->cpu, __entry->freq));

TRACE_EVENT(ina231, TP_PROTO(u64 a15, u64 a7, u64 mem, u64 gpu),
	    TP_ARGS(a15, a7, mem, gpu),
	    TP_STRUCT__entry(__field(u64, a15) __field(u64, a7)
				     __field(u64, mem) __field(u64, gpu)),
	    TP_fast_assign(__entry->a15 = a15; __entry->a7 = a7;
			   __entry->mem = mem; __entry->gpu = gpu;),
	    TP_printk("a15: %u, a7: %u, mem: %u, gpu: %u", __entry->a15,
		      __entry->a7, __entry->mem, __entry->gpu));

TRACE_EVENT(mali, TP_PROTO(u64 load, u64 freq), TP_ARGS(load, freq),
	    TP_STRUCT__entry(__field(u64, load) __field(u64, freq)),
	    TP_fast_assign(__entry->load = load; __entry->freq = freq;),
	    TP_printk("load: %u, freq: %u", __entry->load, __entry->freq));

TRACE_EVENT(exynos_temp, TP_PROTO(u64 t0, u64 t1, u64 t2, u64 t3, u64 t4),
	    TP_ARGS(t0, t1, t2, t3, t4),
	    TP_STRUCT__entry(__field(u64, t0) __field(u64, t1) __field(u64, t2)
				     __field(u64, t3) __field(u64, t4)),
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
