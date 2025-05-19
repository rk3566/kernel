#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>


#define MODNAME "virtual_input"

#define ABS_X_MIN	0
#define ABS_X_MAX	1920
#define ABS_Y_MIN	0
#define ABS_Y_MAX	1200

#define MAX_CONTACTS 10    // 10 fingers is it

#define DEVICE_NAME "virtual_input"
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

static int Major;            /* Major number assigned to our device driver */
static int Device_Open = 0;  /* Is device open?  Used to prevent multiple access to the device */

struct class * cl;
struct device * dev; 

struct file_operations fops = {
       read: device_read,
       write: device_write,
       open: device_open,
       release: device_release
};			

static struct input_dev *virt_ts_dev;		// 가상 터치
static struct input_dev *virt_mouse_dev;	// 가상 마우스

//extern int gSuccessFt5x06;
//extern int gSuccessTsc2007;
//int gSuccessTsc2007 = 0;

static int __init virt_ts_init(void)
{
	int err;

#if 0
	printk("gSuccessFt5x06 = %d\n", gSuccessFt5x06);
	if(gSuccessFt5x06 == 0 && gSuccessTsc2007 == 0){	// 터치가 없는제품 SS-100인 경우, event0를 생성해야 virtual mouse를 event1로 생성할수 있기 때문
		virt_ts_dev = input_allocate_device();
		if (!virt_ts_dev)
			return -ENOMEM;

		printk("name:%s :%s\n", virt_ts_dev->name , virt_ts_dev->phys );
		virt_ts_dev->name = "Virtual touchscreen";
		virt_ts_dev->phys = "input0";
		virt_ts_dev->id.bustype = 0;
		virt_ts_dev->id.vendor = 0;



		input_set_abs_params(virt_ts_dev, ABS_X, 0, 1023, 0, 0);
		input_set_abs_params(virt_ts_dev, ABS_Y, 0, 599, 0, 0);
		input_set_abs_params(virt_ts_dev, ABS_MT_POSITION_X, 0, 1023, 0, 0);
		input_set_abs_params(virt_ts_dev, ABS_MT_POSITION_Y, 0, 599, 0, 0);


		set_bit(EV_ABS, virt_ts_dev->evbit);
		set_bit(EV_KEY, virt_ts_dev->evbit);

		virt_ts_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

		input_set_abs_params(virt_ts_dev, ABS_X, ABS_X_MIN, ABS_X_MAX, 0, 0);
		input_set_abs_params(virt_ts_dev, ABS_Y, ABS_Y_MIN, ABS_Y_MAX, 0, 0);

		input_mt_init_slots(virt_ts_dev, MAX_CONTACTS, INPUT_MT_DIRECT);
		input_set_abs_params(virt_ts_dev, ABS_MT_POSITION_X, ABS_X_MIN, ABS_X_MAX, 0, 0);
		input_set_abs_params(virt_ts_dev, ABS_MT_POSITION_Y, ABS_Y_MIN, ABS_Y_MAX, 0, 0);
		err = input_register_device(virt_ts_dev);
		if (err)
			goto fail1;
	}
#endif

	printk("virtual mouse init\n");
	// 가상 마우스
	virt_mouse_dev = input_allocate_device();
	if (!virt_mouse_dev)
		return -ENOMEM;

	virt_mouse_dev->name = "Virtual mouse";
	virt_mouse_dev->phys = "input1";
	virt_mouse_dev->id.bustype = 0;
	virt_mouse_dev->id.vendor = 1;

	input_set_abs_params(virt_mouse_dev, ABS_X, 0, ABS_X_MAX-1, 0, 0);
	input_set_abs_params(virt_mouse_dev, ABS_Y, 0, ABS_Y_MAX-1, 0, 0);
	input_set_abs_params(virt_mouse_dev, ABS_MT_POSITION_X, 0, ABS_X_MAX-1, 0, 0);
	input_set_abs_params(virt_mouse_dev, ABS_MT_POSITION_Y, 0, ABS_Y_MAX-1, 0, 0);


	// ref http://lxr.free-electrons.com/source/include/uapi/linux/input-event-codes.h#L353
	set_bit(EV_SYN, virt_mouse_dev->evbit);	// syn - 0
	set_bit(EV_REL, virt_mouse_dev->evbit); // rel - 2
	set_bit(EV_ABS, virt_mouse_dev->evbit); // rel - 3
	set_bit(REL_X, virt_mouse_dev->relbit);
	set_bit(REL_Y, virt_mouse_dev->relbit);

	set_bit(EV_KEY, virt_mouse_dev->evbit);  /* Event Type is EV_KEY */
	//set_bit(BTN_0,  virt_mouse_dev->keybit); /* Event Code is BTN_0 */
	set_bit(BTN_MOUSE,  virt_mouse_dev->keybit); /* Event Code is BTN_0 */

	err = input_register_device(virt_mouse_dev);
	if (err)
		goto fail2;


	/* above is evdev part. Below is character device part */

	Major = register_chrdev(0, DEVICE_NAME, &fops);	
	if (Major < 0) {
		printk ("Registering the character device failed with %d\n", Major);
		goto fail1;
	}
	printk ("virtual_touchscreen: Major=%d\n", Major);

	cl = class_create(THIS_MODULE, DEVICE_NAME);
	if (!IS_ERR(cl)) {
		dev = device_create(cl, NULL, MKDEV(Major,0), NULL, DEVICE_NAME);
	}


	return 0;

fail1:	input_free_device(virt_ts_dev);
fail2:	input_free_device(virt_mouse_dev);
		return err;
}

static int device_open(struct inode *inode, struct file *file) {
    if (Device_Open) return -EBUSY;
    ++Device_Open;
    return 0;
}
	
static int device_release(struct inode *inode, struct file *file) {
    --Device_Open;
    return 0;
}
	
static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {
    const char* message = 
        "Usage: write the following commands to /dev/virtual_touchscreen:\n"
        "    x num  - move to (x, ...)\n"
        "    y num  - move to (..., y)\n"
        "    d 0    - touch down\n"
        "    u 0    - touch up\n"
        "    s slot - select multitouch slot (0 to 9)\n"
        "    a flag - report if the selected slot is being touched\n"
        "    e 0   - trigger input_mt_report_pointer_emulation\n"
        "    X num - report x for the given slot\n"
        "    Y num - report y for the given slot\n"
        "    S 0   - sync (should be after every block of commands)\n"
        "    M 0   - multitouch sync\n"
        "    T num - tracking ID\n"
        "    also 0123456789:; - arbitrary ABS_MT_ command (see linux/input.h)\n"
        "  each command is char and int: sscanf(\"%c%d\",...)\n"
        "  <s>x and y are from 0 to 1023</s> Probe yourself range of x and y\n"
        "  Each command is terminated with '\\n'. Short writes == dropped commands.\n"
        "  Read linux Documentation/input/multi-touch-protocol.txt to read about events\n";
    const size_t msgsize = strlen(message);
    loff_t off = *offset;
    if (off >= msgsize) {
        return 0;
    }
    if (length > msgsize - off) {
        length = msgsize - off;
    }
    if (copy_to_user(buffer, message+off, length) != 0) {
        return -EFAULT;
    }

    *offset+=length;
    return length;
}
	
#if 1
static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t *off) {

	struct input_event *ev = (struct input_event*)buff;
	//printk("even\n");
	if(ev->type == EV_REL){
		input_report_rel(virt_ts_dev, ev->code, ev->value);
		printk("rel\n");
	}else if(ev->type == EV_SYN){
		input_sync(virt_ts_dev);
		printk("syn\n");
	}


    return len;
}
#else


static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t *off) {
    char command;
    int arg1;

    int i;
    int p=0;
    for(i=0; i<len; ++i) {
        if (buff[i]=='\n') {
            sscanf(buff+p, "%c%d", &command, &arg1);
            p=i+1;
            execute_command(command, arg1);
			printk("cmd:%c arg:%d\n", command, arg1);
        }
    }

    return len;
}
#endif



static void __exit virt_ts_exit(void)
{
#if 0
	if(gSuccessFt5x06 == 0){
		input_unregister_device(virt_ts_dev);
	}
#endif
	input_unregister_device(virt_mouse_dev);

	if (!IS_ERR(cl)) {
		device_destroy(cl, MKDEV(Major,0));
		class_destroy(cl);
	}
	unregister_chrdev(Major, DEVICE_NAME);
}


static struct of_device_id virtual_ts_dt_match[] = {
	{ .compatible = "bestec,virtual_ts",},
	{}
};

MODULE_DEVICE_TABLE(of, virtual_ts_dt_match);

static int virtual_ts_drvier_probe(struct platform_device *pdev)
{
	printk("Virtual Touch Driver\n");
	virt_ts_init();

	return 0;
}

static struct platform_driver virtual_ts_driver = {
	.probe = virtual_ts_drvier_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "virtual_ts",
		.of_match_table = virtual_ts_dt_match,
	},
};

static int __init virtual_ts_init(void)
{
	return platform_driver_register(&virtual_ts_driver);
}

late_initcall(virtual_ts_init);


//module_init(virt_ts_init);
//module_exit(virt_ts_exit);

MODULE_AUTHOR("Vitaly Shukela, vi0oss@gmail.com");
MODULE_DESCRIPTION("Virtual touchscreen driver");
MODULE_LICENSE("GPL");
