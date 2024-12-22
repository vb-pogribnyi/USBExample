#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h> 

#define VENDOR_ID 0x0483
#define PRODUCT_ID 0x1255
#define DEV_FILE_NAME "usbdrv_%d"

#define CTRL_REQ_LEN 4

const struct usb_device_id usb_drv_id_table[] = {
    {USB_DEVICE(VENDOR_ID, PRODUCT_ID)},
    {}
};

static struct usb_class_driver usb_drv_class;
static struct usb_device *usb_drv_device;
static __u8 *usb_buff;

int usb_drv_open(struct inode *i, struct file *f) {
    return 0;
}

ssize_t usb_drv_read (struct file *f, char __user *buff, size_t count, loff_t *offset) {
    // USB setup request fields
    __u8 usb_request = 0x00;        // GET STATUS
    __u16 usb_requesttype = 0xC0;   // TYPE_VENDOR | RECEPIENT_DEVICE

    // Value & Index can be anything
    __u16 usb_value = 1;
    __u16 usb_index = 2;

    // The host defines length of the response.
    // Device may send fewer bytes, but it should not send more then requested.
    int response_len = CTRL_REQ_LEN;
    int ret = 0;
    
    unsigned int ctrlpipe = usb_rcvctrlpipe(usb_drv_device, 0);

    // Stop reading file if not the first attempt
    if (*offset > 0) return 0;
    printk("Reading device\n");
    
    usb_control_msg(usb_drv_device, ctrlpipe, usb_request, usb_requesttype, usb_value, 
        usb_index, usb_buff, response_len, 1000);

    // Copy the response to the user-space buffer,
    // that will be displayed as a result of the read operation.
    ret = copy_to_user(buff, usb_buff, min((size_t)response_len, count));

    *offset += response_len;

    // Number of bytes read. If not zero, the function will be called again.
    // But if returned zero, the output will be empty.
    return response_len;
}

ssize_t usb_drv_write(struct file *f, const char __user *buff, size_t count, loff_t* offset) {
    // USB setup request fields
    __u8 usb_request = 0x03;        // SET FEATURE
    __u16 usb_requesttype = 0x40;   // TYPE_VENDOR | RECEPIENT_HOST

    // Value & Index can be anything
    __u16 usb_value = 1;
    __u16 usb_index = 2;
    int request_len = CTRL_REQ_LEN;
    int ret = 0;
    
    unsigned int ctrlpipe = usb_sndctrlpipe(usb_drv_device, 0);

    printk("Writing to device\n");    

    ret = copy_from_user(usb_buff, buff, min((size_t)request_len, count));
    usb_control_msg(usb_drv_device, ctrlpipe, usb_request, usb_requesttype, usb_value, 
        usb_index, usb_buff, request_len, 1000);
    // Number of bytes written. If less than 'count',
    // the function will be called again
    return count;
}

struct file_operations fops = {
    .open=usb_drv_open,
    .read=usb_drv_read,
    .write=usb_drv_write
};

int usb_drv_probe (struct usb_interface *intf, const struct usb_device_id *id) {
    int retval = 0;
    printk("Probing device\n");
    usb_drv_device = interface_to_usbdev(intf);
    usb_drv_class.name = DEV_FILE_NAME;
    usb_drv_class.fops = &fops;
    if ((retval = usb_register_dev(intf, &usb_drv_class)) < 0) {
        printk("Cannot register device\n");
    } else {
        printk("Minor obtained: %d\n", intf->minor);
    }
    usb_buff = kmalloc(CTRL_REQ_LEN, GFP_KERNEL);
    return retval;
}

void usb_drv_disconnect(struct usb_interface *intf) {
    printk("Disconnecting device\n");
    kfree(usb_buff);
    usb_deregister_dev(intf, &usb_drv_class);
}

struct usb_driver usb_drv = {
    .name="Test USB device",
    .probe=usb_drv_probe,
    .disconnect=usb_drv_disconnect,
    .id_table=usb_drv_id_table
};

int __init usb_drv_init(void) {
    printk("Initializing\n");
    usb_register(&usb_drv);
    return 0;
}

void __exit usb_drv_exit(void) {
    printk("Exiting\n");
    usb_deregister(&usb_drv);
}

module_init(usb_drv_init);
module_exit(usb_drv_exit);

MODULE_LICENSE("GPL");
