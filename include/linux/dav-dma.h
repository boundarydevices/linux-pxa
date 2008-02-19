#ifndef DAV_DMA_H
#define DAV_DMA_H
/*
 * dav-dma.h
 *
 * Declares the ioctls used to use the DaVinci EDMA driver.
 *
 */
#include <mach/edma.h>
 
#define BASE_MAGIC '\xDd'

struct dav_dma_pool_t {
   u_int32_t   physaddr ;
   u_int32_t   size ;
};

#define DAV_POOLINFO		_IOW(BASE_MAGIC, 0x01, struct dav_dma_pool_t)
#define DAV_DMA_DODMA		_IOR(BASE_MAGIC, 0x02, edmacc_paramentry_regs)
#define DAV_ALLOCATE		_IOWR(BASE_MAGIC, 0x03, u_int32_t)
#define DAV_FREE		_IOW(BASE_MAGIC, 0x04, u_int32_t)

#define DAV_DMA_DEV "dav-dma-0"

#endif
