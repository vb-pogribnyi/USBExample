#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/netlink.h>
#include <net/sock.h>
#define NETLINK_USER 31

#define VENDOR_ID 0x0483
#define PRODUCT_ID 0x1255
#define DEV_FILE_NAME "usbdrv_%d"
#define ISO_BUFF_LENGTH 1024*64

#define CTRL_REQ_LEN 4

const struct usb_device_id usb_drv_id_table[] = {
    {USB_DEVICE(VENDOR_ID, PRODUCT_ID)},
    {}
};

static struct usb_class_driver usb_drv_class;
static struct usb_device *usb_drv_device;
static __u8 *usb_buff;
static struct sock *nl_sock;
static int pid;
struct urb *iso_urb;
static __u8* usb_iso_buffer;

int iso_rx_len = 0;
int iso_packet_size = 8;
int n_iso_packets = 16;
int iso_retries = 0;


static void iso_callback(struct urb* urb) {
    int pkt_idx;
    int i;
    iso_rx_len += urb->actual_length;
    printk("Iso data received: %i bytes (overall %i)\n", urb->actual_length, iso_rx_len);

    /*
    
    iso_urb->number_of_packets = n_iso_packets;
    for (i = 0; i < iso_urb->number_of_packets; i++) {
        iso_urb->iso_frame_desc[i].offset = iso_packet_size * i;
        iso_urb->iso_frame_desc[i].length = iso_packet_size;
        */
    for (pkt_idx = 0; pkt_idx < urb->number_of_packets; pkt_idx++) {
        printk("Packet %i, offset %i, length %i\n", pkt_idx, urb->iso_frame_desc[pkt_idx].offset, urb->iso_frame_desc[pkt_idx].length);
        for (i = 0; i < urb->iso_frame_desc[pkt_idx].length; i++) {
            printk("0x%02x %i ", usb_iso_buffer[urb->iso_frame_desc[pkt_idx].offset + i], i);
        }
        iso_urb->iso_frame_desc[pkt_idx].offset = iso_packet_size * pkt_idx + iso_rx_len;
        iso_urb->iso_frame_desc[pkt_idx].length = iso_packet_size;
        printk("\n");
    }
    printk("\n");


    if (urb->actual_length > 0) iso_retries = 0;
    if (iso_rx_len > 500) return;

    if (iso_retries++ > 3) return;
    usb_submit_urb(urb, GFP_KERNEL);
}

// Send data via netlink socket to a process id with PID saved by nl_receive()
// Effectively, to the last process that was sending data to this socket.
static void nl_send(__u8 *buff, int length) {
    struct sk_buff *skb_out = nlmsg_new(length, 0);
    struct nlmsghdr *nlh;
    int res;

    if (!pid) {
        printk("Recipient PID is not defined. The process must write to the socket first\n");
    }
    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, length, 0);
    NETLINK_CB(skb_out).dst_group = 0;
    strncpy(nlmsg_data(nlh), buff, length);
    res = nlmsg_unicast(nl_sock, skb_out, pid);
    if (res < 0) {
        printk("NLMSG error: %i\n", res);
    }
}

// Callback for when a message is received via netlink
static void nl_receive(struct sk_buff *skb) {
    struct nlmsghdr *nlh;

    nlh = (struct nlmsghdr*)skb->data;
    pid = nlh->nlmsg_pid;
    printk("Received message from pid %i: %s\n", pid, (char*)nlmsg_data(nlh));
    nl_send("Hello there\n", strlen("Hello there\n"));
}

// Called when the device file is opened.
// If not defined, it won't be possible to read or write to the file.
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
    int i = 0;
    
    unsigned int ctrlpipe = usb_sndctrlpipe(usb_drv_device, 0);
    unsigned int isopipe = usb_rcvisocpipe(usb_drv_device, 3);

    printk("Writing to device\n");
    iso_rx_len = 0;

    ret = copy_from_user(usb_buff, buff, min((size_t)request_len, count));
    usb_buff[0] = 1; // Hardcode iso transfer option
    usb_control_msg(usb_drv_device, ctrlpipe, usb_request, usb_requesttype, usb_value, 
        usb_index, usb_buff, request_len, 1000);

    // Start receiving isochronous data
    usb_fill_int_urb(iso_urb, usb_drv_device, isopipe, usb_iso_buffer,
        iso_packet_size * n_iso_packets, iso_callback, 0, 1);
    iso_urb->transfer_flags = URB_ISO_ASAP;
    iso_urb->number_of_packets = n_iso_packets;
    for (i = 0; i < iso_urb->number_of_packets; i++) {
        iso_urb->iso_frame_desc[i].offset = iso_packet_size * i;
        iso_urb->iso_frame_desc[i].length = iso_packet_size;
    }

    iso_retries = 0;
    ret = usb_submit_urb(iso_urb, GFP_KERNEL);
    printk("Sent ISO request with status %i\n", ret);

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

    // Memory allocations for buffers and structures used later
    usb_buff = kmalloc(CTRL_REQ_LEN, GFP_KERNEL);
    usb_iso_buffer = kmalloc(ISO_BUFF_LENGTH, GFP_KERNEL);
    iso_urb = usb_alloc_urb(1, GFP_KERNEL);
    return retval;
}

void usb_drv_disconnect(struct usb_interface *intf) {
    printk("Disconnecting device\n");
    usb_free_urb(iso_urb);
    kfree(usb_buff);
    kfree(usb_iso_buffer);
    usb_deregister_dev(intf, &usb_drv_class);
}

struct usb_driver usb_drv = {
    .name="Test USB device",
    .probe=usb_drv_probe,
    .disconnect=usb_drv_disconnect,
    .id_table=usb_drv_id_table
};

int __init usb_drv_init(void) {
    struct netlink_kernel_cfg cfg = {
        .input=nl_receive,
    };
    printk("Initializing\n");
    usb_register(&usb_drv);
    nl_sock = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    if (!nl_sock) {
        printk("Unable to create socket\n");
    }
    return 0;
}

void __exit usb_drv_exit(void) {
    printk("Exiting\n");
    netlink_kernel_release(nl_sock);
    usb_deregister(&usb_drv);
}

module_init(usb_drv_init);
module_exit(usb_drv_exit);

MODULE_LICENSE("GPL");
