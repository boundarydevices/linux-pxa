#ifndef __GPIO_INT_DAVINCI_H_INCLUDED__
#define __GPIO_INT_DAVINCI_H_INCLUDED__

/*
 * ioctls
 */
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define BASE_MAGIC 0xBD
#define GPIO_INT_DAVINCI_THRESHOLD   _IOR(BASE_MAGIC, 0x01, unsigned long) // expects input threshold
#define GPIO_INT_DAVINCI_READMODE    _IOR(BASE_MAGIC, 0x02, unsigned long) // returns sync count after wait

#endif
