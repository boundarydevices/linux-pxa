/*
 *   Boundary Devices PIC16F616 touch screen controller.
 *   
 *   Copyright (c) by Troy Kisky<troy.kisky@boundarydevices.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/gpio.h>

/*
 * Define this if you want to talk to the input layer
 */
#undef CONFIG_INPUT
#ifdef CONFIG_INPUT
#define USE_INPUT
#else
#undef USE_INPUT
#endif

#ifndef USE_INPUT
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
struct ts_event {
	u16		pressure;
	u16		x;
	u16		y;
	u16		pad;
	struct timeval	stamp;
};
#define NR_EVENTS	16

#else
#include <linux/input.h>
#endif

struct pic16f616_ts {
	struct i2c_client client;
#ifdef USE_INPUT
	struct input_dev	*idev;
#endif
#ifndef USE_INPUT
	struct fasync_struct	*fasync;
	wait_queue_head_t	read_wait;
	u8			evt_head;
	u8			evt_tail;
	struct ts_event		events[NR_EVENTS];
#endif
	wait_queue_head_t	sample_waitq;
	struct semaphore	sem;
	struct completion	init_exit;	//thread exit notification
	struct task_struct	*rtask;
	int			use_count;
	int		bReady;
	int		interruptCnt;
#ifdef TESTING
	struct timeval	lastInterruptTime;
#endif
	int irq;
};
const char *client_name = "Pic16F616-ts";

/* I2C Addresses to scan */
static unsigned short normal_i2c[] = { 0x22,I2C_CLIENT_END };

/* This makes all addr_data:s */
I2C_CLIENT_MODULE_PARM(probe,
	"List of adapter,address pairs to scan additionally");
I2C_CLIENT_MODULE_PARM(ignore,
	"List of adapter,address pairs not to scan");
//I2C_CLIENT_MODULE_PARM(force,
//	"List of adapter,address pairs to boldly assume "
//	"to be present");
static struct i2c_client_address_data addr_data = {
	.normal_i2c =           normal_i2c,
	.probe =                probe,
	.ignore =               ignore,
};
struct pic16f616_ts* gts;

static int ts_startup(struct pic16f616_ts* ts);
static void ts_shutdown(struct pic16f616_ts* ts);

/*-----------------------------------------------------------------------*/
#ifndef USE_INPUT

#define ts_evt_pending(ts)	((volatile u8)(ts)->evt_head != (ts)->evt_tail)
#define ts_evt_get(ts)		((ts)->events + (ts)->evt_tail)
#define ts_evt_pull(ts)		((ts)->evt_tail = ((ts)->evt_tail + 1) & (NR_EVENTS - 1))
#define ts_evt_clear(ts)	((ts)->evt_head = (ts)->evt_tail = 0)

static inline void ts_evt_add(struct pic16f616_ts *ts, u16 pressure, u16 x, u16 y)
{
	int next_head;

	do {
		rmb();
		next_head = (ts->evt_head + 1) & (NR_EVENTS - 1);
		if (next_head != ts->evt_tail) break;
		wake_up_interruptible(&ts->read_wait);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ / 20);
	} while (1);


	ts->events[ts->evt_head].pressure = pressure;
	ts->events[ts->evt_head].x = x;
	ts->events[ts->evt_head].y = y;
	do_gettimeofday(&ts->events[ts->evt_head].stamp);
	ts->evt_head = next_head;

	if (ts->fasync) kill_fasync(&ts->fasync, SIGIO, POLL_IN);
	wake_up_interruptible(&ts->read_wait);
}

static inline void ts_event_release(struct pic16f616_ts *ts)
{
	ts_evt_add(ts, 0, 0, 0);
}

/*
 * User space driver interface.
 */
static ssize_t picts_read(struct file *filp, char *buffer, size_t count,
		loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	struct pic16f616_ts *ts = filp->private_data;
	char *ptr = buffer;
	int err = 0;

	add_wait_queue(&ts->read_wait, &wait);
	while (count >= sizeof(struct ts_event)) {
		err = -ERESTARTSYS;
		if (signal_pending(current))
			break;

		if (ts_evt_pending(ts)) {
			struct ts_event *evt = ts_evt_get(ts);

			err = copy_to_user(ptr, evt, sizeof(struct ts_event));
			ts_evt_pull(ts);

			if (err)
				break;

			ptr += sizeof(struct ts_event);
			count -= sizeof(struct ts_event);
			continue;
		}

		__set_current_state(TASK_INTERRUPTIBLE);
		if (filp->f_flags & O_NONBLOCK) {
			err = -EAGAIN;
			break;
		}
		schedule();
	}
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&ts->read_wait, &wait);
 
//	printk(KERN_ERR "%s: ts_read finished\n", client_name);
	return (ptr == buffer)? err : ptr - buffer;
}

static unsigned int picts_poll(struct file *filp, poll_table *wait)
{
	struct pic16f616_ts *ts = filp->private_data;
	int ret = 0;

	poll_wait(filp, &ts->read_wait, wait);
	if (ts_evt_pending(ts))
		ret = POLLIN | POLLRDNORM;

	return ret;
}

static int picts_fasync(int fd, struct file *filp, int on)
{
	struct pic16f616_ts *ts = filp->private_data;

//	printk(KERN_ERR "%s: ts_fasync called\n", client_name);
	return fasync_helper(fd, filp, on, &ts->fasync);
}

static int picts_open(struct inode *inode, struct file *filp)
{
	struct pic16f616_ts *ts = gts;
	int ret = 0;
	ret = ts_startup(ts);
	if (ret==0) filp->private_data = ts;
	return ret;
}

/*
 * Release touchscreen resources.  Disable IRQs.
 */
static int picts_release(struct inode *inode, struct file *filp)
{
	struct pic16f616_ts *ts = filp->private_data;
	if (ts) {
		down(&ts->sem);
		picts_fasync(-1, filp, 0);
		up(&ts->sem);
		ts_shutdown(ts);
	}
//	printk(KERN_ERR "%s: ts_release finished\n", client_name);
	return 0;
}

static struct file_operations pic16f616_ts_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= picts_read,
	.poll		= picts_poll,
	.open		= picts_open,
	.release	= picts_release,
	.fasync		= picts_fasync,
};

/*
 * The touchscreen is a miscdevice:
 *   10 char        Non-serial mice, misc features
 *   14 = /dev/touchscreen/pic16f616_ts touchscreen
 */
static struct miscdevice pic16f616_ts_dev = {
	minor:	14,
	name:	"pic16f616_ts",
	fops:	&pic16f616_ts_fops,
};

static inline int ts_register(struct pic16f616_ts *ts)
{
	init_waitqueue_head(&ts->read_wait);
	return misc_register(&pic16f616_ts_dev);
}

static inline void ts_deregister(struct pic16f616_ts *ts)
{
	misc_deregister(&pic16f616_ts_dev);
}

/*-----------------------------------------------------------------------*/
#else
/*-----------------------------------------------------------------------*/

#define ts_evt_clear(ts)	do { } while (0)

//////////////////////////
static inline void ts_evt_add(struct pic16f616_ts *ts, u16 pressure,
		u16 x, u16 y)
{
	struct input_dev *idev = ts->idev;
	input_report_abs(idev, ABS_X, x);
	input_report_abs(idev, ABS_Y, y);
	input_report_abs(idev, ABS_PRESSURE, pressure);
	input_sync(idev);
}

static inline void ts_event_release(struct pic16f616_ts *ts)
{
	struct input_dev *idev = ts->idev;
	input_report_abs(idev, ABS_PRESSURE, 0);
	input_sync(idev);
}
static int ts_open(struct input_dev *idev)
{
	struct pic16f616_ts *ts = input_get_drvdata(idev);
	return ts_startup(ts);
}

static void ts_close(struct input_dev *idev)
{
	struct pic16f616_ts *ts = input_get_drvdata(idev);
	ts_shutdown(ts);
}

static inline int ts_register(struct pic16f616_ts *ts)
{
	struct input_dev *idev;
	idev = input_allocate_device();
	if (idev==NULL) {
		return -ENOMEM;
	}
	ts->idev = idev;
	idev->name      = "Touchscreen panel";
	idev->id.product = ts->client->id;
	idev->open      = ts_open;
	idev->close     = ts_close;

	__set_bit(EV_ABS, idev->evbit);
	__set_bit(ABS_X, idev->absbit);
	__set_bit(ABS_Y, idev->absbit);
	__set_bit(ABS_PRESSURE, idev->absbit);
	input_set_drvdata(idev, ts);
	return input_register_device(idev);
}

static inline void ts_deregister(struct pic16f616_ts *ts)
{
	if (ts->idev) {
		input_unregister_device(ts->idev);
		input_free_device(ts->idev);
		ts->idev = NULL;
	}
}

#endif
/*-----------------------------------------------------------------------*/
#define MIN_X		0x20
#define MAX_X		0x22
#define MIN_Y		0x24
#define MAX_Y		0x26
#define SUM_X		0x28
#define SUM_Y		0x2b
#define SAMPLE_CNT 	0x30

/*
 * This is a RT kernel thread that handles the I2c accesses
 * The I2c access functions are expected to be able to sleep.
 */
static int ts_thread(void *_ts)
{
	int ret;
	unsigned char buf[32];
	struct pic16f616_ts *ts = _ts;
	unsigned char sumXReg[1] = { SUM_X };
	struct i2c_msg readSums[2] = {
		{ts->client.addr, 0, 1, sumXReg},
		{ts->client.addr, I2C_M_RD, 6, buf}		//read SUM_X & SUM_Y
	};

	struct task_struct *tsk = current;
	unsigned int i;
	unsigned int j = 1<<22; 
	unsigned int pressure = 1;

	ts->rtask = tsk;

	daemonize("pic16f616tsd");
	/* only want to receive SIGKILL */
	allow_signal(SIGKILL);

	/*
	 * We could run as a real-time thread.  However, thus far
	 * this doesn't seem to be necessary.
	 */
//	tsk->policy = SCHED_FIFO;
//	tsk->rt_priority = 1;

	complete(&ts->init_exit);

	ts->interruptCnt=0;
#if 1
	/* davinci i2c has bug with very 1st write after powerup */
	buf[0] = SUM_X;
	i2c_master_send(&ts->client,buf,1);
#endif
	do {
#ifdef TESTING
		msleep(20);
		printk(KERN_ERR "(i2cs)\n");
//		printk(KERN_ERR "%s: reading from device 0x%x\n",client_name,ts->client.addr);
		do_gettimeofday(&ts->lastInterruptTime);
#endif
		ts->bReady = 0;
		/* For the 1st access and after a release, make sure the initial register address is selected */
		ret = (j & (1<<22))?  (i2c_transfer(ts->client.adapter, readSums, 2)+4) : i2c_master_recv(&ts->client,buf,6);
		if (ret != 6) {
			printk(KERN_WARNING "%s: %s failed\n",client_name,(j & (1<<22))? "i2c_transfer" : "i2c_master_recv");
			i = 0;
			j = (1<<23)|(1<<22);
		} else {
			i = buf[0]+ (buf[1]<<8) + (buf[2]<<16);
			j = buf[3]+ (buf[4]<<8) + (buf[5]<<16);
		}

		if (signal_pending(tsk))
			break;

		if (j & (1<<23)) {
			/* sample is valid */
#ifdef TESTING
			printk(KERN_ERR "%s: i=%06x j=%06x\n",client_name,i,j);
#endif
			if (j & (1<<22)) {
				/* this is a release notice */
				ts_event_release(ts);
			} else {
				/* touch is active */
				ts_evt_add(ts, pressure, i, j & 0x3fffff);
			}
		} else {
			printk(KERN_WARNING "%s: sample not valid i=%06x j=%06x interruptCnt=%i\n",client_name,i,j,ts->interruptCnt);
			j = (1<<22);	/* Force register number write to help recovery */
		}
		wait_event_interruptible(ts->sample_waitq, ts->bReady);
		if (signal_pending(tsk))
			break;
	} while (1);

	ts->rtask = NULL;
	ts_evt_clear(ts);
//	printk(KERN_ERR "%s: ts_thread exiting\n",client_name);
	complete_and_exit(&ts->init_exit, 0);
}

/*
 * We only detect samples ready with this interrupt
 * handler, and even then we just schedule our task.
 */
static irqreturn_t ts_interrupt(int irq, void *id)
{
	struct pic16f616_ts *ts = id;
	int bit;
	{
		int gp = IRQ_TO_GPIO(irq);
		struct gpio_controller  *__iomem g = __gpio_to_controller(gp);
		bit = (__raw_readl(&g->in_data) >> (gp&0x1f))&1;
	}
	ts->interruptCnt++;
	if (bit==0) {
		ts->bReady=1;
		wmb();
		wake_up(&ts->sample_waitq);
	}
#ifdef TESTING
	{
		suseconds_t     tv_usec = ts->lastInterruptTime.tv_usec;
		int delta;
		do_gettimeofday(&ts->lastInterruptTime);
		delta = ts->lastInterruptTime.tv_usec - tv_usec;
		if (delta<0) delta += 1000000;
		printk(KERN_WARNING "\n(t%i %i)\n",delta,bit);
	}
#endif
	return IRQ_HANDLED;
}

static int ts_startup(struct pic16f616_ts* ts)
{
	int ret = 0;
	if (ts==NULL) return -EIO;

	if (down_interruptible(&ts->sem))
		return -EINTR;

	if (ts->use_count++ != 0)
		goto out;

	if (ts->rtask)
		panic("pic16f616tsd: rtask running?");

	ret = request_irq(ts->irq, &ts_interrupt, 0, client_name, ts);
	if (ret) {
		printk(KERN_ERR "%s: request_irq failed, irq:%i\n", client_name,ts->irq);
		goto out;
	}

	init_completion(&ts->init_exit);
	ret = kernel_thread(ts_thread, ts, CLONE_KERNEL);
	if (ret >= 0) {
		wait_for_completion(&ts->init_exit);	//wait for thread to Start
		ret = 0;
	} else {
		free_irq(ts->irq, ts);
	}

 out:
	if (ret)
		ts->use_count--;
	up(&ts->sem);
	return ret;
}

/*
 * Release touchscreen resources.  Disable IRQs.
 */
static void ts_shutdown(struct pic16f616_ts* ts)
{
	if (ts) {
		down(&ts->sem);
		if (--ts->use_count == 0) {
			if (ts->rtask) {
				send_sig(SIGKILL, ts->rtask, 1);
				wait_for_completion(&ts->init_exit);
			}
			free_irq(ts->irq, ts);
		}
		up(&ts->sem);
	}
}
/*-----------------------------------------------------------------------*/

static struct i2c_driver ts_driver;

static int ts_detect_client(struct i2c_adapter *adapter, int address, int kind)
{
	int err = 0;
	struct pic16f616_ts* ts;
	if (gts) {
		printk(KERN_ERR "%s: Error gts is already allocated\n",client_name);
		return -ENOMEM;
	}
	if (0) if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_BLOCK_DATA)) {
		printk(KERN_WARNING "%s functionality check failed\n", client_name);
		return err;
	}
	if (!(ts = kzalloc(sizeof(struct pic16f616_ts),GFP_KERNEL))) {
		err = -ENOMEM;
		printk(KERN_WARNING "Couldn't allocate memory for %s\n", client_name);
		return err;
	}
	init_waitqueue_head(&ts->sample_waitq);
	init_MUTEX(&ts->sem);
	ts->client.addr = address;
	ts->client.adapter = adapter;
	ts->client.driver = &ts_driver;
	ts->client.flags = 0;
	strlcpy(ts->client.name, client_name, I2C_NAME_SIZE);
	ts->irq = IRQ_GPIO(11);
	i2c_set_clientdata(&ts->client, ts);
	if ((err = i2c_attach_client(&ts->client))) {
		printk(KERN_WARNING "%s: i2c_attach_client failed\n", client_name);
		kfree(ts);
		return err;
	}
	err = ts_register(ts);
	if (err==0) {
		gts = ts;
	} else {
		printk(KERN_WARNING "%s: ts_register failed\n", client_name);
		ts_deregister(ts);
		kfree(ts);
	}
	return err;
}
	
static int ts_detach_client(struct i2c_client *client)
{
	struct pic16f616_ts* ts = container_of(client,struct pic16f616_ts,client);
	int err;
	if ((err = i2c_detach_client(client))) {
		printk(KERN_WARNING "%s: i2c_detach_client failed\n",client_name);
		return err;
	}
	if (ts==gts) {
		gts = NULL;
		ts_deregister(ts);
	} else {
		printk(KERN_ERR "%s: Error ts!=gts\n",client_name);
	}
	kfree(client);
	return 0;
}

static int ts_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, &ts_detect_client);
}

/*-----------------------------------------------------------------------*/

static struct i2c_driver ts_driver = {
	.driver = {
		.owner		= THIS_MODULE, 
		.name		= "Pic16F616-ts",
	},
	.attach_adapter	= ts_attach_adapter,
	.detach_client	= ts_detach_client,
};

static int __init ts_init(void)
{
	int res;
	struct i2c_client *client = client;
	if ((res = i2c_add_driver(&ts_driver))) {
		printk(KERN_WARNING "%s: i2c_add_driver failed\n",client_name);
		return res;
	}
	printk("%s: version 1.0 (Dec. 24, 2007)\n",client_name);
	return 0;
}

static void __exit ts_exit(void)
{
	i2c_del_driver(&ts_driver);
}

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("I2C interface for Pic16F616 touch screen controller.");
MODULE_LICENSE("GPL");

module_init(ts_init)
module_exit(ts_exit)
