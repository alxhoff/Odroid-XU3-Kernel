#ifndef __EGLSYSLOG_H__
#define __EGLSYSLOG_H__

#include <linux/ioctl.h>
#include <linux/types.h>

#define OPENGL_TARGET_FPS 30

#define EGL_SYSLOGGER_NAME "EGLSyslogger"
#define EGL_SYSLOGGER_DEV "/dev/" EGL_SYSLOGGER_NAME
#define IOCTL_EGL_LOG_FRAME _IOW('g', 1, struct EGLLogFrame *)

#define EGL_IOCTL_LOGGING
#define LIMIT_FRAME_PERIOD

struct EGLLogFrame {
	uint64_t frame_ts;
	uint64_t inter_frame_period;
};

#endif // __EGLSYSLOG_H__
