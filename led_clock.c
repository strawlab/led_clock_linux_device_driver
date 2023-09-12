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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h>  //copy_to/from_user()
#include <linux/gpio.h>     //GPIO
#include <linux/err.h>

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
  /*Allocating Major number*/
  if((alloc_chrdev_region(&dev, 0, 1, "ledclock_Dev")) <0){
    pr_err("Cannot allocate major number\n");
    goto r_unreg;
  }
  pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));

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

//   /* Using this call the GPIO X will be visible in /sys/class/gpio/
//   ** Now you can change the gpio values by using below commands also.
//   ** echo 1 > /sys/class/gpio/gpioX/value  (turn ON the LED)
//   ** echo 0 > /sys/class/gpio/gpioX/value  (turn OFF the LED)
//   ** cat /sys/class/gpio/gpioX/value  (read the value LED)
//   **
//   ** the second argument prevents the direction from being changed.
//   */
//   gpio_export(GPIO_LED, false);

  pr_info("LED Clock device driver init done.\n");
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
//   gpio_unexport(GPIO_LED);
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
MODULE_AUTHOR("EmbeTronicX <embetronicx@gmail.com>");
MODULE_DESCRIPTION("A simple device led_clock - GPIO Driver");
MODULE_VERSION("1.32");
