/* drivers/i2c/chips/akm8975.c - akm8975 compass driver
 *
 * Copyright (C) 2007-2008 HTC Corporation.
 * Author: Hou-Kun Chen <houkun.chen@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Revised by AKM 2009/04/02
 * 
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include "m9w_akm8975.h"
#include <linux/earlysuspend.h>
#include <plat/gpio-cfg.h>


/*DEBUG definition*/
#define AKM8975_DEBUG		       0
#define AKM8975_DEBUG_MSG	       0
#define AKM8975_DEBUG_FUNC	0
#define AKM8975_DEBUG_DATA	0

#if AKM8975_DEBUG_MSG
#define AKMDBG(format, ...)	printk(KERN_INFO "AKM8975 " format "\n", ## __VA_ARGS__)
#else
#define AKMDBG(format, ...)
#endif

#if AKM8975_DEBUG_FUNC
#define AKMFUNC(func) printk(KERN_INFO "AKM8975 " func " is called\n")
#else
#define AKMFUNC(func)
#endif

#define MAX_FAILURE_COUNT	       3
#define AKM8975_RETRY_COUNT	10
#define AKM8975_DEFAULT_DELAY	200

/*sensor flag*/
#define MAGNETIC_SENSOR_FLAG 0
#define ORIENTATION_SENSOR_FLAG 1

/*sensor name, magnetic or orientation*/
#define MAGNETIC_SENSOR_NAME "magnetic"
#define ORIENTATION_SENSOR_NAME "orientation"

/*input data event definition*/
#define ABS_STATUS                      	(ABS_BRAKE)
#define ABS_WAKE                           (ABS_MISC)

/*global i2c_client*/
static struct i2c_client *this_client;

/*this struct is for magnetic field sensor or orientation sensor*/
struct akm8975_sensor {
    int x;
    int y;
    int z;
    int sensor_flag;
    struct input_dev *input_data;
    atomic_t enable;
    atomic_t delay;
    atomic_t status;
    struct mutex data_lock;
};

/*akm8975 data, include magnetic sensor and orientation sensor*/
struct akm8975_data {
    struct akm8975_sensor magnetic;
    struct akm8975_sensor orientation;
    struct work_struct work;
    struct early_suspend early_suspend;
};

/* Addresses to scan -- protected by sense_data_mutex */
static char sense_data[SENSOR_DATA_SIZE];
static struct mutex sense_data_mutex;
static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static atomic_t data_ready;
static atomic_t open_count;
static atomic_t open_flag;
static atomic_t reserve_open_flag;

static int failure_count = 0;

static short akmd_delay = AKM8975_DEFAULT_DELAY;

static atomic_t suspend_flag = ATOMIC_INIT(0);


/*
 *receive i2c data
*/
static int AKI2C_RxData(char *rxData, int length)
{
    uint8_t loop_i;
    struct i2c_msg msgs[] = {
	{
	 .addr = this_client->addr,
	 .flags = 0,
	 .len = 1,
	 .buf = rxData,
	 },
	{
	 .addr = this_client->addr,
	 .flags = I2C_M_RD,
	 .len = length,
	 .buf = rxData,
	 },
    };
#if AKM8975_DEBUG_DATA
    int i;
    char addr = rxData[0];
#endif
#ifdef AKM8975_DEBUG
    /* Caller should check parameter validity. */
    if ((rxData == NULL) || (length < 1)) {
	return -EINVAL;
    }
#endif
    for (loop_i = 0; loop_i < AKM8975_RETRY_COUNT; loop_i++) {
	if (i2c_transfer(this_client->adapter, msgs, 2) > 0) {
	    break;
	}
	mdelay(10);
    }

    if (loop_i >= AKM8975_RETRY_COUNT) {
	struct akm8975_data *akm = i2c_get_clientdata(this_client);
	if (atomic_read(&akm->magnetic.enable))
	    printk(KERN_ERR "%s retry over %d\n", __func__,
		   AKM8975_RETRY_COUNT);
	return -EIO;
    }
#if AKM8975_DEBUG_DATA
    printk(KERN_INFO "RxData: len=%02x, addr=%02x\n  data=", length, addr);
    for (i = 0; i < length; i++) {
	printk(KERN_INFO " %02x", rxData[i]);
    }
    printk(KERN_INFO "\n");
#endif
    return 0;
}


/*
 *send i2c data
*/
static int AKI2C_TxData(char *txData, int length)
{
    uint8_t loop_i;
    struct i2c_msg msg[] = {
	{
	 .addr = this_client->addr,
	 .flags = 0,
	 .len = length,
	 .buf = txData,
	 },
    };
#if AKM8975_DEBUG_DATA
    int i;
#endif
#ifdef AKM8975_DEBUG
    /* Caller should check parameter validity. */
    if ((txData == NULL) || (length < 2)) {
	return -EINVAL;
    }
#endif
    for (loop_i = 0; loop_i < AKM8975_RETRY_COUNT; loop_i++) {
	if (i2c_transfer(this_client->adapter, msg, 1) > 0) {
	    break;
	}
	mdelay(10);
    }

    if (loop_i >= AKM8975_RETRY_COUNT) {
	struct akm8975_data *akm = i2c_get_clientdata(this_client);
	if (atomic_read(&akm->magnetic.enable))
	    printk(KERN_ERR "%s retry over %d\n", __func__,
		   AKM8975_RETRY_COUNT);
	return -EIO;
    }
#if AKM8975_DEBUG_DATA
    printk(KERN_INFO "TxData: len=%02x, addr=%02x\n  data=", length,
	   txData[0]);
    for (i = 0; i < (length - 1); i++) {
	printk(KERN_INFO " %02x", txData[i + 1]);
    }
    printk(KERN_INFO "\n");
#endif
    return 0;
}


/*
 * set chip single measure mode
*/
static int AKECS_SetMode_SngMeasure(void)
{
    char buffer[2];

    atomic_set(&data_ready, 0);

    /* Set measure mode */
    buffer[0] = AK8975_REG_CNTL;
    buffer[1] = AK8975_MODE_SNG_MEASURE;

    /* Set data */
    return AKI2C_TxData(buffer, 2);
}


/*
 * set chip single measure mode
*/
static int AKECS_SetMode_SelfTest(void)
{
    char buffer[2];

    /* Set measure mode */
    buffer[0] = AK8975_REG_CNTL;
    buffer[1] = AK8975_MODE_SELF_TEST;
    /* Set data */
    return AKI2C_TxData(buffer, 2);
}

static int AKECS_SetMode_FUSEAccess(void)
{
    char buffer[2];

    /* Set measure mode */
    buffer[0] = AK8975_REG_CNTL;
    buffer[1] = AK8975_MODE_FUSE_ACCESS;
    /* Set data */
    return AKI2C_TxData(buffer, 2);
}

static int AKECS_SetMode_PowerDown(void)
{
    char buffer[2];

    /* Set powerdown mode */
    buffer[0] = AK8975_REG_CNTL;
    buffer[1] = AK8975_MODE_POWERDOWN;
    /* Set data */
    return AKI2C_TxData(buffer, 2);
}

static int AKECS_SetMode(char mode)
{
    int ret;

    switch (mode) {
    case AK8975_MODE_SNG_MEASURE:
	ret = AKECS_SetMode_SngMeasure();
	break;
    case AK8975_MODE_SELF_TEST:
	ret = AKECS_SetMode_SelfTest();
	break;
    case AK8975_MODE_FUSE_ACCESS:
	ret = AKECS_SetMode_FUSEAccess();
	break;
    case AK8975_MODE_POWERDOWN:
	ret = AKECS_SetMode_PowerDown();
	/* wait at least 100us after changing mode */
	udelay(100);
	break;
    default:
	AKMDBG("%s: Unknown mode(%d)", __func__, mode);
	return -EINVAL;
    }

    return ret;
}

static int AKECS_CheckDevice(void)
{
    char buffer[2];
    int ret;

    /* Set measure mode */
    buffer[0] = AK8975_REG_WIA;

    /* Read data */
    ret = AKI2C_RxData(buffer, 1);
    if (ret < 0) {
	return ret;
    }
    /* Check read data */
    if (buffer[0] != 0x48) {
	return -ENXIO;
    }

    return 0;
}

static int AKECS_GetData(char *rbuf, int size)
{
#ifdef AKM8975_DEBUG
    /* This function is not exposed, so parameters 
       should be checked internally. */
    if ((rbuf == NULL) || (size < SENSOR_DATA_SIZE)) {
	return -EINVAL;
    }
#endif
    wait_event_interruptible_timeout(data_ready_wq,
				     atomic_read(&data_ready), 1000);
    if (!atomic_read(&data_ready)) {
	AKMDBG("%s: data_ready is not set.", __func__);
	if (!atomic_read(&suspend_flag)) {
	    AKMDBG("%s: suspend_flag is not set.", __func__);
	    failure_count++;
	    if (failure_count >= MAX_FAILURE_COUNT) {
		printk(KERN_ERR
		       "AKM8975 AKECS_GetData: successive %d failure.\n",
		       failure_count);
		atomic_set(&open_flag, -1);
		wake_up(&open_wq);
		failure_count = 0;
	    }
	}
	return -1;
    }

    mutex_lock(&sense_data_mutex);
    memcpy(rbuf, sense_data, size);
    atomic_set(&data_ready, 0);
    mutex_unlock(&sense_data_mutex);

    failure_count = 0;
    return 0;
}

static void AKECS_SetYPR(short *rbuf)
{
    struct akm8975_data *data = i2c_get_clientdata(this_client);
    struct akm8975_sensor *magnetic = &data->magnetic;
    struct akm8975_sensor *orientation = &data->orientation;

#if AKM8975_DEBUG_DATA
    printk(KERN_INFO "AKM8975 %s:\n", __func__);
    printk(KERN_INFO "  yaw =%6d, pitch =%6d, roll =%6d\n",
	   rbuf[0], rbuf[1], rbuf[2]);
    printk(KERN_INFO "  tmp =%6d, m_stat =%6d, g_stat =%6d\n",
	   rbuf[3], rbuf[4], rbuf[5]);
    printk(KERN_INFO "  Acceleration[LSB]: %6d,%6d,%6d\n",
	   rbuf[6], rbuf[7], rbuf[8]);
    printk(KERN_INFO "  Geomagnetism[LSB]: %6d,%6d,%6d\n",
	   rbuf[9], rbuf[10], rbuf[11]);
#endif

    /* Report magnetic sensor information */
    if (atomic_read(&magnetic->enable)) {
	input_report_abs(magnetic->input_data, ABS_X, rbuf[9]);
	input_report_abs(magnetic->input_data, ABS_Y, rbuf[10]);
	input_report_abs(magnetic->input_data, ABS_Z, rbuf[11]);
	input_report_abs(magnetic->input_data, ABS_STATUS, rbuf[4]);
	input_sync(magnetic->input_data);

	mutex_lock(&magnetic->data_lock);

	magnetic->x = rbuf[9];
	magnetic->y = rbuf[10];
	magnetic->z = rbuf[11];
	atomic_set(&magnetic->status, rbuf[4]);

	mutex_unlock(&magnetic->data_lock);
    }

    /* Report magnetic vector information */
    if (atomic_read(&orientation->enable)) {
	input_report_abs(orientation->input_data, ABS_X, rbuf[0]);
	input_report_abs(orientation->input_data, ABS_Y, rbuf[1]);
	input_report_abs(orientation->input_data, ABS_Z, rbuf[2]);
	input_report_abs(orientation->input_data, ABS_STATUS, rbuf[4]);
	input_sync(orientation->input_data);

	mutex_lock(&orientation->data_lock);

	orientation->x = rbuf[0];
	orientation->y = rbuf[1];
	orientation->z = rbuf[2];
	atomic_set(&orientation->status, rbuf[4]);

	mutex_unlock(&orientation->data_lock);
    }
}

static int AKECS_GetOpenStatus(void)
{
    wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
    return atomic_read(&open_flag);
}

static int AKECS_GetCloseStatus(void)
{
    wait_event_interruptible(open_wq, (atomic_read(&open_flag) <= 0));
    return atomic_read(&open_flag);
}

static void AKECS_CloseDone(void)
{
}


static ssize_t akm8975_enable_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct akm8975_sensor *data = input_get_drvdata(input_data);

    AKMFUNC("akm8975_enable_show");
    return sprintf(buf, "%d\n", atomic_read(&data->enable));
}

static ssize_t akm8975_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
    unsigned long enable = simple_strtoul(buf, NULL, 10);
    struct input_dev *input_data = to_input_dev(dev);
    struct akm8975_sensor *data = input_get_drvdata(input_data);

    AKMFUNC("akm8975_enable_store");

    if (atomic_read(&data->enable) == enable)
	return count;

    if ((enable != 0) && (enable != 1)) {
	printk(KERN_ERR "enable is not 0 or 1!\n");
	return count;
    }

    if (data->sensor_flag == MAGNETIC_SENSOR_FLAG) {
	if (enable) {
	    if (atomic_cmpxchg(&open_count, 0, 1) == 0) {
		s3c_gpio_setpin(COMPASS_POWER, 1);	/*power up the sensor */
		if (atomic_cmpxchg(&open_flag, 0, 1) == 0) {
		    atomic_set(&reserve_open_flag, 1);
		    wake_up(&open_wq);
		}
	    }
	} else {
	    atomic_set(&reserve_open_flag, 0);
	    atomic_set(&open_flag, 0);
	    atomic_set(&open_count, 0);
	    wake_up(&open_wq);
	    s3c_gpio_setpin(COMPASS_POWER, 0);	/*power up the sensor */
	}
    }

    atomic_set(&data->enable, enable);

    AKMDBG("%d sensor enable is set to %ld", data->sensor_flag, enable);

    return count;
}

static ssize_t akm8975_delay_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct akm8975_sensor *data = input_get_drvdata(input_data);

    AKMFUNC("akm8975_delay_show");
    return sprintf(buf, "%d\n", atomic_read(&data->delay));
}

static ssize_t akm8975_delay_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct akm8975_sensor *data = input_get_drvdata(input_data);
    unsigned long delay = simple_strtoul(buf, NULL, 10);

    AKMFUNC("akm8975_delay_store");

    if (!atomic_read(&data->enable))
	return count;

    if (delay <= 0)
	delay = 1;
    atomic_set(&data->delay, delay);

    if (data->sensor_flag == MAGNETIC_SENSOR_FLAG)
	akmd_delay = (short) delay;

    return count;
}


static ssize_t akm8975_wake_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    static int cnt = 1;

    input_report_abs(input_data, ABS_WAKE, cnt++);
    return count;
}


static ssize_t akm8975_status_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct akm8975_sensor *data = input_get_drvdata(input_data);

    return sprintf(buf, "%d\n", atomic_read(&data->status));
}

static ssize_t akm8975_data_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct akm8975_sensor *data = input_get_drvdata(input_data);
    int x, y, z;

    mutex_lock(&data->data_lock);

    x = data->x;
    y = data->y;
    z = data->z;

    mutex_unlock(&data->data_lock);

    return sprintf(buf, "%d %d %d\n", x, y, z);
}


static ssize_t akm8975_power_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
    unsigned long power = simple_strtoul(buf, NULL, 10);

    if (power != 0 && power != 1) {
	return -1;
    }

    printk("akm8975_power_store: power set to %ld.\n", power);

    s3c_gpio_setpin(COMPASS_POWER, power);	/*power up the sensor */

    return count;
}


static DEVICE_ATTR(enable, S_IRUGO | S_IWUGO,
		   akm8975_enable_show, akm8975_enable_store);
static DEVICE_ATTR(delay, S_IRUGO | S_IWUGO,
		   akm8975_delay_show, akm8975_delay_store);
static DEVICE_ATTR(wake, S_IWUGO, NULL, akm8975_wake_store);
static DEVICE_ATTR(status, S_IRUGO, akm8975_status_show, NULL);
static DEVICE_ATTR(data, S_IRUGO, akm8975_data_show, NULL);
static DEVICE_ATTR(akm8975_power, S_IWUGO, NULL, akm8975_power_store);


static struct attribute *magnetic_attributes[] = {
    &dev_attr_enable.attr,
    &dev_attr_delay.attr,
    &dev_attr_wake.attr,
    &dev_attr_status.attr,
    &dev_attr_data.attr,
    &dev_attr_akm8975_power.attr,
    NULL
};


static struct attribute *orientation_attributes[] = {
    &dev_attr_enable.attr,
    &dev_attr_delay.attr,
    &dev_attr_wake.attr,
    &dev_attr_status.attr,
    &dev_attr_data.attr,
    NULL
};


static struct attribute_group magnetic_attribute_group = {
    .attrs = magnetic_attributes
};


static struct attribute_group orientation_attribute_group = {
    .attrs = orientation_attributes
};



/***** akmd functions ********************************************/
static int akmd_open(struct inode *inode, struct file *file)
{
    AKMFUNC("akmd_open");
    return nonseekable_open(inode, file);
}

static int akmd_release(struct inode *inode, struct file *file)
{
    AKMFUNC("akmd_release");
    AKECS_CloseDone();
    return 0;
}

static long
akmd_ioctl( struct file *file, unsigned int cmd,
	   unsigned long arg)
{
    void __user *argp = (void __user *) arg;

    /* NOTE: In this function the size of "char" should be 1-byte. */
    char sData[SENSOR_DATA_SIZE];	/* for GETDATA */
    char rwbuf[RWBUF_SIZE];	/* for READ/WRITE */
    char mode;			/* for SET_MODE */
    short value[12];		/* for SET_YPR */
    short delay;		/* for GET_DELAY */
    int status;			/* for OPEN/CLOSE_STATUS */
    int ret = -1;		/* Return value. */
    /*AKMDBG("%s (0x%08X).", __func__, cmd); */

    switch (cmd) {
    case ECS_IOCTL_WRITE:
    case ECS_IOCTL_READ:
	if (argp == NULL) {
	    AKMDBG("invalid argument.");
	    return -EINVAL;
	}
	if (copy_from_user(&rwbuf, argp, sizeof(rwbuf))) {
	    AKMDBG("copy_from_user failed.");
	    return -EFAULT;
	}
	break;
    case ECS_IOCTL_SET_MODE:
	if (argp == NULL) {
	    AKMDBG("invalid argument.");
	    return -EINVAL;
	}
	if (copy_from_user(&mode, argp, sizeof(mode))) {
	    AKMDBG("copy_from_user failed.");
	    return -EFAULT;
	}
	break;
    case ECS_IOCTL_SET_YPR:
	if (argp == NULL) {
	    AKMDBG("invalid argument.");
	    return -EINVAL;
	}
	if (copy_from_user(&value, argp, sizeof(value))) {
	    AKMDBG("copy_from_user failed.");
	    return -EFAULT;
	}
	break;
    default:
	break;
    }

    switch (cmd) {
    case ECS_IOCTL_WRITE:
	AKMFUNC("IOCTL_WRITE");
	if ((rwbuf[0] < 2) || (rwbuf[0] > (RWBUF_SIZE - 1))) {
	    AKMDBG("invalid argument.");
	    return -EINVAL;
	}
	ret = AKI2C_TxData(&rwbuf[1], rwbuf[0]);
	if (ret < 0) {
	    return ret;
	}
	break;
    case ECS_IOCTL_READ:
	AKMFUNC("IOCTL_READ");
	if ((rwbuf[0] < 1) || (rwbuf[0] > (RWBUF_SIZE - 1))) {
	    AKMDBG("invalid argument.");
	    return -EINVAL;
	}
	ret = AKI2C_RxData(&rwbuf[1], rwbuf[0]);
	if (ret < 0) {
	    return ret;
	}
	break;
    case ECS_IOCTL_SET_MODE:
	AKMFUNC("IOCTL_SET_MODE");
	ret = AKECS_SetMode(mode);
	if (ret < 0) {
	    return ret;
	}
	break;
    case ECS_IOCTL_GETDATA:
	AKMFUNC("IOCTL_GET_DATA");
	ret = AKECS_GetData(sData, SENSOR_DATA_SIZE);
	if (ret < 0) {
	    return ret;
	}
	break;
    case ECS_IOCTL_SET_YPR:
	AKECS_SetYPR(value);
	break;
    case ECS_IOCTL_GET_OPEN_STATUS:
	AKMFUNC("IOCTL_GET_OPEN_STATUS");
	status = AKECS_GetOpenStatus();
	AKMDBG("AKECS_GetOpenStatus returned (%d)", status);
	break;
    case ECS_IOCTL_GET_CLOSE_STATUS:
	AKMFUNC("IOCTL_GET_CLOSE_STATUS");
	status = AKECS_GetCloseStatus();
	AKMDBG("AKECS_GetCloseStatus returned (%d)", status);
	break;
    case ECS_IOCTL_GET_DELAY:
	AKMFUNC("IOCTL_GET_DELAY");
	delay = akmd_delay;
	break;
    default:
	return -ENOTTY;
    }

    switch (cmd) {
    case ECS_IOCTL_READ:
	if (copy_to_user(argp, &rwbuf, rwbuf[0] + 1)) {
	    AKMDBG("copy_to_user failed.");
	    return -EFAULT;
	}
	break;
    case ECS_IOCTL_GETDATA:
	if (copy_to_user(argp, &sData, sizeof(sData))) {
	    AKMDBG("copy_to_user failed.");
	    return -EFAULT;
	}
	break;
    case ECS_IOCTL_GET_OPEN_STATUS:
    case ECS_IOCTL_GET_CLOSE_STATUS:
	if (copy_to_user(argp, &status, sizeof(status))) {
	    AKMDBG("copy_to_user failed.");
	    return -EFAULT;
	}
	break;
    case ECS_IOCTL_GET_DELAY:
	if (copy_to_user(argp, &delay, sizeof(delay))) {
	    AKMDBG("copy_to_user failed.");
	    return -EFAULT;
	}
	break;
    default:
	break;
    }

    return 0;
}


/*********************************************/
static struct file_operations akmd_fops = {
    .owner = THIS_MODULE,
    .open = akmd_open,
    .release = akmd_release,
    .unlocked_ioctl = akmd_ioctl,
};


static struct miscdevice akmd_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "akm8975_dev",
    .fops = &akmd_fops,
};

static void akm8975_sensor_free(struct akm8975_sensor *sensor)
{
    if (sensor->sensor_flag == MAGNETIC_SENSOR_FLAG) {
	sysfs_remove_group(&sensor->input_data->dev.kobj,
			   &magnetic_attribute_group);
    } else {
	sysfs_remove_group(&sensor->input_data->dev.kobj,
			   &orientation_attribute_group);
    }
    input_unregister_device(sensor->input_data);
    input_free_device(sensor->input_data);
}

static int akm8975_sensor_init(struct akm8975_sensor *sensor,
			       int sensor_flag, struct i2c_client *client)
{
    int ret;
    struct input_dev *input_data;

    if (sensor_flag != MAGNETIC_SENSOR_FLAG &&
	sensor_flag != ORIENTATION_SENSOR_FLAG) {
	return -1;
    }

    atomic_set(&sensor->enable, 0);
    atomic_set(&sensor->delay, AKM8975_DEFAULT_DELAY);
    atomic_set(&sensor->status, 0);
    mutex_init(&sensor->data_lock);

    input_data = input_allocate_device();
    if (input_data == NULL)
	return -ENOMEM;

    set_bit(EV_ABS, input_data->evbit);
    input_set_capability(input_data, EV_ABS, ABS_X);
    input_set_capability(input_data, EV_ABS, ABS_Y);
    input_set_capability(input_data, EV_ABS, ABS_Z);

    input_set_capability(input_data, EV_ABS, ABS_STATUS);	/* status */
    input_set_capability(input_data, EV_ABS, ABS_WAKE);	/* wake */

    if (sensor_flag == MAGNETIC_SENSOR_FLAG)
	input_data->name = MAGNETIC_SENSOR_NAME;
    else
	input_data->name = ORIENTATION_SENSOR_NAME;

    if (sensor_flag == MAGNETIC_SENSOR_FLAG) {
	/* x, y, z axis of raw magnetic vector (-4096, 4095) */
	input_set_abs_params(input_data, ABS_X, -20480, 20479, 0, 0);
	input_set_abs_params(input_data, ABS_Y, -20480, 20479, 0, 0);
	input_set_abs_params(input_data, ABS_Z, -20480, 20479, 0, 0);
	input_set_abs_params(input_data, ABS_STATUS, -32768, 3, 0, 0);
    } else {			//set orientation value range       
	input_set_abs_params(input_data, ABS_X, 0, 23040, 0, 0);	/*yaw (0, 360) */
	input_set_abs_params(input_data, ABS_Y, -11520, 11520, 0, 0);	/*pitch (-180, 180) */
	input_set_abs_params(input_data, ABS_Z, -5760, 5760, 0, 0);	/*roll (-90, 90) */
	input_set_abs_params(input_data, ABS_STATUS, -32768, 3, 0, 0);
    }

    sensor->sensor_flag = sensor_flag;

    input_data->dev.parent = &client->dev;
    ret = input_register_device(input_data);
    if (ret < 0) {
	printk("%s register input device error!\n", input_data->name);
	goto exit1;
    }
    sensor->input_data = input_data;
    input_set_drvdata(sensor->input_data, sensor);

    if (sensor_flag == MAGNETIC_SENSOR_FLAG) {
	ret =
	    sysfs_create_group(&sensor->input_data->dev.kobj,
			       &magnetic_attribute_group);
    } else {
	ret =
	    sysfs_create_group(&sensor->input_data->dev.kobj,
			       &orientation_attribute_group);
    }
    if (ret < 0) {
	printk("%s create sysfs error!\n", input_data->name);
	goto exit2;
    }

    return 0;
  exit2:
    input_unregister_device(input_data);
  exit1:
    input_free_device(input_data);

    return ret;
}


#ifdef CONFIG_PM
static int akm8975_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct akm8975_data *akm = i2c_get_clientdata(this_client);

    AKMFUNC("akm8975_suspend");
    atomic_set(&suspend_flag, 1);
    atomic_set(&reserve_open_flag, atomic_read(&open_flag));
    atomic_set(&open_flag, 0);
    wake_up(&open_wq);

    if (atomic_read(&akm->magnetic.enable)) {
	s3c_gpio_setpin(COMPASS_POWER, 0);	/*power down the sensor */
    }

    AKMDBG("suspended with flag=%d", atomic_read(&reserve_open_flag));

    return 0;
}

static int akm8975_resume(struct i2c_client *client)
{
    struct akm8975_data *akm = i2c_get_clientdata(this_client);

    AKMFUNC("akm8975_resume");

    if (atomic_read(&akm->magnetic.enable)) {
	s3c_gpio_setpin(COMPASS_POWER, 1);	/*power up the sensor */
    }

    atomic_set(&suspend_flag, 0);
    atomic_set(&open_flag, atomic_read(&reserve_open_flag));
    wake_up(&open_wq);
    AKMDBG("resumed with flag=%d", atomic_read(&reserve_open_flag));

    return 0;
}
#else
#define akm8975_suspend NULL
#define akm8975_resume NULL
#endif


#ifdef CONFIG_HAS_EARLYSUSPEND
static void akm8975_early_suspend(struct early_suspend *h)
{
    akm8975_suspend(this_client, PMSG_SUSPEND);
}


static void akm8975_late_resume(struct early_suspend *h)
{
    akm8975_resume(this_client);
}
#endif


int akm8975_probe(struct i2c_client *client,
		  const struct i2c_device_id *id)
{
    struct akm8975_data *akm = NULL;
    int err = 0;

    AKMFUNC("akm8975_probe");

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
	printk(KERN_ERR
	       "AKM8975 akm8975_probe: check_functionality failed.\n");
	err = -ENODEV;
	return err;
    }

    /* Allocate memory for driver data */
    akm = kzalloc(sizeof(struct akm8975_data), GFP_KERNEL);
    if (!akm) {
	printk(KERN_ERR
	       "AKM8975 akm8975_probe: memory allocation failed.\n");
	err = -ENOMEM;
	return err;
    }


    i2c_set_clientdata(client, akm);

    this_client = client;

    /*power on */
    s3c_gpio_setpin(COMPASS_POWER, 1);

    /* Check connection */
    err = AKECS_CheckDevice();
    if (err < 0) {
	printk(KERN_ERR
	       "AKM8975 akm8975_probe: set power down mode error\n");
	goto exit1;
    }

    err =
	akm8975_sensor_init(&akm->magnetic, MAGNETIC_SENSOR_FLAG,
			    this_client);
    if (err < 0) {
	printk(KERN_ERR
	       "AKM8975 akm8975_probe: magnetic sensor init error\n");
	goto exit1;
    }

    err =
	akm8975_sensor_init(&akm->orientation, ORIENTATION_SENSOR_FLAG,
			    this_client);
    if (err < 0) {
	printk(KERN_ERR
	       "AKM8975 akm8975_probe: orientation sensor init error\n");
	goto exit2;
    }

    err = misc_register(&akmd_device);
    if (err) {
	printk(KERN_ERR
	       "AKM8975 akm8975_probe: akmd_device register failed\n");
	goto exit3;
    }

    init_waitqueue_head(&data_ready_wq);
    init_waitqueue_head(&open_wq);


    /*power down */
    s3c_gpio_setpin(COMPASS_POWER, 0);

#ifdef CONFIG_HAS_EARLYSUSPEND
    akm->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    akm->early_suspend.suspend = akm8975_early_suspend;
    akm->early_suspend.resume = akm8975_late_resume;
    register_early_suspend(&akm->early_suspend);
#endif

    AKMDBG("successfully probed.");

    return 0;

  exit3:
    akm8975_sensor_free(&akm->orientation);
  exit2:
    akm8975_sensor_free(&akm->magnetic);
  exit1:
    kfree(akm);
    s3c_gpio_setpin(COMPASS_POWER, 0);

    return err;
}

static int akm8975_remove(struct i2c_client *client)
{
    struct akm8975_data *akm = i2c_get_clientdata(client);
    unregister_early_suspend(&akm->early_suspend);
    misc_deregister(&akmd_device);
    akm8975_sensor_free(&akm->orientation);
    akm8975_sensor_free(&akm->magnetic);
    kfree(akm);
    AKMDBG("successfully removed.");
    return 0;
}

static const struct i2c_device_id akm8975_id[] = {
    {AKM8975_I2C_NAME, 0},
    {}
};

static struct i2c_driver akm8975_driver = {
    .probe = akm8975_probe,
    .remove = akm8975_remove,
#ifdef CONFIG_PM
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend = akm8975_suspend,
    .resume = akm8975_resume,
#endif
#endif
    .id_table = akm8975_id,
    .driver = {
	       .name = AKM8975_I2C_NAME,
	       },
};

static int __init akm8975_init(void)
{
    AKMDBG("compass driver: initialize");
    return i2c_add_driver(&akm8975_driver);
}

static void __exit akm8975_exit(void)
{
    AKMDBG("compass driver: release");
    i2c_del_driver(&akm8975_driver);
}

module_init(akm8975_init);
module_exit(akm8975_exit);

MODULE_AUTHOR("Meizu");
MODULE_DESCRIPTION("AKM8975 compass driver");
MODULE_LICENSE("GPL");
