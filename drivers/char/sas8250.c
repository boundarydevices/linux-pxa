//
//	Boundary Devices SAS 8250 driver
//      
//      This module will produce a message-oriented interface to
//	a character device by interpreting small bits of the IGT
//	SAS protocol. The SAS protocol is a 9-bit protocol using 
//	the parity bit as an address indicator (parity set on 
//	address byte). A message begins with an address byte and
//	ends with either an inter-character gap of at least 5ms
//	or an address byte.
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
#include <linux/irq.h>
#include <mach/gpio.h>
#include <asm/atomic.h>
#include <linux/serial_reg.h>
#include <linux/proc_fs.h>
#include <linux/hrtimer.h>
#include <mach/hardware.h>

#define DRIVER_NAME "sas_8250"

struct user_data ;

struct sas8250_dev {
	unsigned irq ;
	unsigned io_address ;
	atomic_t use_count ;
	unsigned interrupts ;
	unsigned parity_errs ;
	unsigned framing_errs ;
	unsigned overruns ;
	unsigned breaks ;
	struct sas8250_dev *next ;
        struct user_data *user ;
};

static struct sas8250_dev *dev_head = 0 ;
static struct sas8250_dev *dev_tail = 0 ;

#define MAX_CHARS	4096
#define CHAR_MASK	(MAX_CHARS-1)

#define	MAX_MSGLEN	256
#define MAX_MSGS	((MAX_CHARS)/MAX_MSGLEN)
#define MSG_MASK	((MAX_MSGS)-1)

struct user_data {
        struct sas8250_dev	*dev ;
        struct hrtimer 		timer ;
	wait_queue_head_t 	wait_queue ; // Used for blocking read, poll
	u32 *__iomem		regs ;
	unsigned		msg_add ;
	unsigned		msg_take ;
	unsigned		char_add ;
	unsigned		char_take ;
	u8			*chars ;
	u32			*msgs ;		// low 16 start, high 16 end
};

#define USERDATA_SIZE	(sizeof(struct user_data)+MAX_CHARS+(MAX_MSGS*sizeof(u32)))

#define UART_DLL_DED	(0x20/sizeof(u32))
#define UART_DLH_DED	(0x24/sizeof(u32))
#define UART_PID0	(0x28/sizeof(u32))
#define UART_PID1	(0x2C/sizeof(u32))
#define UART_PWREMU	(0x30/sizeof(u32))

#define IDLE_PERIOD	(5 * 1000000)	/* ns delay between chars */

static int sas8250_major = 0;

module_param(sas8250_major, uint,0);
MODULE_PARM_DESC(sas8250_major, "Choose major device number");

static char *sas8250_devs = NULL ;
module_param(sas8250_devs, charp,0);
MODULE_PARM_DESC(sas8250_devs, "Specify SAS devices");


static u32 read_8250_reg(u32 ioaddress, unsigned offs)
{
	return __raw_readl(IO_ADDRESS(ioaddress)+(offs*sizeof(u32)));
}

static void write_8250_reg(u32 ioaddress, unsigned offs, u32 value)
{
        __raw_writel(value,IO_ADDRESS(ioaddress)+(offs*sizeof(u32)));
}

static void end_msg(struct user_data *usr)
{
	unsigned msgIdx = usr->msg_add & MSG_MASK ;
	unsigned endChar = usr->char_add & CHAR_MASK ;

	usr->msgs[msgIdx] |= (endChar<<16);
        usr->msg_add++ ;

	msgIdx = (msgIdx+1)&MSG_MASK ;
	usr->msgs[msgIdx] = endChar ;

        wake_up_interruptible(&usr->wait_queue);
}

static irqreturn_t interrupt_handler(int irq, void *data)
{																		 
        struct user_data *usr = (struct user_data *)data ;
        struct sas8250_dev *dev = usr->dev ;

	if( usr && dev ){
		dev->interrupts++ ;
		do {
			u32 lsr ;
			u32 iir = read_8250_reg(dev->io_address,UART_IIR);
			if( iir & UART_IIR_NO_INT)
				break;
			lsr = read_8250_reg(dev->io_address,UART_LSR);
			if( lsr & UART_LSR_PE ){
				end_msg(usr);
				dev->parity_errs++ ;
			}
			if( lsr & UART_LSR_DR ){
				u8 byte = read_8250_reg(dev->io_address,UART_RX);
				unsigned add = usr->char_add++ ;
				add &= CHAR_MASK ;
				usr->chars[add] = byte ;
                                hrtimer_start(&usr->timer, ktime_set(0, IDLE_PERIOD), HRTIMER_MODE_REL);
			}
			if( lsr & UART_LSR_PE )
			if( lsr & UART_LSR_FE )
				dev->framing_errs++ ;
			if( lsr & UART_LSR_OE )
				dev->overruns++ ;
			if( lsr & UART_LSR_BI )
				dev->breaks++ ;
		} while( 1 );
	}
	return IRQ_HANDLED;
}

static enum hrtimer_restart rx_timeout(struct hrtimer *hrtimer)
{
	struct user_data *usr = container_of(hrtimer, struct user_data, timer);
	end_msg(usr);
	return HRTIMER_NORESTART;
}

static int sas8250_open (struct inode *inode, struct file *file) {
	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
        struct sas8250_dev *dev = dev_head ;
	while( dev && (0 < minor) ){
		dev = dev->next ;
		minor-- ;
	}
	if( dev ) {
		int uses = atomic_inc_return(&dev->use_count);
		if( 1 == uses ){
			u32 pid0, pid1 ;
                        struct user_data *usr = (struct user_data *)kzalloc(USERDATA_SIZE,GFP_KERNEL);
			usr->dev = dev ;
                        hrtimer_init(&usr->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
                        init_waitqueue_head(&usr->wait_queue);
                        usr->timer.function = rx_timeout ;
			usr->chars = ((u8 *)usr)+sizeof(*usr);
			usr->msgs  = (u32 *)(usr->chars+MAX_CHARS);
                        usr->regs = ioremap(dev->io_address,PAGE_SIZE);
			printk( KERN_ERR "%s: %u 0x%x %u uses (%p) (%p)\n", __func__, dev->irq, dev->io_address, uses, usr, usr->regs );
			pid0 = read_8250_reg(dev->io_address,UART_PID0);
                        pid1 = read_8250_reg(dev->io_address,UART_PID1);
			printk( KERN_ERR "PID 0x%08x 0x%08x\n", pid0, pid1 );
			write_8250_reg(dev->io_address,UART_LCR,0x3B);		// space parity
			write_8250_reg(dev->io_address,UART_DLL_DED,0x58);		// 19200 baud
			write_8250_reg(dev->io_address,UART_DLH_DED,0x00);
			if( 0 > request_irq( dev->irq, interrupt_handler, IRQF_DISABLED, DRIVER_NAME, usr)) {
				printk(KERN_ERR __FILE__ ": irq %d unavailable\n\r", dev->irq );
				kfree(usr);
				return -EIO ;
			}
                        write_8250_reg(dev->io_address,UART_IER,0x01);
			file->private_data = usr ;
			dev->user = usr ;
			return 0;
		}
		else {
			atomic_dec(&dev->use_count);
			return -EBUSY ;
		}
	} else {
		return -EINVAL ;
	}
}

static int sas8250_release (struct inode *inode, struct file *file) {
        struct user_data *usr = (struct user_data *)file->private_data ;
	if( (0 != usr) && (0 != usr->dev) ){
		int numLeft = atomic_dec_return(&usr->dev->use_count);
		printk( KERN_ERR "%s: %u (%p)\n", __func__, numLeft, usr );
                hrtimer_cancel(&usr->timer);
		if( 0 != numLeft ){
			printk( KERN_ERR "%s: invalid use count %d\n", __func__, numLeft );
			return 0 ;
		}
		usr->dev->user = 0 ;
                iounmap(usr->regs);
                free_irq(usr->dev->irq,usr);
		kfree(usr);
	}
	return 0;
}

static ssize_t sas8250_read (struct file *file, char *buf,
		size_t space, loff_t *ppos) 
{
	int numread = 0 ;
        struct user_data *usr = (struct user_data *)file->private_data ;
	do {
		unsigned avail = (usr->msg_add-usr->msg_take) & MSG_MASK ;
		if( avail ){
			unsigned msgIdx = (usr->msg_take++) & MSG_MASK ;
			u32 msg = usr->msgs[msgIdx];
			unsigned cstart = msg & 0xFFFF ;
			unsigned cend   = msg >> 16 ;
			unsigned len = (cend-cstart) & CHAR_MASK ;
//			printk( KERN_ERR "%u: %u..%u (%u)\n", msgIdx, cstart, cend, len );
			if( space >= len ){
			}
			else
				numread = -EMSGSIZE ;
			numread = len ;
			if( cstart <= cend ){
				if( copy_to_user( buf,usr->chars+cstart,len) )
					numread = -EFAULT ;
			} else {
				unsigned len0 = MAX_CHARS - cstart ;
				if( copy_to_user(buf,usr->chars+cstart,len0)
				    ||
                                    copy_to_user(buf+len0,usr->chars,cend) )
					numread = -EFAULT ;
			} // two-piece copy
			break;
		} else if(0==(file->f_flags & O_NONBLOCK)){
			if( wait_event_interruptible(usr->wait_queue, (usr->msg_add != usr->msg_take)) ){
				numread = -EINTR ;
				break;
			}
		} else 
			break;
	} while( 1 );

        return numread ;
}

static unsigned int sas8250_poll(struct file *file, poll_table *wait)
{
        struct user_data *usr = (struct user_data *)file->private_data ;
	if( usr ){
		int rval = 0 ;
		poll_wait(file, &usr->wait_queue, wait);
		if( usr->msg_add != usr->msg_take ){
			rval = POLLIN | POLLRDNORM ;
		}
		return rval ;
	}
	else
		return -EINVAL ;
}

static int sas8250_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg) 
{
	int retval = -EINVAL ;
	printk( KERN_ERR "%s: cmd 0x%x, arg 0x%lx\n", __func__, cmd, arg );
	return retval ;
}

struct file_operations sas8250_fops = {
	.owner		=	THIS_MODULE,
	.read		=	sas8250_read,
	.ioctl		=	sas8250_ioctl,
	.open		=	sas8250_open,
	.poll		=	sas8250_poll,
	.release	=	sas8250_release,
};

static char const devusage[] = {
	KERN_ERR "Usage: irq,0xaddress"
};

static void parse_devs( char *devstring )
{
	char *token ;
	while( 0 != (token=strsep(&devstring,":")) ){
                struct sas8250_dev *addr ;
		char *part ;
		part=strsep(&token,",");
		if( 0 == part ){
			printk(devusage); return ;
		}
		addr = kzalloc(sizeof(*addr),GFP_KERNEL);
		addr->next = 0 ;
		addr->irq = simple_strtoul(part,0,0);
		part=strsep(&token,",");
		if( 0 == part ){
			printk(devusage); 
			kfree(addr);
			return ;
		}
		addr->io_address = simple_strtoul(part,0,0);
		if(dev_tail)
			dev_tail->next = addr ;
		else
			dev_head = addr ;
		dev_tail = addr ;
	}
}

static int read_proc_dev(char *page, int count, struct sas8250_dev *dev )
{
	int start_count = count ;
	int numWritten = 0 ;
	int uses = atomic_read(&dev->use_count);
	
	numWritten = snprintf(page, count, "%p: irq %u, addr 0x%x, %u uses %p\n", dev, dev->irq, dev->io_address, uses, dev->user );
	page += numWritten ; count -= numWritten ;
	numWritten = snprintf(page, count, "   %u ints, PE %u, FE %u, OE %u, BI %u\n", dev->interrupts, dev->parity_errs, dev->framing_errs, dev->overruns, dev->breaks );
	page += numWritten ; count -= numWritten ;
	if( (0 < uses) && (0 != dev->user) ){
                struct user_data *usr = dev->user ;
		numWritten = snprintf(page, count, "  chars: add %u, take %u\n", usr->char_add, usr->char_take );
                page += numWritten ; count -= numWritten ;
		numWritten = snprintf(page, count, "  msgs:  add %u, take %u\n", usr->msg_add, usr->msg_take );
                page += numWritten ; count -= numWritten ;
	}

	return start_count - count ;
}

static int sas_read_proc( char *page, char **start, off_t off,
                            int count, int *eof, void *data )
{
	int rval = 0 ;
	struct sas8250_dev *dev = dev_head ;
	while( (0 != dev) && (0 < count) ){
		int numWritten = read_proc_dev(page, count, dev);
                rval += numWritten ;
		count -= numWritten ;
		page += numWritten ;
		dev = dev->next ;
	}
	*eof = 1 ;
	return rval ;
}

#define PROC_NAME "sas"

static int __init sas8250_init_module (void) {
	printk( KERN_ERR "%s: devices: %s\n", __func__, sas8250_devs );
	if( sas8250_devs )
                parse_devs(sas8250_devs);
	if( 0 == dev_head )
		printk( KERN_ERR "%s: No devices defined\n", __func__ );
	else {
                struct sas8250_dev *addr = dev_head ;
		int i = 0 ;
		while( addr ){
			printk( KERN_ERR "[%u] == %u 0x%x\n", i++, addr->irq, addr->io_address );
			addr = addr->next ;
		}

		i = register_chrdev (sas8250_major, DRIVER_NAME, &sas8250_fops);
		if (i < 0){
			printk( KERN_ERR "%s: error registering char device %s:%d\n", __func__, DRIVER_NAME, sas8250_major );
			return - EIO;
		}
                if( 0 == sas8250_major ){
			struct proc_dir_entry *pde ;
			printk( KERN_ERR "%s: registered as major number %u\n", DRIVER_NAME, i );
			sas8250_major = i ;
			pde = create_proc_entry(PROC_NAME, 0, 0);
			if( pde ) {
				pde->read_proc  = sas_read_proc ;
			}
			else
				printk( KERN_ERR "%s: Error creating proc entry\n", __func__ );
		}
	}
	return 0;
}

// close and cleanup module
static void __exit sas8250_cleanup_module (void) {
	printk( KERN_ERR "%s\n", __func__ );
	unregister_chrdev (sas8250_major, DRIVER_NAME);
        remove_proc_entry(PROC_NAME, 0 );
}

module_init(sas8250_init_module);
module_exit(sas8250_cleanup_module);
MODULE_AUTHOR("eric.nelson@boundarydevices.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SAS8250");

