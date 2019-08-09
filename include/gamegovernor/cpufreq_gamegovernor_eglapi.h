#ifndef _GAMEGOVERNOR_EGL_H_
#define _GAMEGOVERNOR_EGL_H_

#define FILENAME "/dev/gamegovernor_device"
#define IOCTL_CMD_NEW_FRAME _IOR('D', 1, ioctl_struct_new_frame *)

typedef struct {
	uint64_t time;
	uint64_t sleep_time;
} ioctl_struct_new_frame;

#define TARGET_FRAME_RATE 30

#endif // _GAMEGOVERNOR_EGL_H_
