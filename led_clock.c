/***************************************************************************//**
*  \file       led_clock.c
*
*  \details    LED clock
*
*  \author     Andrew Straw
*
*  \Tested with Linux raspberrypi 6.1.21-v7+
*
*******************************************************************************/
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>

// Based on https://github.com/Embetronicx/Tutorials/blob/master/Linux/Device_Driver/High_Resolution_Timer/driver.c
// and https://github.com/Embetronicx/Tutorials/tree/master/Linux/Device_Driver/GPIO-in-Linux-Device-Driver
// for write-ups, see
// https://embetronicx.com/tutorials/linux/device-drivers/using-high-resolution-timer-in-linux-device-driver/
//

//Timer Variable
#define TIMEOUT_NSEC   ( 10000000L )      // 10 msecs
#define TIMEOUT_SEC    ( 0 )

static struct hrtimer ledclock_hr_timer;
static unsigned int count = 0;

// LED is connected to this GPIO
#define GPIO_LED (16)

dev_t dev = 0;
static struct class *dev_class;
static struct cdev ledclock_cdev;

static int __init ledclock_driver_init(void);
static void __exit ledclock_driver_exit(void);


/*************** Driver functions **********************/
static int ledclock_cdev_open(struct inode *inode, struct file *file);
static int ledclock_cdev_release(struct inode *inode, struct file *file);
static ssize_t ledclock_cdev_read(struct file *filp,
                char __user *buf, size_t len,loff_t * off);
static ssize_t ledclock_cdev_write(struct file *filp,
                const char *buf, size_t len, loff_t * off);
/******************************************************/

//File operation structure
static struct file_operations ledclock_fops =
{
  .owner          = THIS_MODULE,
  .read           = ledclock_cdev_read,
  .write          = ledclock_cdev_write,
  .open           = ledclock_cdev_open,
  .release        = ledclock_cdev_release,
};

//Timer Callback function. This will be called when timer expires
enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    int led_value = count++%2;
    gpio_set_value(GPIO_LED, led_value);
    // pr_info("Setting LED on timer callback [%d]\n", led_value);
    hrtimer_forward_now(timer,ktime_set(TIMEOUT_SEC, TIMEOUT_NSEC));
    return HRTIMER_RESTART;
}

/*
** This function will be called when we open the Device file
*/
static int ledclock_cdev_open(struct inode *inode, struct file *file)
{
  pr_info("LED clock device file opened\n");
  return 0;
}

/*
** This function will be called when we close the Device file
*/
static int ledclock_cdev_release(struct inode *inode, struct file *file)
{
  pr_info("LED clock device file released\n");
  return 0;
}

/*
** This function will be called when we read the Device file
*/
static ssize_t ledclock_cdev_read(struct file *filp,
                char __user *buf, size_t len, loff_t *off)
{
  uint8_t gpio_state = 0;

  //reading GPIO value
  gpio_state = gpio_get_value(GPIO_LED);

  //write to user
  len = 1;
  if( copy_to_user(buf, &gpio_state, len) > 0) {
    pr_err("ERROR: Not all the bytes have been copied to user\n");
  }

  pr_info("LED clock read : GPIO_LED = %d \n", gpio_state);

  return 0;
}

/*
** This function will be called when we write the Device file
*/
static ssize_t ledclock_cdev_write(struct file *filp,
                const char __user *buf, size_t len, loff_t *off)
{
  uint8_t rec_buf[10] = {0};

  if( copy_from_user( rec_buf, buf, len ) > 0) {
    pr_err("ERROR: Not all the bytes have been copied from user\n");
  }

  pr_info("LED clock write : GPIO_LED Set = %c\n", rec_buf[0]);

  if (rec_buf[0]=='1') {
    //set the GPIO value to HIGH
    gpio_set_value(GPIO_LED, 1);
  } else if (rec_buf[0]=='0') {
    //set the GPIO value to LOW
    gpio_set_value(GPIO_LED, 0);
  } else {
    pr_err("Unknown command : Please provide either 1 or 0 \n");
  }

  return len;
}

/*
** Module Init function
*/
static int __init ledclock_driver_init(void)
{
  ktime_t ktime;

  /*Allocating Major number*/
  if((alloc_chrdev_region(&dev, 0, 1, "ledclock_Dev")) <0){
    pr_err("Cannot allocate major number\n");
    goto r_unreg;
  }

  /*Creating cdev structure*/
  cdev_init(&ledclock_cdev,&ledclock_fops);

  /*Adding character device to the system*/
  if((cdev_add(&ledclock_cdev,dev,1)) < 0){
    pr_err("Cannot add the device to the system\n");
    goto r_del;
  }

  /*Creating struct class*/
  if(IS_ERR(dev_class = class_create(THIS_MODULE,"ledclock_class"))){
    pr_err("Cannot create the struct class\n");
    goto r_class;
  }

  /*Creating device*/
  if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"ledclock_device"))){
    pr_err( "Cannot create the Device \n");
    goto r_device;
  }

  //Checking the GPIO is valid or not
  if(gpio_is_valid(GPIO_LED) == false){
    pr_err("GPIO %d is not valid\n", GPIO_LED);
    goto r_device;
  }

  //Requesting the GPIO
  if(gpio_request(GPIO_LED,"GPIO_LED") < 0){
    pr_err("ERROR: GPIO %d request\n", GPIO_LED);
    goto r_gpio;
  }

  //configure the GPIO as output
  gpio_direction_output(GPIO_LED, 0);

  ktime = ktime_set(TIMEOUT_SEC, TIMEOUT_NSEC);
  hrtimer_init(&ledclock_hr_timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
  ledclock_hr_timer.function = &timer_callback;
  hrtimer_start( &ledclock_hr_timer, ktime, HRTIMER_MODE_REL);

  pr_info("LED Clock device driver initialized using GPIO %d.\n", GPIO_LED);
  return 0;

r_gpio:
  gpio_free(GPIO_LED);
r_device:
  device_destroy(dev_class,dev);
r_class:
  class_destroy(dev_class);
r_del:
  cdev_del(&ledclock_cdev);
r_unreg:
  unregister_chrdev_region(dev,1);

  return -1;
}

/*
** Module exit function
*/
static void __exit ledclock_driver_exit(void)
{
  hrtimer_cancel(&ledclock_hr_timer);
  gpio_free(GPIO_LED);
  device_destroy(dev_class,dev);
  class_destroy(dev_class);
  cdev_del(&ledclock_cdev);
  unregister_chrdev_region(dev, 1);
  pr_info("LED Clock device driver exit done.\n");
}

module_init(ledclock_driver_init);
module_exit(ledclock_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrew Straw <strawman@astraw.com>");
MODULE_DESCRIPTION("LED clock");
MODULE_VERSION("1.1");
