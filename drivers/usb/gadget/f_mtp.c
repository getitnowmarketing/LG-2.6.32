/*
 * f_mtp.c -- USB MTP gadget driver 
* LG USB gadget driver for MTP sync
* This program is the modified version of free software for LG USB gadget driver
* Editor : jaeho.cho@lge.com
*/
/* #define VERBOSE_DEBUG */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>

#include <linux/types.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>

#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>

#include "gadget_chips.h"


// #define DEBUG

#define mtp_err(fmt, arg...)	printk(KERN_ERR "%s(): " fmt, __func__, ##arg)
#ifdef DEBUG
#define mtp_debug(fmt, arg...)	printk(KERN_ERR "%s(): " fmt, __func__, ##arg)
#else
#define mtp_debug(fmt, arg...)
#endif

#define BULK_BUFFER_SIZE    16 * 1024
#define MIN(a, b)	((a < b) ? a : b)

/*
 *
 */

/* static strings, in UTF-8 */
#define STRING_INTERFACE	0
#define STRING_MTP      	1

static struct usb_string mtp_string_defs[] = {
	[STRING_INTERFACE].s = "LG MTP Interface",
    [STRING_MTP].s = "MSFT100\376",
	{  /* ZEROES END LIST */ },
};

static struct usb_gadget_strings mtp_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		mtp_string_defs,
};

static struct usb_gadget_strings *mtp_strings[] = {
	&mtp_string_table,
	NULL,
};

/* There is only one interface. */
static struct usb_interface_descriptor intf_desc = {
	.bLength = sizeof intf_desc,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints = 3,
	.bInterfaceClass = 0x06,
	.bInterfaceSubClass = 0x01,
	.bInterfaceProtocol = 0x01,
};

static struct usb_endpoint_descriptor fs_bulk_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fs_bulk_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fs_intr_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize = __constant_cpu_to_le16(64),
	.bInterval = 10,
};

static struct usb_descriptor_header *fs_mtp_descs[] = {
	(struct usb_descriptor_header *) &intf_desc,
	(struct usb_descriptor_header *) &fs_bulk_out_desc,
	(struct usb_descriptor_header *) &fs_bulk_in_desc,
	(struct usb_descriptor_header *) &fs_intr_in_desc,
	NULL,
};

static struct usb_endpoint_descriptor hs_bulk_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(512),
	.bInterval = 0,
};

static struct usb_endpoint_descriptor hs_bulk_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(512),
	.bInterval = 0,
};

static struct usb_endpoint_descriptor hs_intr_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize = __constant_cpu_to_le16(64),
	.bInterval = 10,
};

static struct usb_descriptor_header *hs_mtp_descs[] = {
	(struct usb_descriptor_header *) &intf_desc,
	(struct usb_descriptor_header *) &hs_bulk_out_desc,
	(struct usb_descriptor_header *) &hs_bulk_in_desc,
	(struct usb_descriptor_header *) &hs_intr_in_desc,
	NULL,
};

/* used when MTP function is disabled */
static struct usb_descriptor_header *null_mtp_descs[] = {
	NULL,
};

#define MAX_BULK_RX_REQ_NUM 8
#define MAX_BULK_TX_REQ_NUM 8
#define MAX_CTL_RX_REQ_NUM	8

/*---------------------------------------------------------------------------*/
struct usb_mtp_context {
	struct usb_function function;
	struct usb_composite_dev *cdev;

	spinlock_t lock;  /* For RX/TX/INT list */

	struct usb_ep *bulk_in;
	struct usb_ep *bulk_out;
	struct usb_ep *intr_in;

	struct list_head rx_reqs;
	struct list_head rx_done_reqs;
	struct list_head tx_reqs;
	struct list_head ctl_rx_reqs;
	struct list_head ctl_rx_done_reqs;

	int online;
	int error;
	int cancel;
	int ctl_cancel;
	int intr_in_busy;

	wait_queue_head_t rx_wq;
	wait_queue_head_t tx_wq;
	wait_queue_head_t ctl_rx_wq;
	wait_queue_head_t ctl_tx_wq;

	struct usb_request *int_tx_req;
	struct usb_request *ctl_tx_req;

	/* the request we're currently reading from */
	struct usb_request *cur_read_req;
	/* buffer to point to available data in the current request */
	unsigned char *read_buf;
	/* available data length */
	int data_len;
};

static struct usb_mtp_context g_usb_mtp_context;

/* record all usb requests for bulk out */
static struct usb_request *pending_reqs[MAX_BULK_RX_REQ_NUM];
#define MTP_CANCEL_REQ_DATA_SIZE		6

struct ctl_req_wrapper {
	int header;
	struct usb_ctrlrequest creq;
	struct list_head	list;
	char cancel_data[MTP_CANCEL_REQ_DATA_SIZE];
};

struct ctl_req_wrapper ctl_reqs[MAX_CTL_RX_REQ_NUM];
struct ctl_req_wrapper *cur_creq;
int ctl_tx_done;

#ifdef CONFIG_USB_GADGET_LG_MTP_DRIVER
#define MTP_ONLINE 0x00
#define MTP_OFFLINE 0x01
#define MTP_UNKOWN 0x02
#define MTP_NO_INIT_STATUS 0x03
#define MTP_CLASS_CANCEL_REQ			0x64
#define MTP_CLASS_GET_EXTEND_EVEVT_DATA	0x65
#define MTP_CLASS_RESET_REQ				0x66
#define MTP_CLASS_GET_DEVICE_STATUS		0x67

__u8 g_bRequest= MTP_NO_INIT_STATUS;

#define USB_MTP_IOC_MAGIC 0xFF

#define USB_MTP_FUNC_IOC_CANCEL_REQUEST_SET _IOW(USB_MTP_IOC_MAGIC, 0x20, int)
#define USB_MTP_FUNC_IOC_CANCEL_REQUEST_GET _IOW(USB_MTP_IOC_MAGIC, 0x21, int)
#define USB_MTP_FUNC_IOC_GET_EXTENDED_EVENT_DATA_SET    _IOW(USB_MTP_IOC_MAGIC, 0x22, int)
#define USB_MTP_FUNC_IOC_GET_EXTENDED_EVENT_DATA_GET    _IOW(USB_MTP_IOC_MAGIC, 0x23, int)
#define USB_MTP_FUNC_IOC_DEVICE_RESET_REQUEST_SET   _IOW(USB_MTP_IOC_MAGIC, 0x24, int)
#define USB_MTP_FUNC_IOC_DEVICE_RESET_REQUEST_GET   _IOW(USB_MTP_IOC_MAGIC, 0x25, int)
#define USB_MTP_FUNC_IOC_GET_DEVICE_STATUS_SET  _IOW(USB_MTP_IOC_MAGIC, 0x26, int)
#define USB_MTP_FUNC_IOC_GET_DEVICE_STATUS_GET  _IOW(USB_MTP_IOC_MAGIC, 0x27, int)
#define USB_MTP_FUNC_IOC_GET_ONLINE_STATUS_GET  _IOW(USB_MTP_IOC_MAGIC, 0x28, int)
#define USB_MTP_FUNC_IOC_CONTROL_REQUEST_GET _IOW(USB_MTP_IOC_MAGIC, 0x29, int)
#endif

/*-------------------------------------------------------------------------*/

static struct usb_request *req_new(struct usb_ep *ep, int size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
	req->buf = kmalloc(size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void req_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

/* add a request to the tail of a list */
static void req_put(struct list_head *head, struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&g_usb_mtp_context.lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&g_usb_mtp_context.lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request *req_get(struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&g_usb_mtp_context.lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&g_usb_mtp_context.lock, flags);
	return req;
}

/* add a mtp control request to the tail of a list */
static void ctl_req_put(struct list_head *head, struct ctl_req_wrapper *req)
{
	unsigned long flags;

	spin_lock_irqsave(&g_usb_mtp_context.lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&g_usb_mtp_context.lock, flags);
}

/* remove a request from the head of a list */
static struct ctl_req_wrapper *ctl_req_get(struct list_head *head)
{
	unsigned long flags;
	struct ctl_req_wrapper *req;

	spin_lock_irqsave(&g_usb_mtp_context.lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct ctl_req_wrapper, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&g_usb_mtp_context.lock, flags);
	return req;
}
/*-------------------------------------------------------------------------*/

static void mtp_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	mtp_debug("status is %d %p %d\n", req->status, req, req->actual);
	if (req->status == -ECONNRESET)
		usb_ep_fifo_flush(ep);

	if (req->status != 0) {
		g_usb_mtp_context.error = 1;
		mtp_err("status is %d %p len=%d\n",
		req->status, req, req->actual);
	}

	req_put(&g_usb_mtp_context.tx_reqs, req);
	wake_up(&g_usb_mtp_context.tx_wq);
}

static void mtp_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	mtp_debug("status is %d %p %d\n", req->status, req, req->actual);
	if (req->status == 0) {
		req_put(&g_usb_mtp_context.rx_done_reqs, req);
	} else {
		mtp_err("status is %d %p len=%d\n",
		req->status, req, req->actual);
		g_usb_mtp_context.error = 1;
		if (req->status == -ECONNRESET)
			usb_ep_fifo_flush(ep);
		req_put(&g_usb_mtp_context.rx_reqs, req);
	}
	wake_up(&g_usb_mtp_context.rx_wq);
}

static void mtp_int_complete(struct usb_ep *ep, struct usb_request *req)
{
	mtp_debug("status is %d %d\n", req->status, req->actual);

	if (req->status == -ECONNRESET)
		usb_ep_fifo_flush(ep);

	if (req->status != 0)
		mtp_err("status is %d %p len=%d\n",
		req->status, req, req->actual);

	g_usb_mtp_context.intr_in_busy = 0;
	return;
}

static ssize_t mtp_read(struct file *fp, char __user *buf,
				size_t count, loff_t *pos)
{
	struct usb_request *req = 0;
	int xfer, rc = count;
	int ret;

	while (count > 0) {
		mtp_debug("count=%d\n", count);
		if (g_usb_mtp_context.error) {
			return -EIO;
		}
		/* we will block until we're online */
		ret = wait_event_interruptible(g_usb_mtp_context.rx_wq,
			(g_usb_mtp_context.online || g_usb_mtp_context.cancel));
		if (g_usb_mtp_context.cancel) {
			mtp_debug("cancel return in mtp_read at beginning\n");
			g_usb_mtp_context.cancel = 0;
			return -EINVAL;
		}
		if (ret < 0) {
			mtp_err("wait_event_interruptible return %d\n", ret);
			rc = ret;
			break;
		}

		/* if we have idle read requests, get them queued */
		while (1) {
			req = req_get(&g_usb_mtp_context.rx_reqs);
			if (!req)
				break;
requeue_req:
			req->length = BULK_BUFFER_SIZE;
			mtp_debug("rx %p queue\n", req);
			ret = usb_ep_queue(g_usb_mtp_context.bulk_out,
				req, GFP_ATOMIC);

			if (ret < 0) {
				mtp_err("queue error %d\n", ret);
				g_usb_mtp_context.error = 1;
				req_put(&g_usb_mtp_context.rx_reqs, req);
				return ret;
			}
		}

		/* if we have data pending, give it to userspace */
		if (g_usb_mtp_context.data_len > 0) {
			if (g_usb_mtp_context.data_len < count)
				xfer = g_usb_mtp_context.data_len;
			else
				xfer = count;

			if (copy_to_user(buf, g_usb_mtp_context.read_buf,
								xfer)) {
				rc = -EFAULT;
				break;
			}
			g_usb_mtp_context.read_buf += xfer;
			g_usb_mtp_context.data_len -= xfer;
			buf += xfer;
			count -= xfer;
			mtp_debug("xfer=%d\n", xfer);

			/* if we've emptied the buffer, release the request */
			if (g_usb_mtp_context.data_len == 0) {
				mtp_debug("LG_FW : mtp data_len  = 0\n");
				req_put(&g_usb_mtp_context.rx_reqs,
						g_usb_mtp_context.cur_read_req);
				g_usb_mtp_context.cur_read_req = 0;
#if defined(CONFIG_USB_GADGET_LG_MTP_DRIVER)
				count = 0;
				rc = xfer;
#endif
			}
			continue;
		}

		/* wait for a request to complete */
		req = 0;
		mtp_debug("wait req finish\n");
		ret = wait_event_interruptible(g_usb_mtp_context.rx_wq,
		((req = req_get(&g_usb_mtp_context.rx_done_reqs))
			|| g_usb_mtp_context.cancel));
		mtp_debug("req finished\n");
		if (g_usb_mtp_context.cancel) {
			if (req != 0)
				req_put(&g_usb_mtp_context.rx_reqs, req);
			mtp_debug("cancel return in mtp_read at complete\n");
			g_usb_mtp_context.cancel = 0;
			return -EINVAL;
		}
		if (ret < 0) {
			mtp_err("wait_event_interruptible(2) return %d\n", ret);
			rc = ret;
			break;
		}
		if (req != 0) {
			/* if we got a 0-len one we need to put it back into
			** service.  if we made it the current read req we'd
			** be stuck forever
			*/
			if (req->actual == 0)
				goto requeue_req;

			g_usb_mtp_context.cur_read_req = req;
			g_usb_mtp_context.data_len = req->actual;
			g_usb_mtp_context.read_buf = req->buf;
			mtp_debug("rx %p done actual=%d\n", req, req->actual);
		}
	}

	mtp_debug("mtp_read returning %d\n", rc);
	return rc;
}

static ssize_t mtp_write(struct file *fp, const char __user *buf,
				 size_t count, loff_t *pos)
{
	struct usb_request *req;
	int rc = count, xfer;
	int ret;

	while (count > 0) {
		mtp_debug("count=%d\n", count);
		if (g_usb_mtp_context.error) {
			return -EIO;
		}
		/* get an idle tx request to use */
		ret = wait_event_interruptible(g_usb_mtp_context.tx_wq,
			(g_usb_mtp_context.online || g_usb_mtp_context.cancel));

		if (g_usb_mtp_context.cancel) {
			mtp_debug("cancel return in mtp_write at beginning\n");
			g_usb_mtp_context.cancel = 0;
			return -EINVAL;
		}
		if (ret < 0) {
			mtp_err("wait_event_interruptible return %d\n", ret);
			rc = ret;
			break;
		}

		req = 0;
		mtp_debug("get tx req\n");
		ret = wait_event_interruptible(g_usb_mtp_context.tx_wq,
			((req = req_get(&g_usb_mtp_context.tx_reqs))
			 || g_usb_mtp_context.cancel));

		mtp_debug("got tx req\n");
		if (g_usb_mtp_context.cancel) {
			mtp_debug("cancel return in mtp_write get req\n");
			if (req != 0)
				req_put(&g_usb_mtp_context.tx_reqs, req);
			g_usb_mtp_context.cancel = 0;
			return -EINVAL;
		}
		if (ret < 0) {
			mtp_err("wait_event_interruptible return(2) %d\n", ret);
			rc = ret;
			break;
		}

		if (req != 0) {
			if (count > BULK_BUFFER_SIZE)
				xfer = BULK_BUFFER_SIZE;
			else
				xfer = count;
			if (copy_from_user(req->buf, buf, xfer)) {
				req_put(&g_usb_mtp_context.tx_reqs, req);
				rc = -EFAULT;
				break;
			}

			req->length = xfer;
			ret = usb_ep_queue(g_usb_mtp_context.bulk_in,
				req, GFP_ATOMIC);
			if (ret < 0) {
				mtp_err("error %d\n", ret);
				g_usb_mtp_context.error = 1;
				req_put(&g_usb_mtp_context.tx_reqs, req);
				rc = ret;
				break;
			}

			buf += xfer;
			count -= xfer;
			mtp_debug("xfer=%d\n", xfer);
		}
	}

	mtp_debug("mtp_write returning %d\n", rc);
	return rc;
}

/* ioctl related */
#define MTP_EVENT_SIZE   28
struct mtp_event_data {
    unsigned char data[MTP_EVENT_SIZE];
};
#ifdef CONFIG_USB_GADGET_LG_MTP_DRIVER
extern int mtp_get_usb_state(void);
#endif

static int mtp_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{   
  u16 g_mtp_get_status;    
  struct usb_request	*req =g_usb_mtp_context.cdev->req;
  struct usb_ep *ep0 = g_usb_mtp_context.cdev->gadget->ep0;
  long ret;
  int usbconnect = 0;

  switch (cmd)  
  {      	       
	case USB_MTP_FUNC_IOC_CONTROL_REQUEST_GET:
		
      if(g_bRequest==MTP_CLASS_CANCEL_REQ||
        g_bRequest==MTP_CLASS_RESET_REQ||
        g_bRequest==MTP_CLASS_GET_DEVICE_STATUS
        )
      {
         mtp_debug("USB_MTP_FUNC_IOC_CONTROL_REQUEST_GET status = %d\n", g_bRequest);
         ret = copy_to_user ((void __user *)arg, &g_bRequest, sizeof(g_bRequest));
      }
      else
      {
         mtp_debug("USB_MTP_FUNC_IOC_OTHER_CONTROL_REQUEST_GET status = %d\n", g_bRequest);
         usbconnect = mtp_get_usb_state();
         if(usbconnect == 0)
         {
           g_bRequest = MTP_OFFLINE; //offline
           ret = copy_to_user ((void __user *)arg, &g_bRequest, sizeof(g_bRequest));
         }
         else
         {
           if(g_usb_mtp_context.online == 1)
           {
              g_bRequest = MTP_ONLINE;//online
              ret = copy_to_user ((void __user *)arg, &g_bRequest, sizeof(g_bRequest));
           }
           else
           {
              g_bRequest = MTP_UNKOWN; //unkown
              ret = copy_to_user ((void __user *)arg, &g_bRequest, sizeof(g_bRequest));
           }
         }
      }
	  
	   g_bRequest = MTP_NO_INIT_STATUS;
	   
      if(ret >= 0)
        return ret;
      else
        return -EFAULT;   
	  break;
		
	case USB_MTP_FUNC_IOC_GET_DEVICE_STATUS_SET:
		 mtp_debug("USB_MTP_FUNC_IOC_GET_DEVICE_STATUS_SET status = %d\n", g_bRequest);
         ret = copy_from_user (&g_mtp_get_status, (void __user *)arg, sizeof(g_mtp_get_status));
         if(ret < 0)
           return -EFAULT;

         if(req == NULL)
         {
           mtp_debug("LG_FW :: req is NULL");
           return -EFAULT;
         }
         *((u16 *)(req->buf)) = 0x0004;
         *((u16 *)(req->buf + 2)) = arg;
		 req->zero = 0;
		 req->length = 6;
         usb_ep_queue(ep0,req, GFP_ATOMIC);
		 break;
	                          
    default :
      mtp_debug("Invalid IOCTL  Processed!!\n");
      break; 
 	}  
  
	return 0;  
}

/* file operations for MTP device /dev/mtp */
static const struct file_operations mtp_fops = {
	.owner = THIS_MODULE,
	.read = mtp_read,
	.write = mtp_write,
};

static struct miscdevice mtp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mtp",
	.fops = &mtp_fops,
};

/* file operations for MTP device /dev/mtp_csr */
static const struct file_operations mtp_csr_fops = {
	.owner = THIS_MODULE,
	.ioctl = mtp_ioctl,
};
static struct miscdevice mtp_csr_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mtp_csr",
	.fops = &mtp_csr_fops,
};

/* mtpctl related */
#define MTP_CTL_CLASS_REQ    1
#define MTP_CTL_CLASS_REPLY  2

struct mtp_ctl_msg_header {
    int msg_len;
    int msg_id;
};

#define MTP_CTL_MSG_HEADER_SIZE   (sizeof(struct mtp_ctl_msg_header))
#define MTP_CTL_MSG_SIZE	(MTP_CTL_MSG_HEADER_SIZE +\
			 sizeof(struct usb_ctrlrequest))


static void mtp_ctl_read_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct ctl_req_wrapper	*ctl_req = ep->driver_data;

	mtp_debug("mtp_ctl_read_complete --> %d, %d/%d\n",
	req->status, req->actual, req->length);

	if (req->status == 0)
		memcpy(ctl_req->cancel_data, req->buf, req->actual);

	wake_up(&g_usb_mtp_context.ctl_rx_wq);
}

static void mtp_ctl_write_complete(struct usb_ep *ep, struct usb_request *req)
{
	mtp_debug("mtp_ctl_write_complete --> %d, %d/%d\n",
	req->status, req->actual, req->length);

	ctl_tx_done = 1;

	wake_up(&g_usb_mtp_context.ctl_tx_wq);
}

static ssize_t mtp_ctl_read(struct file *file, char *buf,
	size_t count, loff_t *pos)
{
    int ret, size = sizeof(struct usb_ctrlrequest);
	struct mtp_ctl_msg_header msg;

	mtp_debug("count=%d\n", count);

	if (!g_usb_mtp_context.online)
		return -EINVAL;

	if (!cur_creq) {
		ret = wait_event_interruptible(g_usb_mtp_context.ctl_rx_wq,
		((cur_creq = ctl_req_get(&g_usb_mtp_context.ctl_rx_done_reqs))
			|| g_usb_mtp_context.ctl_cancel));
		if (g_usb_mtp_context.ctl_cancel) {
			mtp_debug("ctl_cancel return in mtp_ctl_read\n");
			if (cur_creq)
				ctl_req_put(&g_usb_mtp_context.ctl_rx_reqs,
				cur_creq);
			g_usb_mtp_context.ctl_cancel = 0;
			return -EINVAL;
		}
		if (ret < 0) {
			mtp_err("wait_event_interruptible return %d\n", ret);
			return ret;
		}
	}

	msg.msg_id = MTP_CTL_CLASS_REQ;
	msg.msg_len = MTP_CTL_MSG_SIZE;
	if (cur_creq->creq.bRequest == MTP_CLASS_CANCEL_REQ)
		msg.msg_len = MTP_CTL_MSG_SIZE + MTP_CANCEL_REQ_DATA_SIZE;

	if (cur_creq->header == 1) {
		cur_creq->header = 0;
		if (copy_to_user(buf, &msg, MTP_CTL_MSG_HEADER_SIZE))
			goto ctl_read_fail;
		ret = MTP_CTL_MSG_HEADER_SIZE;
		mtp_debug("msg header return %d\n", ret);
	} else {
		if (copy_to_user(buf, &cur_creq->creq, size))
			goto ctl_read_fail;
		ret = size;
		if (cur_creq->creq.bRequest == MTP_CLASS_CANCEL_REQ) {
			if (copy_to_user(buf + size, &cur_creq->cancel_data,
				MTP_CANCEL_REQ_DATA_SIZE))
				goto ctl_read_fail;
			ret += MTP_CANCEL_REQ_DATA_SIZE;
		}
		mtp_debug("prepare %d %x\n", ret, cur_creq->creq.bRequest);
		ctl_req_put(&g_usb_mtp_context.ctl_rx_reqs, cur_creq);
		cur_creq = NULL;
	}

	mtp_debug("return %d\n", ret);
    return ret;

ctl_read_fail:
	ctl_req_put(&g_usb_mtp_context.ctl_rx_reqs, cur_creq);
	cur_creq = NULL;
	mtp_debug("return -EFAULT\n");
    return -EFAULT;
}

static ssize_t mtp_ctl_write(struct file *file, const char *buf,
	size_t count, loff_t *pos)
{
    struct mtp_ctl_msg_header msg;
	struct usb_request *req = NULL;
	struct usb_ep *ep0;
	int ret;

	mtp_debug("count=%d\n", count);

	ret = wait_event_interruptible(g_usb_mtp_context.ctl_tx_wq,
		(g_usb_mtp_context.online || g_usb_mtp_context.ctl_cancel));
	if (g_usb_mtp_context.ctl_cancel) {
		mtp_debug("ctl_cancel return in mtp_ctl_write 1\n");
		g_usb_mtp_context.ctl_cancel = 0;
		return -EINVAL;
	}
	if (ret < 0)
		return ret;

	ep0 = g_usb_mtp_context.cdev->gadget->ep0;
    if (count > ep0->maxpacket || count < MTP_CTL_MSG_HEADER_SIZE) {
		mtp_err("size invalid\n");
		return -ENOMEM;
    }

    /* msg info */
    if (copy_from_user(&msg, buf, MTP_CTL_MSG_HEADER_SIZE))
		return -EINVAL;

    mtp_debug("msg len = %d, msg id = %d", msg.msg_len, msg.msg_id);
    if (msg.msg_id != MTP_CTL_CLASS_REPLY) {
		mtp_err("invalid id %d", msg.msg_id);
		return -EINVAL;
    }

    /* sending the data */
	req = g_usb_mtp_context.ctl_tx_req;
	if (!req)
		return -ENOMEM;
    req->length = count - MTP_CTL_MSG_HEADER_SIZE;
	req->complete = mtp_ctl_write_complete;
    if (copy_from_user(req->buf,
		(u8 *)buf + MTP_CTL_MSG_HEADER_SIZE, req->length)) {
		return -EINVAL;
	}
	ctl_tx_done = 0;
	if (usb_ep_queue(ep0, req, GFP_ATOMIC)) {
		req->status = 0;
		mtp_ctl_write_complete(ep0, req);
		return -EIO;
	}
	ret = wait_event_interruptible(g_usb_mtp_context.ctl_tx_wq,
		(ctl_tx_done || g_usb_mtp_context.ctl_cancel));
	ctl_tx_done = 0;
	if (g_usb_mtp_context.ctl_cancel) {
		mtp_debug("ctl_cancel return in mtp_ctl_write\n");
		g_usb_mtp_context.ctl_cancel = 0;
		return -EINVAL;
	}
	if (ret < 0)
		return ret;

	mtp_debug("return count=%d\n", count);
    return count;
}

static const struct file_operations mtp_ctl_fops = {
     .read = mtp_ctl_read,
     .write = mtp_ctl_write,
};

static void
mtp_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_request *req;
	int n;

	for (n = 0; n < MAX_BULK_RX_REQ_NUM; n++)
		pending_reqs[n] = NULL;

	while ((req = req_get(&g_usb_mtp_context.rx_reqs)))
		req_free(req, g_usb_mtp_context.bulk_out);
	while ((req = req_get(&g_usb_mtp_context.rx_done_reqs)))
		req_free(req, g_usb_mtp_context.bulk_out);
	while ((req = req_get(&g_usb_mtp_context.tx_reqs)))
		req_free(req, g_usb_mtp_context.bulk_in);

	req_free(g_usb_mtp_context.int_tx_req, g_usb_mtp_context.intr_in);
	req_free(g_usb_mtp_context.ctl_tx_req,
	g_usb_mtp_context.cdev->gadget->ep0);
	g_usb_mtp_context.intr_in_busy = 0;
#if !defined(CONFIG_USB_GADGET_LG_MTP_DRIVER)
	misc_deregister(&mtp_device);
    remove_proc_entry("mtpctl", NULL);
#endif
}

static int
mtp_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	int n, rc, id;
	struct usb_ep *ep;
	struct usb_request *req;
#if !defined(CONFIG_USB_GADGET_LG_MTP_DRIVER)
    struct proc_dir_entry *mtp_proc = NULL;
#endif

	g_usb_mtp_context.cdev = c->cdev;
	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	intf_desc.bInterfaceNumber = id;

	/* Find all the endpoints we will use */
	ep = usb_ep_autoconfig(g_usb_mtp_context.cdev->gadget,
						&fs_bulk_in_desc);
	if (!ep) {
		mtp_err("auto-configure hs_bulk_in_desc error\n");
		goto autoconf_fail;
	}
	ep->driver_data = &g_usb_mtp_context;
	g_usb_mtp_context.bulk_in = ep;

	ep = usb_ep_autoconfig(g_usb_mtp_context.cdev->gadget,
						&fs_bulk_out_desc);
	if (!ep) {
		mtp_err("auto-configure hs_bulk_out_desc error\n");
		goto autoconf_fail;
	}
	ep->driver_data = &g_usb_mtp_context;
	g_usb_mtp_context.bulk_out = ep;

	ep = usb_ep_autoconfig(g_usb_mtp_context.cdev->gadget,
						&fs_intr_in_desc);
	if (!ep) {
		mtp_err("auto-configure hs_intr_in_desc error\n");
		goto autoconf_fail;
	}
	ep->driver_data = &g_usb_mtp_context;
	g_usb_mtp_context.intr_in = ep;

	if (gadget_is_dualspeed(g_usb_mtp_context.cdev->gadget)) {
		/* Assume endpoint addresses are the same for both speeds */
		hs_bulk_in_desc.bEndpointAddress =
		    fs_bulk_in_desc.bEndpointAddress;
		hs_bulk_out_desc.bEndpointAddress =
		    fs_bulk_out_desc.bEndpointAddress;
		hs_intr_in_desc.bEndpointAddress =
		    fs_intr_in_desc.bEndpointAddress;
	}

	rc = -ENOMEM;

	for (n = 0; n < MAX_BULK_RX_REQ_NUM; n++) {
		req = req_new(g_usb_mtp_context.bulk_out, BULK_BUFFER_SIZE);
		if (!req)
			goto autoconf_fail;

		pending_reqs[n] = req;

		req->complete = mtp_out_complete;
		req_put(&g_usb_mtp_context.rx_reqs, req);
	}
	for (n = 0; n < MAX_BULK_TX_REQ_NUM; n++) {
		req = req_new(g_usb_mtp_context.bulk_in, BULK_BUFFER_SIZE);
		if (!req)
			goto autoconf_fail;

		req->complete = mtp_in_complete;
		req_put(&g_usb_mtp_context.tx_reqs, req);
	}

	for (n = 0; n < MAX_CTL_RX_REQ_NUM; n++)
		ctl_req_put(&g_usb_mtp_context.ctl_rx_reqs, &ctl_reqs[n]);

	g_usb_mtp_context.int_tx_req =
		req_new(g_usb_mtp_context.intr_in, BULK_BUFFER_SIZE);
	if (!g_usb_mtp_context.int_tx_req)
		goto autoconf_fail;
	g_usb_mtp_context.intr_in_busy = 0;
	g_usb_mtp_context.int_tx_req->complete = mtp_int_complete;

	g_usb_mtp_context.ctl_tx_req =
		req_new(g_usb_mtp_context.cdev->gadget->ep0, 512);
	if (!g_usb_mtp_context.ctl_tx_req)
		goto autoconf_fail;
#if !defined(CONFIG_USB_GADGET_LG_MTP_DRIVER)
	misc_register(&mtp_device);

	mtp_proc = create_proc_entry("mtpctl", 0666, 0);
	if (!mtp_proc) {
		mtp_err("creating /proc/mtpctl failed\n");
		goto autoconf_fail;
    }
    mtp_proc->proc_fops = &mtp_ctl_fops;
#endif
	return 0;

autoconf_fail:
	rc = -ENOTSUPP;
	mtp_function_unbind(c, f);
	return rc;
}

static void mtp_function_disable(struct usb_function *f)
{
	printk(KERN_DEBUG "%s(): disabled\n", __func__);
	g_usb_mtp_context.online = 0;
	g_usb_mtp_context.cancel = 1;
	g_usb_mtp_context.ctl_cancel = 1;
	g_usb_mtp_context.error = 1;

	usb_ep_fifo_flush(g_usb_mtp_context.bulk_in);
	usb_ep_fifo_flush(g_usb_mtp_context.bulk_out);
	usb_ep_fifo_flush(g_usb_mtp_context.intr_in);
	usb_ep_disable(g_usb_mtp_context.bulk_in);
	usb_ep_disable(g_usb_mtp_context.bulk_out);
	usb_ep_disable(g_usb_mtp_context.intr_in);

	g_usb_mtp_context.cur_read_req = 0;
	g_usb_mtp_context.read_buf = 0;
	g_usb_mtp_context.data_len = 0;
	/* readers may be blocked waiting for us to go online */
	wake_up(&g_usb_mtp_context.rx_wq);
	wake_up(&g_usb_mtp_context.tx_wq);
	wake_up(&g_usb_mtp_context.ctl_rx_wq);
	wake_up(&g_usb_mtp_context.ctl_tx_wq);
}

static void start_out_receive(void)
{
	struct usb_request *req;
	int ret;

	/* if we have idle read requests, get them queued */
	while ((req = req_get(&g_usb_mtp_context.rx_reqs))) {
		req->length = BULK_BUFFER_SIZE;
		ret = usb_ep_queue(g_usb_mtp_context.bulk_out, req, GFP_ATOMIC);
		if (ret < 0) {
			mtp_err("error %d\n", ret);
			g_usb_mtp_context.error = 1;
			req_put(&g_usb_mtp_context.rx_reqs, req);
		}
	}
}

static int mtp_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	int ret;

	printk(KERN_DEBUG "%s intf=%d alt=%d\n", __func__, intf, alt);
	ret = usb_ep_enable(g_usb_mtp_context.bulk_in,
			ep_choose(g_usb_mtp_context.cdev->gadget,
				&hs_bulk_in_desc,
				&fs_bulk_in_desc));
	if (ret)
		return ret;
	ret = usb_ep_enable(g_usb_mtp_context.bulk_out,
			ep_choose(g_usb_mtp_context.cdev->gadget,
				&hs_bulk_out_desc,
				&fs_bulk_out_desc));
	if (ret) {
		usb_ep_disable(g_usb_mtp_context.bulk_in);
		return ret;
	}

	ret = usb_ep_enable(g_usb_mtp_context.intr_in,
			ep_choose(g_usb_mtp_context.cdev->gadget,
				&hs_intr_in_desc,
				&fs_intr_in_desc));
	if (ret) {
		usb_ep_disable(g_usb_mtp_context.bulk_in);
		usb_ep_disable(g_usb_mtp_context.bulk_out);
		return ret;
	}

	g_usb_mtp_context.cur_read_req = 0;
	g_usb_mtp_context.read_buf = 0;
	g_usb_mtp_context.data_len = 0;

	/* we're online -- get all rx requests queued */
	start_out_receive();

	g_usb_mtp_context.online = 1;
	g_usb_mtp_context.cancel = 0;
	g_usb_mtp_context.ctl_cancel = 0;
	g_usb_mtp_context.error = 0;

	/* readers may be blocked waiting for us to go online */
	wake_up(&g_usb_mtp_context.rx_wq);
	return 0;
}

#define MTP_MOD_VENDOR_CODE   0x1C
static int  mtp_ext_id = 4;
static unsigned char mtp_ext_desc[] =
"\050\000\000\000\000\001\004\000\001\000\000\000\000\000\000\000\000\001"
"\115\124\120\000\000\000\000\000\060\060\000\000\000\000\000\000\000\000"
"\000\000\000\000";
static int  mtp_ext_str_idx = 238;

static int mtp_function_setup(struct usb_function *f,
					const struct usb_ctrlrequest *ctrl)
{
	int	value = -EOPNOTSUPP;
	u16     wIndex = le16_to_cpu(ctrl->wIndex);
	u16     wLength = le16_to_cpu(ctrl->wLength);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	struct ctl_req_wrapper	*ctl_req;

//    int result = -EOPNOTSUPP;

	mtp_debug("bRequestType=0x%x bRequest=0x%x wIndex=0x%x wLength=0x%x\n",
		ctrl->bRequestType, ctrl->bRequest, wIndex, wLength);

	switch (ctrl->bRequestType & USB_TYPE_MASK) {
	case USB_TYPE_VENDOR:
		switch (ctrl->bRequest) {
		case MTP_MOD_VENDOR_CODE:
			if (wIndex == mtp_ext_id) {
				memcpy(req->buf, mtp_ext_desc,
						sizeof(mtp_ext_desc));
				if (wLength < mtp_ext_desc[0])
					value = wLength;
				else
					value = mtp_ext_desc[0];

				req->zero = 0;
				req->length = value;
				if (usb_ep_queue(cdev->gadget->ep0, req,
					GFP_ATOMIC))
					mtp_err("ep0 in queue failed\n");
			}
			break;
		default:
			break;
		}
		break;
	case USB_TYPE_CLASS:
		switch (ctrl->bRequest) {
		case MTP_CLASS_CANCEL_REQ:
		case MTP_CLASS_GET_EXTEND_EVEVT_DATA:
		case MTP_CLASS_RESET_REQ:
		case MTP_CLASS_GET_DEVICE_STATUS:
#ifdef CONFIG_USB_GADGET_LG_MTP_DRIVER
            g_bRequest = ctrl->bRequest;
#endif
			mtp_debug("ctl request=0x%x\n", ctrl->bRequest);
			ctl_req = ctl_req_get(&g_usb_mtp_context.ctl_rx_reqs);
			if (!ctl_req) {
				mtp_err("get free ctl req failed\n");
				break;
			}
			memcpy(&ctl_req->creq, ctrl,
					sizeof(struct usb_ctrlrequest));
			ctl_req->header = 1;
			ctl_req_put(&g_usb_mtp_context.ctl_rx_done_reqs,
				ctl_req);
			value = 0;
			if ((ctrl->bRequest  == MTP_CLASS_CANCEL_REQ)
				&& wLength == MTP_CANCEL_REQ_DATA_SIZE) {

				memset(&ctl_req->cancel_data, 0,
					MTP_CANCEL_REQ_DATA_SIZE);
				value = wLength;
				cdev->gadget->ep0->driver_data = ctl_req;
				req->complete = mtp_ctl_read_complete;
				req->zero = 0;
				req->length = wLength;

				if (usb_ep_queue(cdev->gadget->ep0,
						req, GFP_ATOMIC)) {
					mtp_err("ep0 out queue failed\n");
					mtp_ctl_read_complete(cdev->gadget->ep0,
							req);
				}
			} else
				wake_up(&g_usb_mtp_context.ctl_rx_wq);
			break;
		default:
			break;
		}
	}

	mtp_debug("return value=%d\n", value);
	return value;

}


int  mtp_function_add(struct usb_composite_dev *cdev,
	struct usb_configuration *c)
{
	int ret = 0;
	int status;

	init_waitqueue_head(&g_usb_mtp_context.rx_wq);
	init_waitqueue_head(&g_usb_mtp_context.tx_wq);
	init_waitqueue_head(&g_usb_mtp_context.ctl_rx_wq);
	init_waitqueue_head(&g_usb_mtp_context.ctl_tx_wq);

	INIT_LIST_HEAD(&g_usb_mtp_context.rx_reqs);
	INIT_LIST_HEAD(&g_usb_mtp_context.rx_done_reqs);
	INIT_LIST_HEAD(&g_usb_mtp_context.tx_reqs);
	INIT_LIST_HEAD(&g_usb_mtp_context.ctl_rx_reqs);
	INIT_LIST_HEAD(&g_usb_mtp_context.ctl_rx_done_reqs);

	status = usb_string_id(c->cdev);
	if (status >= 0) {
		mtp_string_defs[STRING_INTERFACE].id = status;
        intf_desc.iInterface = status;
	}

	mtp_string_defs[STRING_MTP].id = mtp_ext_str_idx;

	g_usb_mtp_context.cdev = cdev;
	g_usb_mtp_context.function.name = "mtp";
	g_usb_mtp_context.function.descriptors = fs_mtp_descs;
	g_usb_mtp_context.function.hs_descriptors = hs_mtp_descs;
	g_usb_mtp_context.function.strings = mtp_strings;
	g_usb_mtp_context.function.bind = mtp_function_bind;
	g_usb_mtp_context.function.unbind = mtp_function_unbind;
	g_usb_mtp_context.function.setup = mtp_function_setup;
	g_usb_mtp_context.function.set_alt = mtp_function_set_alt;
	g_usb_mtp_context.function.disable = mtp_function_disable;

	ret = usb_add_function(c, &g_usb_mtp_context.function);
	if (ret) {
		mtp_err("MTP gadget driver failed to initialize\n");
		return ret;
	}

	return 0;
}

struct usb_function *mtp_function_enable(int enable, int id)
{
	printk(KERN_DEBUG "%s enable=%d id=%d\n", __func__, enable, id);
	if (enable) {
		g_usb_mtp_context.function.descriptors = fs_mtp_descs;
		g_usb_mtp_context.function.hs_descriptors = hs_mtp_descs;
		intf_desc.bInterfaceNumber = id;
	} else {
		g_usb_mtp_context.function.descriptors = null_mtp_descs;
		g_usb_mtp_context.function.hs_descriptors = null_mtp_descs;
	}
	return &g_usb_mtp_context.function;
}

#if defined(CONFIG_USB_GADGET_LG_MTP_DRIVER)
int mtp_function_init(void)
{
    int ret;
#if 0
    struct proc_dir_entry *mtp_proc = NULL;
#endif
    ret = misc_register(&mtp_device);
    if (ret)
    {
	    printk(KERN_ERR "mtp device failed to initialize\n");
    }
    ret = misc_register(&mtp_csr_device);
    
    if (ret)
    {
	    printk(KERN_ERR "mtp csr device failed to initialize\n");
    }
    
#if 0    
    mtp_proc = create_proc_entry("mtpctl", 0666, 0);
    if (!mtp_proc) {
	   mtp_err("creating /proc/mtpctl failed\n");
         return 1;
    }
    mtp_proc->proc_fops = &mtp_ctl_fops;
#endif
    return ret;
}

void mtp_function_exit(void)
{
	misc_deregister(&mtp_device);
    misc_deregister(&mtp_csr_device);
}
#endif

