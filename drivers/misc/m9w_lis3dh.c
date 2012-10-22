/*
 * LIS3DH accelerometer driver
 *
 * Copyright (c) 2010 Yamaha Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/earlysuspend.h>

#define LIS3DH_VERSION "1.1.1"
#define LIS3DH_NAME    "st-lis3dh"

#define DEBUG_DELAY 0
//#define TRACE_FUNC() printk(LIS3DH_NAME ": <trace> %s()\n", __FUNCTION__)
#define TRACE_FUNC()

/*
 * Default parameters
 */
#define LIS3DH_DEFAULT_DELAY           200
#define LIS3DH_MAX_DELAY                  1000

/*
 * Registers
 */
#define LIS3DH_WHO_AM_I_REG           0x0f
#define LIS3DH_WHO_AM_I                   0x33

#define LIS3DH_CTRL_REG1                   0x20
#define LIS3DH_CTRL_REG2                   0x21
#define LIS3DH_CTRL_REG3                   0x22
#define LIS3DH_CTRL_REG4                   0x23
#define LIS3DH_CTRL_REG5                   0x24
#define LIS3DH_CTRL_REG6                   0x25

#define LIS3DH_ODR_1HZ                      0x10
#define LIS3DH_ODR_10HZ                    0x20
#define LIS3DH_ODR_25HZ                    0x30
#define LIS3DH_ODR_50HZ                    0x40
#define LIS3DH_ODR_100HZ                  0x50
#define LIS3DH_ODR_200HZ                  0x60
#define LIS3DH_ODR_400HZ                  0x70

#define LIS3DH_X_ENABLE                    0x01
#define LIS3DH_Y_ENABLE                    0x02
#define LIS3DH_Z_ENABLE                    0x04
#define LIS3DH_XYZ_ENABLE                0x07

#define LIS3DH_FS_2G                          0x00
#define LIS3DH_FS_4G                          0x10
#define LIS3DH_FS_8G                          0x20
#define LIS3DH_FS_16G                        0x30

#define LIS3DH_FILTER_ENABLE           0x08
#define LIS3DH_FILTER1                       0
#define LIS3DH_FILTER2                       0x10
#define LIS3DH_FILTER3                       0x20
#define LIS3DH_FILTER4                       0x30

#define LIS3DH_ADC_ENABLE               0x80
#define LIS3DH_TEMP_ENABLE             0x40

#define LIS3DH_HR_ENABLE                  0x08

#define LIS3DH_ACC_REG                      0x28


#define LIS3DH_STATUS_AUX               0x07
#define LIS3DH_OUT_1_L                      0x08
#define LIS3DH_OUT_1_H                     0x09
#define LIS3DH_OUT_2_L                      0x0a
#define LIS3DH_OUT_2_H                     0x0b
#define LIS3DH_OUT_3_L                      0x0c
#define LIS3DH_OUT_3_H                     0x0d
#define LIS3DH_INT_COUNTER              0x0e
#define LIS3DH_TEMP_CFG_REG           0x1f
#define LIS3DH_REFERENCE                  0x26
#define LIS3DH_STATUS_REG               0x27
#define LIS3DH_OUT_X_L                      0x28
#define LIS3DH_OUT_X_H                     0x29
#define LIS3DH_OUT_Y_L                      0x2a
#define LIS3DH_OUT_Y_H                     0x2b
#define LIS3DH_OUT_Z_L                     0x2c
#define LIS3DH_OUT_Z_H                     0x2d
#define LIS3DH_FIFO_CTRL_REG          0x2e
#define LIS3DH_FIFO_SRC_REG            0x2f
#define LIS3DH_INT1_CFG                    0x30
#define LIS3DH_INT1_SRC                    0x31
#define LIS3DH_INT1_THS                    0x32
#define LIS3DH_INT1_DURATION          0x33
#define LIS3DH_CLICK_CFG                   0x38
#define LIS3DH_CLICK_SRC                   0x39
#define LIS3DH_CLICK_THS                   0x3a
#define LIS3DH_TIME_LIMIT                 0x3b
#define LIS3DH_TIME_LATENCY            0x3c
#define LIS3DH_TIME_WINDOW            0x3d


/* ABS axes parameter range [um/s^2] (for input event) */
#define GRAVITY_EARTH                     9806550
#define ABSMIN_2G                         (-GRAVITY_EARTH * 2)
#define ABSMAX_2G                         (GRAVITY_EARTH * 2)

#define LIS3DH_RESOLUTION                  1024
#define LIS3DH_DATA_THRESHOLD         (18 * GRAVITY_EARTH / 1000)  //18mg

/*filter len macro*/
#define LIS3DH_MAX_FILTER_LEN 30
#define LIS3DH_FILTER_LEN          2

/*filter struct defination*/
struct lis3dh_filter {
    int num;
    int filter_len;
    int index;
    int sequence[LIS3DH_MAX_FILTER_LEN];
};

struct acceleration {
    int x;
    int y;
    int z;
};

/*
 * Output data rate
 */
struct lis3dh_odr {
        unsigned long delay;            /* min delay (msec) in the range of ODR */
        u8 odr;                         /* bandwidth register value */
};


static const struct lis3dh_odr lis3dh_odr_table[] = {
    {10,    LIS3DH_ODR_100HZ},
    {20,    LIS3DH_ODR_50HZ},
    {60,    LIS3DH_ODR_25HZ},
    {180,   LIS3DH_ODR_10HZ},
    {1000,  LIS3DH_ODR_1HZ},
};

/*
 * Transformation matrix for chip mounting position
 */
static const int lis3dh_position_map[][3][3] = {
    {{-1,  0,  0}, { 0, -1,  0}, { 0,  0,  1}}, /* top/upper-left */
    {{ 0, -1,  0}, { 1,  0,  0}, { 0,  0,  1}}, /* top/upper-right */
    {{ 1,  0,  0}, { 0,  1,  0}, { 0,  0,  1}}, /* top/lower-right */
    {{ 0,  1,  0}, {-1,  0,  0}, { 0,  0,  1}}, /* top/lower-left */
    {{ 1,  0,  0}, { 0, -1,  0}, { 0,  0, -1}}, /* bottom/upper-left */
    {{ 0,  1,  0}, { 1,  0,  0}, { 0,  0, -1}}, /* bottom/upper-right */
    {{-1,  0,  0}, { 0,  1,  0}, { 0,  0, -1}}, /* bottom/lower-right */
    {{ 0, -1,  0}, {-1,  0,  0}, { 0,  0, -1}}, /* bottom/lower-right */
};

/*
 * driver private data
 */
struct lis3dh_data {
	atomic_t enable;                /* attribute value */
	atomic_t delay;                 /* attribute value */
	atomic_t position;              /* attribute value */
	u8 odr;                         /* output data rate register value */
	struct acceleration last;       /* last measured data */
	struct mutex enable_mutex;
	struct mutex data_mutex;
	struct i2c_client *client;
	struct input_dev *input;
	struct delayed_work work;
	struct lis3dh_filter filter[3];
	int filter_len;
	int suspend_enable;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

#define delay_to_jiffies(d) ((d)?msecs_to_jiffies(d):1)
#define actual_delay(d)     (jiffies_to_msecs(delay_to_jiffies(d)))

static int temperature = 0;



static int lis3dh_i2c_read_byte(struct i2c_client *client,unsigned char adr)
{
	char buf;
	int ret;

	buf = adr;
	ret = i2c_master_send(client, &buf, 1);
	if(ret != 1) {
		dev_err(&client->dev, "failed to transmit instructions to lis3dh.\n");
		return ret;
	}

	ret = i2c_master_recv(client, &buf, 1);
	if (ret != 1) {
		dev_err(&client->dev, "failed to receive response from lis3dh.\n");
		return ret;
	}

	return ret = buf;
}

static int lis3dh_i2c_write_byte(struct i2c_client *client,char adr,char data)
{
	char buf[2];
	int ret;

	buf[0] = adr;
	buf[1] = data;
	ret = i2c_master_send(client, buf, 2);
	if(ret != 2)
		dev_err(&client->dev, "failed to transmit instructions to lis3dh.\n");

	return ret;
}


static int lis3dh_i2c_read_multiplebytes(struct i2c_client *client,unsigned char adr,unsigned char * buf,unsigned char size)
{
    int ret = -1;

    if(size == 0) return ret;

    adr = adr | 0x80;

    ret = i2c_master_send(client, &adr, 1);
    if(ret != 1) {
        dev_err(&client->dev, "failed to transmit instructions to lis3dh.\n");
        return ret;
    }

    ret = i2c_master_recv(client, buf, size);
    if (ret != size) {
        dev_err(&client->dev, "failed to receive response from lis3dh.\n");
        return ret;
    }

    return ret;
}

#if 1
static void filter_init(struct lis3dh_filter *filter, int filter_len)
{
    int i;

    filter->num = 0;
    filter->index = 0;
    filter->filter_len = filter_len;

    for (i = 0; i < filter->filter_len; i++) {
        filter->sequence[i] = 0;
    }
}

static void lis3dh_filter_initialize(struct lis3dh_data *lis3dh, int filter_len)
{
    int i;

    lis3dh->filter_len = filter_len;

    for (i = 0; i < 3; i++)
        filter_init(&lis3dh->filter[i], lis3dh->filter_len);
}

static int get_filter_data(struct lis3dh_filter *filter, int in)
{
    int32_t out = 0;
    int i;

    if (filter->filter_len == 0) {
        return in;
    }
    if (filter->num < filter->filter_len) {
        filter->sequence[filter->index++] = in;
        filter->num++;
        return in;
    }
    else {
        if (filter->filter_len <= filter->index) {
            filter->index = 0;
        }
        filter->sequence[filter->index++] = in;

        for (i = 0; i < filter->filter_len; i++) {
            out += filter->sequence[i];
        }
        return out / filter->filter_len;
    }
}

/*
static void lis3dh_get_filter_datas(struct lis3dh_data *lis3dh, struct acceleration *new, struct acceleration *old)
{
    new->x = get_filter_data(&lis3dh->filter[0], old->x);
    new->y = get_filter_data(&lis3dh->filter[1], old->y);
    new->z = get_filter_data(&lis3dh->filter[2], old->z);
}
*/
#endif

/*
 * Device dependant operations
 */
static int lis3dh_power_up(struct lis3dh_data *lis3dh)
{
    struct i2c_client *client = lis3dh->client;
    u8 data;
    int ret;

    TRACE_FUNC();

    data = lis3dh->odr | LIS3DH_XYZ_ENABLE;
    ret = lis3dh_i2c_write_byte(client, LIS3DH_CTRL_REG1, data);
    if (ret < 0) printk("lis3dh write lis3dh ctrl reg1 error.\n");

    data = LIS3DH_FS_2G| LIS3DH_HR_ENABLE;
    ret = lis3dh_i2c_write_byte(client, LIS3DH_CTRL_REG4, data);
    if (ret < 0) printk("lis3dh write lis3dh ctrl reg4 error.\n");

    return 0;
}

static int lis3dh_power_down(struct lis3dh_data *lis3dh)
{
    struct i2c_client *client = lis3dh->client;
    u8 data;

    TRACE_FUNC();

    data = LIS3DH_XYZ_ENABLE;
    lis3dh_i2c_write_byte(client, LIS3DH_CTRL_REG1, data);

    data = 0;
    lis3dh_i2c_write_byte(client, LIS3DH_CTRL_REG4, data);
    return 0;
}

static int lis3dh_hw_init(struct lis3dh_data *lis3dh)
{
    struct i2c_client *client = lis3dh->client;
    u8 data;

    data = LIS3DH_XYZ_ENABLE;
    lis3dh_i2c_write_byte(client, LIS3DH_CTRL_REG1, data);

    data = 0;
    lis3dh_i2c_write_byte(client, LIS3DH_CTRL_REG2, data);

    data = 0x00;
    lis3dh_i2c_write_byte(client, LIS3DH_CTRL_REG3, data);

    data = LIS3DH_FS_2G;   // 2g range
    lis3dh_i2c_write_byte(client, LIS3DH_CTRL_REG4, data);

    data = 0x00;
    lis3dh_i2c_write_byte(client, LIS3DH_CTRL_REG5, data);

    data = 0x00;
    lis3dh_i2c_write_byte(client, LIS3DH_CTRL_REG6, data);

    data = LIS3DH_ADC_ENABLE | LIS3DH_TEMP_ENABLE;
    lis3dh_i2c_write_byte(client, LIS3DH_TEMP_CFG_REG, data);

    return 0;
}

static int lis3dh_get_enable(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct lis3dh_data *lis3dh = i2c_get_clientdata(client);

    return atomic_read(&lis3dh->enable);
}

static void lis3dh_set_enable(struct device *dev, int enable)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct lis3dh_data *lis3dh = i2c_get_clientdata(client);
    int delay = atomic_read(&lis3dh->delay);

    mutex_lock(&lis3dh->enable_mutex);

    if (enable) {                   /* enable if state will be changed */
        if (!atomic_cmpxchg(&lis3dh->enable, 0, 1)) {
            lis3dh_power_up(lis3dh);
            schedule_delayed_work(&lis3dh->work, delay_to_jiffies(delay) + 1);
        }
    } else {                        /* disable if state will be changed */
        if (atomic_cmpxchg(&lis3dh->enable, 1, 0)) {
            cancel_delayed_work_sync(&lis3dh->work);
            lis3dh_power_down(lis3dh);
        }
    }
    atomic_set(&lis3dh->enable, enable);

    mutex_unlock(&lis3dh->enable_mutex);
}

static int lis3dh_get_delay(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct lis3dh_data *lis3dh = i2c_get_clientdata(client);

    return atomic_read(&lis3dh->delay);
}

static void lis3dh_set_delay(struct device *dev, int delay)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct lis3dh_data *lis3dh = i2c_get_clientdata(client);
    int i;
    u8 data;

   TRACE_FUNC();


    /* determine optimum ODR */
    for (i = 1; (i < ARRAY_SIZE(lis3dh_odr_table)) &&
             (actual_delay(delay) >= lis3dh_odr_table[i].delay); i++)
        ;
    lis3dh->odr = lis3dh_odr_table[i-1].odr;
    atomic_set(&lis3dh->delay, lis3dh_odr_table[i-1].delay);

    mutex_lock(&lis3dh->enable_mutex);

    if (lis3dh_get_enable(dev)) {
        if (atomic_read(&lis3dh->delay) == 20 || atomic_read(&lis3dh->delay) == 10) {
            lis3dh_filter_initialize(lis3dh, LIS3DH_FILTER_LEN);
        }
        cancel_delayed_work_sync(&lis3dh->work);
        data = lis3dh->odr | LIS3DH_XYZ_ENABLE;
	lis3dh_i2c_write_byte(lis3dh->client, LIS3DH_CTRL_REG1, data);
        schedule_delayed_work(&lis3dh->work, delay_to_jiffies(delay));
    }

    mutex_unlock(&lis3dh->enable_mutex);
}

static int lis3dh_get_position(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct lis3dh_data *lis3dh = i2c_get_clientdata(client);

    return atomic_read(&lis3dh->position);
}

static void lis3dh_set_position(struct device *dev, int position)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct lis3dh_data *lis3dh = i2c_get_clientdata(client);

    atomic_set(&lis3dh->position, position);
}


int lis3dh_get_temperature(void)
{
    return temperature;
}


static int lis3dh_measure(struct lis3dh_data *lis3dh, struct acceleration *accel)
{
    struct i2c_client *client = lis3dh->client;
    u8 buf[6];
    int raw[3], data[3];
    int pos = atomic_read(&lis3dh->position);
    int i, j;
#if DEBUG_DELAY
    struct timespec t;
#endif

#if DEBUG_DELAY
    getnstimeofday(&t);
#endif


#if 0
	for (i = 0; i < 6; i++)
		buf[i] = lis3dh_i2c_read_byte(client, LIS3DH_ACC_REG + i);
#else
       lis3dh_i2c_read_multiplebytes(client, LIS3DH_ACC_REG, buf, 6);
#endif

     /*read the temperature*/
    temperature = lis3dh_i2c_read_byte(client, LIS3DH_OUT_3_H);

    /*conver x, y, z value*/
    for (i = 0; i < 3; i++) {
        /* normalization */
		raw[i] = *(s16 *)&buf[i*2];
		raw[i] = raw[i] >> 4;
        raw[i] = (long long)raw[i] * GRAVITY_EARTH / LIS3DH_RESOLUTION;
    }

        /* for X, Y, Z axis */
    for (i = 0; i < 3; i++) {
        /* coordinate transformation */
        data[i] = 0;
        for (j = 0; j < 3; j++) {
            data[i] += raw[j] * lis3dh_position_map[pos][i][j];
        }
    }


#if DEBUG_DELAY
    dev_info(&client->dev, "%ld.%lds:raw(%5d,%5d,%5d) => norm(%8d,%8d,%8d)\n", t.tv_sec, t.tv_nsec,
             raw[0], raw[1], raw[2], data[0], data[1], data[2]);
#endif

    accel->x = data[0];
    accel->y = data[1];
    accel->z = data[2];

    return 0;
}


static void lis3dh_work_func(struct work_struct *work)
{
    struct lis3dh_data *lis3dh = container_of((struct delayed_work *)work, struct lis3dh_data, work);
    struct acceleration accel;
    unsigned long delay = delay_to_jiffies(atomic_read(&lis3dh->delay));
    struct timeval tstart, tend;
    unsigned long pass_time;
    static int event_flag = 0;


    event_flag = (event_flag == 0 ?1:0);

    do_gettimeofday(&tstart);

    lis3dh_measure(lis3dh, &accel);

    mutex_lock(&lis3dh->data_mutex);

    if ((accel.x - lis3dh->last.x > LIS3DH_DATA_THRESHOLD) || (lis3dh->last.x - accel.x > LIS3DH_DATA_THRESHOLD)) {
        if (atomic_read(&lis3dh->delay) == 20 || atomic_read(&lis3dh->delay) == 10) {
            lis3dh->last.x = get_filter_data(&lis3dh->filter[0], accel.x);
        }
        else {
            lis3dh->last.x = accel.x;
        }
    }
    if ((accel.y - lis3dh->last.y > LIS3DH_DATA_THRESHOLD) || (lis3dh->last.y - accel.y > LIS3DH_DATA_THRESHOLD)) {
        if (atomic_read(&lis3dh->delay) == 20 || atomic_read(&lis3dh->delay) == 10) {
            lis3dh->last.y = get_filter_data(&lis3dh->filter[1], accel.y);
        }
        else {
            lis3dh->last.y = accel.y;
        }
    }
    if ((accel.z - lis3dh->last.z > LIS3DH_DATA_THRESHOLD) || (lis3dh->last.z - accel.z  > LIS3DH_DATA_THRESHOLD)) {
        if (atomic_read(&lis3dh->delay) == 20 || atomic_read(&lis3dh->delay) == 10) {
            lis3dh->last.z = get_filter_data(&lis3dh->filter[2], accel.z);
        }
        else {
            lis3dh->last.z = accel.z;
        }
    }

    input_report_abs(lis3dh->input, ABS_X, lis3dh->last.x);
    input_report_abs(lis3dh->input, ABS_Y, lis3dh->last.y);
    input_report_abs(lis3dh->input, ABS_Z, lis3dh->last.z);
    input_report_abs(lis3dh->input, ABS_THROTTLE, event_flag);
    input_sync(lis3dh->input);

    mutex_unlock(&lis3dh->data_mutex);

    do_gettimeofday(&tend);

    pass_time = (tend.tv_sec - tstart.tv_sec) * 1000 + (tend.tv_usec - tstart.tv_usec) / 1000;
    pass_time = delay_to_jiffies(pass_time);

    if (pass_time > delay) delay = 0;
    else delay = delay - pass_time;

    schedule_delayed_work(&lis3dh->work, delay);
}

/*
 * Input device interface
 */
static int lis3dh_input_init(struct lis3dh_data *lis3dh)
{
    struct input_dev *dev;
    int err;

    dev = input_allocate_device();
    if (!dev) {
        return -ENOMEM;
    }
    dev->name = "accelerometer";
    dev->id.bustype = BUS_I2C;

    set_bit(EV_ABS, dev->evbit);
    input_set_capability(dev, EV_ABS, ABS_X);
    input_set_capability(dev, EV_ABS, ABS_Y);
    input_set_capability(dev, EV_ABS, ABS_Z);
    input_set_capability(dev, EV_ABS, ABS_THROTTLE);
    input_set_capability(dev, EV_ABS, ABS_MISC);

    input_set_abs_params(dev, ABS_X, ABSMIN_2G, ABSMAX_2G, 0, 0);
    input_set_abs_params(dev, ABS_Y, ABSMIN_2G, ABSMAX_2G, 0, 0);
    input_set_abs_params(dev, ABS_Z, ABSMIN_2G, ABSMAX_2G, 0, 0);
    input_set_abs_params(dev, ABS_THROTTLE, 0, 1, 0, 0);

    input_set_drvdata(dev, lis3dh);

    err = input_register_device(dev);
    if (err < 0) {
        input_free_device(dev);
        return err;
    }
    lis3dh->input = dev;

    return 0;
}

static void lis3dh_input_fini(struct lis3dh_data *lis3dh)
{
    struct input_dev *dev = lis3dh->input;

    input_unregister_device(dev);
    input_free_device(dev);
}

/*
 * sysfs device attributes
 */
static ssize_t lis3dh_enable_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", lis3dh_get_enable(dev));
}

static ssize_t lis3dh_enable_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    unsigned long enable = simple_strtoul(buf, NULL, 10);

    TRACE_FUNC();

    if ((enable == 0) || (enable == 1)) {
        lis3dh_set_enable(dev, enable);
    }

    return count;
}

static ssize_t lis3dh_delay_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", lis3dh_get_delay(dev));
}

static ssize_t lis3dh_delay_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    unsigned long delay = simple_strtoul(buf, NULL, 10);

    if (delay > LIS3DH_MAX_DELAY) {
        delay = LIS3DH_MAX_DELAY;
    }

    lis3dh_set_delay(dev, delay);

   return count;
}

static ssize_t lis3dh_position_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", lis3dh_get_position(dev));
}

static ssize_t lis3dh_position_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count)
{
    unsigned long position;

    position = simple_strtoul(buf, NULL,10);
    if ((position >= 0) && (position <= 7)) {
        lis3dh_set_position(dev, position);
    }

    return count;
}

static ssize_t lis3dh_wake_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    static atomic_t serial = ATOMIC_INIT(0);

    input_report_abs(input, ABS_MISC, atomic_inc_return(&serial));

    return count;
}

static ssize_t lis3dh_data_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct lis3dh_data *lis3dh = input_get_drvdata(input);
    struct acceleration accel;

    mutex_lock(&lis3dh->data_mutex);
    accel = lis3dh->last;
    mutex_unlock(&lis3dh->data_mutex);

    return sprintf(buf, "%d %d %d\n", accel.x, accel.y, accel.z);
}


static DEVICE_ATTR(enable, S_IRUGO|S_IWUGO,
                   lis3dh_enable_show, lis3dh_enable_store);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUGO,
                   lis3dh_delay_show, lis3dh_delay_store);
static DEVICE_ATTR(position, S_IRUGO|S_IWUGO,
                   lis3dh_position_show, lis3dh_position_store);
static DEVICE_ATTR(wake, S_IWUGO,
                  NULL, lis3dh_wake_store);
static DEVICE_ATTR(data, S_IRUGO,
                  lis3dh_data_show, NULL);

static struct attribute *lis3dh_attributes[] = {
    &dev_attr_enable.attr,
    &dev_attr_delay.attr,
    &dev_attr_position.attr,
    &dev_attr_wake.attr,
    &dev_attr_data.attr,
    NULL
};

static struct attribute_group lis3dh_attribute_group = {
    .attrs = lis3dh_attributes
};

static int lis3dh_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    u8 id;

    id = lis3dh_i2c_read_byte(client, LIS3DH_WHO_AM_I_REG);
    if (id != LIS3DH_WHO_AM_I)
        return -ENODEV;

    return 0;
}


#ifdef CONFIG_PM
static int lis3dh_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct lis3dh_data *lis3dh = i2c_get_clientdata(client);

   TRACE_FUNC();

    mutex_lock(&lis3dh->enable_mutex);

  if (atomic_read(&lis3dh->enable)) {
        cancel_delayed_work_sync(&lis3dh->work);
        lis3dh_power_down(lis3dh);
        atomic_set(&lis3dh->enable, 0);
        lis3dh->suspend_enable = 1;
    }
    else {
	  lis3dh->suspend_enable = 0;
    }

    mutex_unlock(&lis3dh->enable_mutex);

    return 0;
}

static int lis3dh_resume(struct i2c_client *client)
{
    struct lis3dh_data *lis3dh = i2c_get_clientdata(client);
    int delay = atomic_read(&lis3dh->delay);

    TRACE_FUNC();

    mutex_lock(&lis3dh->enable_mutex);

     if (lis3dh->suspend_enable) {
        lis3dh_power_up(lis3dh);
        schedule_delayed_work(&lis3dh->work, delay_to_jiffies(delay) + 1);
        atomic_set(&lis3dh->enable, 1);
        lis3dh->suspend_enable = 0;
    }

    mutex_unlock(&lis3dh->enable_mutex);

    return 0;
}
#else
#define lis3dh_suspend NULL
#define lis3dh_resume NULL
#endif


#ifdef CONFIG_HAS_EARLYSUSPEND
static void lis3dh_early_suspend(struct early_suspend *h)
{
    struct lis3dh_data *lis3dh = container_of(h, struct lis3dh_data, early_suspend);
    lis3dh_suspend(lis3dh->client, PMSG_SUSPEND);
}


static void lis3dh_late_resume(struct early_suspend *h)
{
    struct lis3dh_data *lis3dh = container_of(h, struct lis3dh_data, early_suspend);
    lis3dh_resume(lis3dh->client);
}
#endif


static int lis3dh_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct lis3dh_data *lis3dh;
    int err;

    TRACE_FUNC();

    /* setup private data */
    lis3dh = kzalloc(sizeof(struct lis3dh_data), GFP_KERNEL);
    if (!lis3dh) {
        err = -ENOMEM;
        goto error_0;
    }

    mutex_init(&lis3dh->enable_mutex);
    mutex_init(&lis3dh->data_mutex);

    /* setup i2c client */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        err = -ENODEV;
        goto error_1;
    }

    i2c_set_clientdata(client, lis3dh);
    lis3dh->client = client;

    /* detect and init hardware */
    if ((err = lis3dh_detect(client, NULL))) {
        goto error_1;
    }

    lis3dh_hw_init(lis3dh);
    lis3dh_set_delay(&client->dev, LIS3DH_DEFAULT_DELAY);

    lis3dh_set_position(&client->dev, 6);

    /* setup driver interfaces */
    INIT_DELAYED_WORK(&lis3dh->work, lis3dh_work_func);

    err = lis3dh_input_init(lis3dh);
    if (err < 0) {
        goto error_1;
    }

    err = sysfs_create_group(&lis3dh->input->dev.kobj, &lis3dh_attribute_group);
    if (err < 0) {
        goto error_2;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    lis3dh->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    lis3dh->early_suspend.suspend = lis3dh_early_suspend;
    lis3dh->early_suspend.resume = lis3dh_late_resume;
    register_early_suspend(&lis3dh->early_suspend);
#endif

    return 0;

error_2:
    lis3dh_input_fini(lis3dh);
error_1:
    kfree(lis3dh);
error_0:
    return err;
}

static int lis3dh_remove(struct i2c_client *client)
{
    struct lis3dh_data *lis3dh = i2c_get_clientdata(client);

    TRACE_FUNC();

    unregister_early_suspend(&lis3dh->early_suspend);
    lis3dh_set_enable(&client->dev, 0);
    sysfs_remove_group(&lis3dh->input->dev.kobj, &lis3dh_attribute_group);
    lis3dh_input_fini(lis3dh);

    kfree(lis3dh);

    return 0;
}


static const struct i2c_device_id lis3dh_id[] = {
    {LIS3DH_NAME, 0},
    {},
};

MODULE_DEVICE_TABLE(i2c, lis3dh_id);

static struct i2c_driver lis3dh_driver = {
    .driver = {
        .name = LIS3DH_NAME,
        .owner = THIS_MODULE,
    },
    .probe = lis3dh_probe,
    .remove = lis3dh_remove,
#ifdef CONFIG_PM
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend = lis3dh_suspend,
    .resume = lis3dh_resume,
#endif
#endif
    .id_table = lis3dh_id,
};

static int __init lis3dh_init(void)
{
    return i2c_add_driver(&lis3dh_driver);
}

static void __exit lis3dh_exit(void)
{
    i2c_del_driver(&lis3dh_driver);
}

module_init(lis3dh_init);
module_exit(lis3dh_exit);

MODULE_DESCRIPTION("LIS3DH accelerometer driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(LIS3DH_VERSION);
