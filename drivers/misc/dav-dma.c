/*
 *  linux/drivers/misc/dav-dma.c
 *
 *  Copyright (C) 2007 Boundary Devices, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <mach/edma.h>
#include <linux/dav-dma.h>
#include <asm/uaccess.h>
#include <linux/mm.h>

#ifndef MODULE
#include "ffit.h"
#else
#include "ffit.c"
#endif
#include <linux/dma-mapping.h>

// #define DEBUG
#ifdef DEBUG
#define DEBUGMSG( __fmt, ... ) printk( KERN_ERR __fmt, ## __VA_ARGS__ )
#else
#define DEBUGMSG( __fmt, ... ) 
#endif

struct dav_dma_dev_t {
	struct cdev cdev; /* Char device structure */
};

static int dav_dma_major = 0 ;
static int dav_dma_minor = 0 ;
static int const dav_dma_nr_devs = 1 ;
static char dav_dma_name[] = {
        "dav_dma"
};
static struct dav_dma_dev_t dav_dma_dev ;
static struct class *dav_dma_class;
static unsigned pool_size = 0 ;
static unsigned pool_physaddr = 0 ;
static unsigned phys_size = 0 ;
static void *pool_data ;
static dma_addr_t pool_phys ;
static list_header_t *mem_pool ;
static DECLARE_MUTEX(pool_lock);
static int dmach = -1 ;
static int tcc=TCC_ANY ;
static struct completion dma_completion = COMPLETION_INITIALIZER(dma_completion);

typedef struct {
	struct list_head node_ ;
	unsigned long    size_ ;
	unsigned long    pad_ ;	// to 16-bytes
} allocHeader_t ;

static void *pool_alloc( unsigned size )
{
	void *rval = 0 ;

	if( ( 0 == pool_data ) || ( 0 == mem_pool ) ){
		printk( KERN_ERR "%s: no memory pool\n", __FUNCTION__ );
		return 0 ;
	}

	if( down_interruptible(&pool_lock) ) {
		printk( KERN_ERR "%s: interrupted\n", __FUNCTION__ );
		return 0 ;
	}

	rval = rtl_malloc( mem_pool, size );

	DEBUGMSG( "alloc: %u/%p/char 0x%02x\n", size, rval, ((u8 *)rval)[sizeof(allocHeader_t)] );

	up(&pool_lock);

	return rval ;
}

static void pool_free( void *ptr )
{
	if( down_interruptible(&pool_lock) )
		return ;

	DEBUGMSG( "free: %p/char 0x%02x\n", ptr, ((u8 *)ptr)[sizeof(allocHeader_t)] );

	rtl_free(mem_pool,ptr);

	up(&pool_lock);
}

static int dav_dma_open(struct inode *i, struct file *f)
{
	struct list_head *allocs = (struct list_head *)kmalloc( sizeof(struct list_head), GFP_KERNEL );
	if( allocs ) {
		INIT_LIST_HEAD(allocs);
		f->private_data = allocs ;
		DEBUGMSG( "%s:\n", __FUNCTION__ );
		return 0 ;
	}

	return -ENOMEM ;
}

static int dav_dma_release(struct inode *i, struct file *f)
{
	struct list_head *allocs = (struct list_head *)f->private_data ;
	DEBUGMSG( "%s:\n", __FUNCTION__ );
	if( allocs ) {
		while( !list_empty(allocs) ) {
			allocHeader_t *node = (allocHeader_t *)allocs->next ;
			if( ( 0 == node->node_.next )
			    ||
			    ( 0 == node->node_.prev )
			    ||
			    ( &node->node_ == node->node_.next )
			    ||
			    ( &node->node_ == node->node_.prev )
			    ||
			    ( ( node->node_.next != allocs ) 
                              &&
                              ( 0 != ((unsigned long)node->node_.next & 15 ) ) )
			    ||
			    ( ( node->node_.prev != allocs ) 
                              &&
			      ( 0 != ((unsigned long)node->node_.prev & 15 ) ) ) ){
				printk( KERN_ERR "Invalid memory node %p/%p/%p/%lu/%lu\n", 
					node, 
					node ? node->node_.next : 0,
					node ? node->node_.prev : 0,
					node ? node->size_ : 0, 
					node ? node->pad_ : 0 
				      );
				break ;
			}
			else {
				list_del(&node->node_);
//				memset( node, 0xbb, node->size_ + sizeof(*node) );
				pool_free(node);
			}
		}

		kfree( allocs );
	}

	f->private_data = 0 ;

	return 0 ;
}

static void dav_dma_callback(int lch, unsigned short ch_status, void *data)
{
	DEBUGMSG( "%s: %u\n", __FUNCTION__, ch_status );
        complete(&dma_completion);
}

static int dav_dma_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long param)
{
	DEBUGMSG( "%s\n", __FUNCTION__);
	switch (cmd) {
		case DAV_POOLINFO: {
                        struct dav_dma_pool_t pi ;
			pi.physaddr = pool_phys ;
			pi.size = pool_size ;
			if( 0 == copy_to_user( (void *)param, &pi, sizeof(pi) ) )
				return 0 ;
			else
                                return -EFAULT ;
		}
		case DAV_ALLOCATE: {
			unsigned size ;
			allocHeader_t *mem ;
			unsigned long  offs ;

			struct list_head *allocs = (struct list_head *)f->private_data ;
			if( 0 == allocs )
				return -EFAULT ;

			if( copy_from_user( &size, (void __user *)param, sizeof(size) ) )
				return -EFAULT;

			if( 0 == size ) {
				printk( KERN_ERR "alloc zero size\n" );
				return -ENOMEM ;
			}

			mem = (allocHeader_t *)pool_alloc(size+sizeof( allocHeader_t ));

			if( 0 == mem ) {
				printk( KERN_DEBUG "DAV_ALLOC error: %u bytes\n", size );
				return -ENOMEM ;
			}

			mem->size_ = size ;
			mem->pad_  = 0 ;
			list_add( &mem->node_, allocs );

			offs = (char *)(mem+1) - (char *)pool_data ;

			if( copy_to_user( (void __user *)param, &offs, sizeof(offs) ) )
				return -EFAULT ;
			else
				return 0 ;
		}
		case DAV_FREE: {
			allocHeader_t *mem ;
			unsigned long  offs ;

			if( copy_from_user( &offs, (void __user *)param, sizeof(offs) ) )
				return -EFAULT;

			if( (0 == offs) || (pool_size <= offs) ) {
				printk( KERN_ERR "Invalid DAV free: %lx\n", offs );
				return -EFAULT ;
			}

			mem = (allocHeader_t *)((char *)pool_data + offs - sizeof(allocHeader_t) );

			if( ( 0 == mem->node_.next )
			    ||
			    ( 0 == mem->node_.prev )
			    ||
			    ( &mem->node_ == mem->node_.next )
			    ||
			    ( &mem->node_ == mem->node_.prev )
			    ||
                            ( ( mem->node_.next != f->private_data )
                              &&
                              ( 0 != ((unsigned long)mem->node_.next & 15 ) ) )
			    ||
                            ( ( mem->node_.prev != f->private_data )
                              &&
			      ( 0 != ((unsigned long)mem->node_.prev & 15 ) ) ) ){
				printk( KERN_ERR "free invalid ptr: %p/%p/%p\n", 
					&mem->node_,
					mem->node_.next,
					mem->node_.prev );
				return -EFAULT ;
			}

			list_del(&mem->node_);
//			memset( mem, 0xaa, mem->size_ + sizeof(allocHeader_t) );
			pool_free(mem);
			return 0 ;
		}

		case DAV_DMA_DODMA: {
			edmacc_paramentry_regs regs ;
			int rval ;
			if( copy_from_user( &regs, (void *)param, sizeof(regs) ) ){
				printk( KERN_ERR "%s: Invalid user ptr\n", __FUNCTION__ );
				return -EFAULT ;
			}
                        regs.opt = (regs.opt & ~TCC)|tcc ;
			davinci_set_dma_params(dmach, &regs);

			rval = davinci_start_dma(dmach);
			DEBUGMSG( "%s: dma %d\n", __FUNCTION__, rval );

			if( !wait_for_completion_interruptible_timeout(&dma_completion, 100 ) ){
	                	printk( KERN_ERR "%s: timeout\n", __FUNCTION__ );
                     printk( KERN_ERR "opt == 0x%08x\n", regs.opt );
                     printk( KERN_ERR "src == 0x%08x\n", regs.src );
                     printk( KERN_ERR "a_b_cnt == 0x%08x\n", regs.a_b_cnt );
                     printk( KERN_ERR "dst == 0x%08x\n", regs.dst );
                     printk( KERN_ERR "src_dst_bidx == 0x%08x\n", regs.src_dst_bidx );
                     printk( KERN_ERR "link_bcntrld == 0x%08x\n", regs.link_bcntrld );
                     printk( KERN_ERR "src_dst_cidx == 0x%08x\n", regs.src_dst_cidx );
                     printk( KERN_ERR "ccnt == 0x%08x\n", regs.ccnt );
                     rval = -EFAULT ;
			}

			return rval ;
		}
		default:
			break ;
	}
	return -EINVAL;
}

static int dav_dma_mmap(struct file * file, struct vm_area_struct * vma)
{
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO|VM_RESERVED;
	vma->vm_pgoff = pool_phys >> PAGE_SHIFT;
	return remap_pfn_range( vma, vma->vm_start, vma->vm_pgoff, pool_size, vma->vm_page_prot);
}

static ssize_t dav_dma_read (struct file *filp, char *buffer,size_t count, loff_t *ppos)
{
	if( pool_data && ppos && (*ppos < pool_size) ){
		unsigned left = pool_size-*ppos ;
		if( left < count )
			count = left ;
		if( copy_to_user( buffer, pool_data+*ppos, count ) ){
			return -EFAULT ;
		}
		*ppos += count ;
		return count ;
	}
	else
                return -EIO ;
}

struct file_operations dav_dma_fops = {
	.owner = THIS_MODULE,
	.ioctl = dav_dma_ioctl,
	.open = dav_dma_open,
	.release = dav_dma_release,
	.read = dav_dma_read,
	.mmap = dav_dma_mmap
};

static void dav_dma_setup_cdev(struct dav_dma_dev_t *dev)
{
	int err, devno = MKDEV(dav_dma_major, dav_dma_minor );
	cdev_init(&dev->cdev, &dav_dma_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &dav_dma_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding dav_dma", err);
}

static int dav_dma_setup(char *options)
{
	char *this_opt;
	int rval = 0 ;
   printk( KERN_ERR "%s options=%s\n", __FUNCTION__, options );

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if( 0 == strncmp("pool=",this_opt,5) ){
			pool_size = PAGE_ALIGN(simple_strtoul(this_opt+5,0,0));
			printk( KERN_ERR "pool size == 0x%x, order 0x%x\n", pool_size, get_order(pool_size) );
			pool_size = (1<<get_order(pool_size))<<PAGE_SHIFT ;
		} else if( 0 == strncmp("phys=", this_opt, 5) ){
			char* p;
			char const *optval = this_opt+5 ;
			printk( KERN_ERR "%s: phys (%s)", __func__, optval );
			pool_physaddr=simple_strtoul(optval, &p, 0);
			if(p && (*p=='M')){ 
			   pool_physaddr = (pool_physaddr<<20)+0x80000000;
			   p++ ;
			}
			if( p && ('+' == *p) ){
				p++ ;
				phys_size =simple_strtoul(p,&p,0);
				if(p && ('M'==*p)){
					phys_size <<= 20 ;
				}
				printk( KERN_ERR "%s: 0x%x, size 0x%x\n", __func__, pool_physaddr, phys_size );
			}
			else
				printk( KERN_ERR "%s: phys=0x%x, no size\n", __func__, pool_physaddr );
		}
		else if( *this_opt ){
			printk( KERN_ERR "Unknown option %s\n", this_opt );
			rval = -1 ;
			break;
		}
	}

	return 0 ;
}

static char *options = "";
module_param(options, charp, S_IRUGO);

#ifndef MODULE
static int __init save_options(char *args)
{
	if (!args || !*args)
		return 0;
   options=args ;

	return 0;
}
__setup("dav-dma=", save_options);
#endif

static int dav_dma_init(void)
{
	int result ;
	DEBUGMSG( "%s\n", __FUNCTION__);
	if (dav_dma_major) {
		int dev = MKDEV(dav_dma_major, dav_dma_minor);
		result = register_chrdev_region(dev, dav_dma_nr_devs, dav_dma_name);
	} else {
		int dev ;
		result = alloc_chrdev_region(&dev, dav_dma_minor, dav_dma_nr_devs, dav_dma_name);
		dav_dma_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "dav_dma: can't get major %d\n", dav_dma_major);
		return result ;
	}

	printk( KERN_INFO "registered chrdrv %s: %u\n", dav_dma_name, dav_dma_major );
	dav_dma_setup(options);
	if( 0 < pool_size ){
		pool_data = dma_alloc_coherent( 0, pool_size, &pool_phys, GFP_KERNEL | GFP_DMA );
		printk( KERN_ERR "dma_alloc: %p (phys %p)\n", pool_data, (void *)pool_phys );
//dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *handle, int gfp)
      // pool_data = (void *)__get_free_pages(GFP_KERNEL|GFP_DMA,get_order(pool_size));
	}
        else if( (0<pool_physaddr) && (0 < phys_size) ){
           pool_data = ioremap(pool_physaddr, phys_size);
	   if( pool_data ){
		pool_phys = pool_physaddr ;
		pool_size = phys_size ;
		printk( KERN_ERR "remapped 0x%x+0x%x to 0x%x\n", pool_physaddr, phys_size, pool_data );
	   } else
		   printk( KERN_ERR "%s: Error remapping 0x%x (%u)\n", __func__, pool_physaddr, phys_size );
        } else
		printk( KERN_INFO "%s: no memory pool allocated: 0x%x 0x%x\n", __FUNCTION__, pool_physaddr, phys_size );

	if( pool_data ){
		mem_pool = init_memory_pool(pool_size, pool_data);
		printk( KERN_ERR "memory pool header at %p\n", mem_pool );
	}
	else
		printk( KERN_ERR "Error allocating pool of 0x%x bytes\n", pool_size );
	if( 0 == ( result = davinci_request_dma(DAVINCI_DMA_CHANNEL_ANY, dav_dma_name, dav_dma_callback, 0, &dmach, &tcc, EVENTQ_1)) ){
		printk( KERN_ERR "%s: DMA channel %d, tcc %d\n", __FUNCTION__, dmach, tcc );
		tcc <<= 12 ;
		init_completion(&dma_completion);
	} else {
		printk( KERN_ERR "%s: error %d requesting DMA\n", __FUNCTION__, result );
		return -EFAULT ;
	}
	
	dav_dma_setup_cdev(&dav_dma_dev);
	dav_dma_class = class_create(THIS_MODULE, dav_dma_name);
	device_create(dav_dma_class, NULL,
			MKDEV(dav_dma_major, dav_dma_minor), NULL, "dav-dma-%d", 0);

	return result ;
}

static void dav_dma_exit(void)
{
	DEBUGMSG( "%s\n", __FUNCTION__);
	if( pool_data ){
//		free_pages( (unsigned long)pool_data, get_order(pool_size) );
		if( 0 == pool_physaddr ){
			dma_free_coherent(NULL, pool_size, (void *)pool_data, pool_phys );
		} // dynamically allocated
		else {
			iounmap(pool_data);
		}
	}
        if( 0 <= dmach ){
           davinci_free_dma(dmach);
           printk( KERN_ERR "%s: free dma channel %d\n", __FUNCTION__, dmach );
           dmach = -1 ;
        }
        device_destroy(dav_dma_class,MKDEV(dav_dma_major, dav_dma_minor));
        class_destroy(dav_dma_class);
	cdev_del(&dav_dma_dev.cdev);
	unregister_chrdev_region(MKDEV(dav_dma_major, dav_dma_minor), dav_dma_nr_devs);
}
module_init(dav_dma_init);
module_exit(dav_dma_exit);

MODULE_AUTHOR("Boundary Devices <info@boundarydevices.com>");
MODULE_DESCRIPTION("DaVinci EDMA driver");
MODULE_LICENSE("GPL");
