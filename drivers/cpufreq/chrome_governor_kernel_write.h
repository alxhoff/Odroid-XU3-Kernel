#ifndef CHROME_GOVERNOR_KERNEL_WRITE_H
#define CHROME_GOVERNOR_KERNEL_WRITE_H

#define KERNEL_DEBUG_MSG(...) \
            do { printk(KERN_INFO __VA_ARGS__); } while (0)

#define KERNEL_ERROR_MSG(...) \
            do { printk(KERN_ERR __VA_ARGS__); } while (0)

#define KERNEL_LOGGG_MSG(...) \
            do { printk(KERN_ERR __VA_ARGS__); } while (0)

#define KERNEL_VERBOSE_MSG(...) \
            do { printk(KERN_INFO __VA_ARGS__); } while (0)

#define KERNEL_WARNING_MSG(...) \
            do { printk(KERN_WARNING __VA_ARGS__); } while (0)

#endif //CHROME_GOVERNOR_KERNEL_WRITE_H