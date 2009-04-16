//
//	Boundary Devices Davinci GPIO Interrupt counter
//      
//      This module will count interrupts on a GPIO pin specified through
//      an ioctl call. It will report the interrupt count through:
//
//          /proc/dav_gpio
//
//          read() - will return an 8-byte structure containing a 
//                jiffy count and the interrupt count.
//
//	Copyright (C) Eric Nelson <eric.nelson@boundarydevices.com> 2009.
//

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <mach/irqs.h>
#include <linux/gpio_int_davinci.h>
#include <linux/irq.h>

#define DRIVER_NAME "dav_gpio_int"

struct jiffies_and_count {
	unsigned jiffies ;
	unsigned count ;
};

#define READMODE	0
#define	TRIGGERMODE	1

struct driver_data {
	unsigned		mode ;
	unsigned 		take_from ;	// entry into timer_data
	unsigned		threshold ;	// report anything >= this value in trigger mode 
};

int gpio_major = 0;

module_param(gpio_major, uint,0);
MODULE_PARM_DESC(gpio_major, "Choose major device number");

int gpio_number = 12 ;
module_param(gpio_number, uint,0);
MODULE_PARM_DESC(gpio_number, "Choose GPIO number");

static atomic_t counter = ATOMIC_INIT(0);

#define MAX_TIMERDATA 128

struct timer_data_t {
	atomic_t			addto ;
	unsigned			prev_count ;
	struct jiffies_and_count	data[MAX_TIMERDATA];
	wait_queue_head_t 		wait_queue ; // Used for blocking read, poll
};

static struct timer_data_t timer_data ;
static struct timer_list timer = TIMER_INITIALIZER(0,0,(unsigned long)&timer_data);

// open function - called when the "file" /dev/gpio is opened in userspace
static int gpio_open (struct inode *inode, struct file *file) {
	struct driver_data *data = kmalloc(sizeof(struct driver_data),GFP_KERNEL);
        file->private_data = data ;
	memset(file->private_data,0,sizeof(*data));
	return 0;
}

// close function - called when the "file" /dev/gpio is closed in userspace  
static int gpio_release (struct inode *inode, struct file *file) {
        if(file->private_data){
		kfree(file->private_data);
                file->private_data = 0 ;
	}
	return 0;
}

//
// skip input data if wrapped
// returns number of items available
//
static unsigned skip( struct driver_data *data )
{
	unsigned next = data->take_from ;
	unsigned addto = atomic_read(&timer_data.addto);
	unsigned num_avail = addto-next ;
	if( num_avail >= MAX_TIMERDATA ){
		num_avail = (MAX_TIMERDATA/2);
		data->take_from = addto - num_avail ;
	} // missed something
	return num_avail ;
}

// read function called when from /dev/gpio is read
static ssize_t gpio_read (struct file *file, char *buf,
		size_t space, loff_t *ppos) 
{
	int numread = 0 ;
        struct driver_data *data = (struct driver_data *)file->private_data ;
	do {
		unsigned distance = skip(data);
		unsigned next = data->take_from ;

		while ((0 < distance) && (sizeof(struct jiffies_and_count) <= space)) {
                        unsigned curpos = (next%MAX_TIMERDATA);
			struct jiffies_and_count *entry = timer_data.data+curpos ;
			next++ ;
                        data->take_from = next ;
			distance-- ;
			if( (READMODE == data->mode) || (entry->count > data->threshold) ){
				if ( copy_to_user(buf,entry,sizeof(struct jiffies_and_count)) ) {
					numread = -EFAULT ;
					break;
				} else {
					numread += sizeof(struct jiffies_and_count);
					space -= sizeof(struct jiffies_and_count);
					buf += sizeof(struct jiffies_and_count);
				}
			} else
				break;
		}
		if(0 == numread){
			if(0==(file->f_flags & O_NONBLOCK)){
				if( wait_event_interruptible(timer_data.wait_queue, next!=atomic_read(&timer_data.addto)) )
					break;
			}
			else
				break;
		}
		else if(0 < numread) {
			break;
		}
	} while( 1 );
	
        return numread ;
}

static unsigned int gpio_poll(struct file *filp, poll_table *wait)
{
	struct driver_data *data = (struct driver_data *)filp->private_data ;
	if( data ){
		int rval = 0 ;
		poll_wait(filp, &timer_data.wait_queue, wait);
		if( READMODE == data->mode ){
			if(data->take_from != atomic_read(&timer_data.addto)) 
				rval = POLLIN | POLLRDNORM ;
		} else {
			unsigned count = skip(data);
			while( 0 < count-- ){
				unsigned curpos = (data->take_from%MAX_TIMERDATA);
				struct jiffies_and_count *entry = timer_data.data+curpos ;
				if (entry->count > data->threshold) {
					rval = POLLIN | POLLRDNORM ;
					break;
				}
				data->take_from++ ;
			}
		}
		return rval ;
	}
	else
		return -EINVAL ;
}

// ioctl - I/O control
static int gpio_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg) 
{
	int retval = -EINVAL ;
        struct driver_data *data = (struct driver_data *)file->private_data ;
	switch (cmd) {
		case GPIO_INT_DAVINCI_THRESHOLD: {
			unsigned threshold ;
			if( copy_from_user( &threshold, (void __user *)arg, sizeof(threshold) ) )
                                return -EFAULT;
			data->mode = TRIGGERMODE ;
			data->threshold = threshold ;

			break;
		}
		case GPIO_INT_DAVINCI_READMODE: {
			data->mode = READMODE ;
			break;
		}
	}
	return retval ;
}

// interrupt handler
static int interrupt_handler(int irq, void *data)
{
	atomic_inc(&counter);
	return IRQ_HANDLED;
}

// define which file operations are supported
struct file_operations gpio_fops = {
	.owner		=	THIS_MODULE,
	.read		=	gpio_read,
	.ioctl		=	gpio_ioctl,
	.open		=	gpio_open,
	.poll		=	gpio_poll,
	.release	=	gpio_release,
};

void timer_fn(unsigned long arg)
{
	struct timer_data_t *data = &timer_data ;
	unsigned long j = jiffies ;
	unsigned long c = atomic_read(&counter);
	unsigned next = atomic_read(&data->addto)%MAX_TIMERDATA ;
	data->data[next].jiffies = j ;
	data->data[next].count = c-data->prev_count ;
	atomic_inc(&data->addto);
        data->prev_count = c ;
	timer.expires=j+1 ;
        add_timer(&timer);
	wake_up_interruptible(&data->wait_queue);
}

// initialize module (and interrupt)
static int __init gpio_init_module (void) {
	int i;
	
	i = register_chrdev (gpio_major, DRIVER_NAME, &gpio_fops);
	if (i < 0){
		printk( KERN_ERR "%s: error registering char device %s:%d\n", __func__, DRIVER_NAME, gpio_major );
		return - EIO;
	}
	if( 0 == gpio_major )
		gpio_major = i ;
	
        if (request_irq( IRQ_GPIO(gpio_number), interrupt_handler, 
			 IRQF_DISABLED, "Davinci gpio pin", 0)<0) {
		printk(KERN_ERR __FILE__ ": irq %d unavailable\n\r", IRQ_GPIO(gpio_number));
	}
	set_irq_type(IRQ_GPIO(12), IRQ_TYPE_EDGE_FALLING);

	memset(&timer_data,0,sizeof(timer_data));
        init_waitqueue_head(&timer_data.wait_queue);

	timer.expires = jiffies+1 ;
	timer.function = timer_fn ;
	add_timer(&timer);
	return 0;
}

// close and cleanup module
static void __exit gpio_cleanup_module (void) {
	del_timer_sync(&timer);
        free_irq(IRQ_GPIO(gpio_number),0);
	unregister_chrdev (gpio_major, DRIVER_NAME);
}

module_init(gpio_init_module);
module_exit(gpio_cleanup_module);
MODULE_AUTHOR("eric.nelson@boundarydevices.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO interrupt counter for Davinci platform");

