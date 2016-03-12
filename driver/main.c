#include <linux/module.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/sched.h>

//#define SMON_DEBUG		/* Debug Mode (comment to deactivate)*/
#include "smon.h"

#define SMON_MAJOR 	0	/* dynamic major by default */
#define SMON_DEVS	1	/* number of devices */


struct smon_dev {
	atomic_t	count;
	struct cdev	cdev;
};

/*
 * The different configurable parameters
 */
int smon_major	= SMON_MAJOR;
int smon_devs	= SMON_DEVS;	/* number of bare smon devices */

module_param(smon_major, int, 0);
module_param(smon_devs, int, 0);


struct smon_dev smon_device;


static int smon_open(struct inode *inode, struct file *filp)
{
	PDEBUG("open: current = %d\n", current->pid);
	atomic_inc(&smon_device.count);
	return 0;
}


static int smon_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release: current = %d\n", current->pid);
	filp->private_data = NULL;
	atomic_dec(&smon_device.count);
	return 0;
}

static long smon_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct smon_event *pevent;
	struct smon_evset *pevset;
	struct smon_envir *penvir;
	struct smon_ring_buffer *rb = (struct smon_ring_buffer *)filp->private_data;
	struct smon_ioctl *_arg;

	//PDEBUG("ioctl: current = %d\n", current->pid);
	/* don't even decode wrong cmds: better returning  ENOTTY than EFAULT */
	if (_IOC_TYPE(cmd) != SMON_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > SMON_IOC_MAXNR)
		return -ENOTTY;

	/*
	 * the type is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. Note that the type is user-oriented, while
	 * verify_area is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch(cmd) {

		case SMON_IOCRESET:	/* not implemented */
			break;
		case SMON_IOCSEVT:
			_arg = (struct smon_ioctl *) arg;
			pevent = _arg->event;
			if (!pevent) {
				PDEBUG("ioctl_set_event: error: no event provided\n");
				err = -SMEIOCTL;
			}
			else {
				PDEBUG("ioctl_set_event: tag=%s, perfevtsel=%#.8x\n", pevent->tag, (unsigned int)pevent->perfevtsel);
				err = smon_set_event (pevent);
			}
			break;
		case SMON_IOCGEVT:
			_arg = (struct smon_ioctl *) arg;
			PDEBUG("ioctl_get_event: id=%d\n", _arg->id);
			pevent = smon_get_event (_arg->id);
			if (pevent)
				err = copy_to_user(_arg->event, pevent, sizeof(struct smon_event));
			else
				err = -SMENOEVT;
			break;
		case SMON_IOCCEVT:
			PDEBUG("ioctl_check_event: id=%d\n", (unsigned int)arg);
			err = smon_check_event((unsigned int)arg);
			break;
		case SMON_IOCSEVS:
			_arg = (struct smon_ioctl *) arg;
			pevset = _arg->evset;
			if (!pevset) {
				PDEBUG("ioctl_set_evset: error: no evset provided\n");
				err = -SMEIOCTL;
			}
			else {
				PDEBUG("ioctl_set_evset: tag=%s\n", pevset->tag);
				err = smon_set_evset (pevset);
			}
			break;
		case SMON_IOCGEVS:
			_arg = (struct smon_ioctl *) arg;
			PDEBUG("ioctl_get_evset: id=%d\n", _arg->id);
			pevset = smon_get_evset (_arg->id);
			if (pevset)
				err = copy_to_user(_arg->evset, pevset, sizeof(struct smon_evset));
			else
				err = -SMENOEVS;
			break;
		case SMON_IOCCEVS:
			PDEBUG("ioctl_check_evset: id = %d\n", (unsigned int)arg);
			err = smon_check_evset((unsigned int)arg);
			break;
		case SMON_IOCSTSK:
			_arg = (struct smon_ioctl *) arg;
			penvir = _arg->envir;
			if (!penvir) {
				PDEBUG("ioctl_set_task: error: no envir provided\n");
				err = -SMEIOCTL;
			} else if(!rb) {
				PDEBUG("ioctl_set_task: error: no ring buffer (need to mmap)\n");
				err = -SMEIOCTL;
			} else {
				PDEBUG("ioctl_set_task: pid = %d\n", _arg->pid);
				err = smon_sched_register_task(_arg->pid, penvir, rb);
			}
			break;
		case SMON_IOCUTSK:
			_arg = (struct smon_ioctl *) arg;

			PDEBUG("ioctl_unset_task: pid = %d\n", _arg->pid);
			err = smon_sched_unregister_task(_arg->pid);
			break;
		case SMON_IOCREAD:
			_arg = (struct smon_ioctl *) arg;
			//PDEBUG("ioctl_read: %d bytes\n", _arg->size);
			smon_rb_read(rb, _arg->items, _arg->size);
		default:  /* redundant, as cmd was checked against MAXNR */
			return -ENOTTY;
			break;
	}
	
	return err;
}

static unsigned int smon_poll(struct file *filp, poll_table *wait)
{
	struct smon_ring_buffer *rb = filp->private_data;
	unsigned int mask = 0;
	unsigned long flags;

	//PDEBUG("poll: current = %d ... ", current->pid);

	if (!rb)
		return mask;

	smon_rb_lock(rb, &flags);
	
	poll_wait(filp, &rb->inq,  wait);

	//PDEBUG("smon_poll: is empty returns %d\n", smon_rb_is_empty(rb));
	
	if (smon_rb_batch_available(rb)) {
		if (smon_rb_is_active(rb)) {
			mask |= (POLLIN | POLLRDNORM);		/* readable */
			//PDEBUG("smon_poll: POLLIN\n");
		}
		else {
			mask |= POLLHUP;
			//PDEBUG("smon_poll: POLLHUP\n");
		}
	} else if (!smon_rb_is_empty(rb) && !smon_rb_is_active(rb))
		mask |= POLLHUP;
	
	smon_rb_unlock(rb, &flags);

	//PDEBUG("done!\n");

	return mask;
}

/*
 * mmap stuff
 */
void smon_vma_open(struct vm_area_struct *vma)
{
	PDEBUG("smon_vma_open: current = %d\n", current->pid);
}

void smon_vma_close(struct vm_area_struct *vma)
{
	struct smon_ring_buffer *rb = vma->vm_private_data;

	if (rb) {
		smon_destroy_rb(rb);
		vma->vm_private_data = NULL;
	}
	PDEBUG("smon_vma_close: current = %d\n", current->pid);
}

int smon_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page = NULL;
	void *pageptr = NULL;
	struct smon_ring_buffer *rb = (struct smon_ring_buffer *)vma->vm_private_data;

	PDEBUG("vma_fault: vmf->pgoff = %lu\n", vmf->pgoff);
	/* check if page in ring buffer range */
	pageptr = (void*)smon_rb_get_page (rb, vmf->pgoff);
	if (!pageptr)
		goto out;

	page = virt_to_page(pageptr);
	get_page(page);
	
out:
	vmf->page = page;
	return 0;
}

/* mmap fops */
struct vm_operations_struct smon_vm_ops = {
	.open	= smon_vma_open,
	.close	= smon_vma_close,
	.fault	= smon_vma_fault,
};

static int smon_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct smon_ring_buffer *rb = NULL;
	unsigned long n_pages;

	PDEBUG("mmap: current= %d\n", current->pid);

	n_pages = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;

	/* Check if at least 2 pages */
	if (n_pages < 2)
		return -ENODEV;

	/* Check if data pages are a power of 2 */
	if (!is_power_of_2(n_pages))
		return -ENODEV;

	rb = smon_create_rb (n_pages);
	if (!rb)
		return -ENODEV;

	filp->private_data = rb;
	vma->vm_ops = &smon_vm_ops;
	vma->vm_private_data = rb;
	smon_vma_open(vma);

	return 0;
}

/*
 * The File Operations
 */
struct file_operations smon_fops = {
	.owner		= THIS_MODULE,
	.open		= smon_open,
	.release	= smon_release,
	.unlocked_ioctl	= smon_ioctl,
	.mmap		= smon_mmap,
	.poll		= smon_poll,
};



/* Setup Device */
static void smon_setup_cdev(struct smon_dev *dev, int index)
{
	int err, devno = MKDEV(smon_major, index);
    
	cdev_init(&dev->cdev, &smon_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &smon_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding smon%d", err, index);
}

/*
 * Module Stuff
 */
static int __init smon_init(void)
{
	int res;
	dev_t dev = MKDEV(smon_major, 0);

	/*
	 * Register major, and accept a dynamic number
	 */
	if (smon_major)
		res = register_chrdev_region(dev, smon_devs, "smon");
	else {
		res = alloc_chrdev_region(&dev, 0, smon_devs, "smon");
		smon_major = MAJOR(dev);
	}
	if (res < 0) {
		PDEBUG("init: error: registering chrdev\n");
		return res;
	}

	/* Initialize Sampling */
	smon_sample_init();
	
	/* Initialize Events */
	res = smon_event_init();
	if (res < 0)
		goto fail_event;
	/* Initialize Evsets */
	res = smon_evset_init();
	if (res < 0)
		goto fail_evset;

	/* Initialize Sched */
	res = smon_sched_init();
	if (res < 0)
		goto fail_sched;

	/* Device Setup */
	memset(&smon_device, 0, sizeof(struct smon_dev));
	atomic_set(&smon_device.count, 0);
	smon_setup_cdev (&smon_device, 0);

	PDEBUG("Hello!\n");

	return 0;

fail_sched:
	PDEBUG("init: error: initializing sched\n");
	smon_evset_exit();
fail_evset:
	PDEBUG("init: error: initializing event-sets\n");
	smon_event_exit();
fail_event:
	PDEBUG("init: error: initializing events\n");
	unregister_chrdev_region(dev, smon_devs);
	return res;
}

static void __exit smon_exit(void)
{
	/* Destroy Sched */
	smon_sched_exit();
	/* Destroy Event-Sets */
	smon_evset_exit();
	/* Destroy Events */
	smon_event_exit();

	cdev_del(&smon_device.cdev);
	unregister_chrdev_region(MKDEV (smon_major, 0), 1);
	PDEBUG("Goodbye!\n");
}

module_init (smon_init);
module_exit (smon_exit);


MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Luis Taniça <tanica.luis@gmail.com>");
MODULE_DESCRIPTION ("SchedMon Driver");

