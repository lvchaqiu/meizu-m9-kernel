/*
 *  gp2ap012a00f.c - sharp proximity and  ambient  light  sensor driver
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <linux/hwmon-sysfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/cpufreq.h>


//#include <plat/gpio-cfg.h>
#include <linux/gpio.h>

#define GP2AP_DRV_NAME "sharp-gp2ap012a00f"

/*for debug*/
#define GP2AP_DATA_DEBUG 0
#define GP2AP_FUNC_DEBUG 0

#if GP2AP_DATA_DEBUG
#define g_dbg(fmt...) 	printk( fmt)
#define g_err(fmt...) 	printk( fmt)
#else
#define g_dbg(fmt...)
#define g_err(fmt...)
#endif

#if GP2AP_FUNC_DEBUG
#define TRACE_FUNC() printk(GP2AP_DRV_NAME ": <trace> %s()\n", __FUNCTION__)	
#else
#define TRACE_FUNC()
#endif

#define PROXIMITY_CALIB_FILE "/data/calibration/proximity_calibration"

/*gp2ap registers definitions*/
#define COMMAND1_REG		     0x00      //default 0
#define COMMAND2_REG		     0x01      //0
#define COMMAND3_REG		     0x02      //0
#define DATA_LSB_REG		     0x03      //0
#define DATA_MSB_REG		     0x04     //0
#define LT_LSB_REG		            0x05     //0
#define LT_MSB_REG		            0x06     //0
#define HT_LSB_REG		            0x07     //0xff
#define HT_MSB_REG    	            0x08     //0xff

/*operating mode*/
#define SHUTDOWN_MODE      0
#define OPERATING_MODE      0x80

#define AUTO_SHUTDOWN_MODE             0
#define CONTINUE_OPERATING_MODE     0x40

#define ALS_MODE                     0
#define PROXIMITY_MODE         0x30
#define LED_OFF_MODE            0x10
#define LED_ON_MODE              0x20

/*measure cycles*/
#define MEASURE_CYCLES1        0
#define MEASURE_CYCLES4        0x01
#define MEASURE_CYCLES8        0x02
#define MEASURE_CYCLES16      0x03

/*data resolution*/
#define RESOLUTION16              0
#define RESOLUTION14              0x10
#define RESOLUTION12              0x20
#define RESOLUTION10              0x30
#define RESOLUTION8                0x40
#define RESOLUTION6                0x50

/*data range*/
#define RANGE_1024_X1           0
#define RANGE_2048_X2           0x01
#define RANGE_3072_X3           0x02
#define RANGE_4096_X4           0x03
#define RANGE_6144_X3           0x04
#define RANGE_8192_X4           0x05
#define RANGE_10240_X5         0x06
#define RANGE_12288_X6         0x07
#define RANGE_16384_X4         0x08
#define RANGE_32768_X8         0x09
#define RANGE_49152_X12       0x0a
#define RANGE_65536_X16       0x0b
#define RANGE_81920_X10       0x0c
#define RANGE_98304_X12       0x0d
#define RANGE_114688_X14     0x0e
#define RANGE_131072_X16     0x0f

/*intval time between two measurements*/
#define INTVAL_1                      0
#define INTVAL_2                      0
#define INTVAL_4                      0x40
#define INTVAL_8                      0x80
#define INTVAL_16                    0xc0

/*led current for proximity mode*/
#define LED_CURRENT_60	        0
#define LED_CURRENT_120	 0x10
#define LED_CURRENT_180	 0x20
#define LED_CURRENT_240	 0x30

/*interrupt mode*/
#define INTERRUPT_OUTPUT	 0
#define DETECTION_OUTPUT     0x08

/*led frequency*/
#define LED_FREQUENCY_327_5    0
#define LED_FREQUENCY_DC          0x02


/*other definitions*/

/*sensor name*/
#define LIGHT_SENSOR_NAME 				"light"
#define PROXIMITY_SENSOR_NAME 		"proximity"

/*sensor default delay*/
#define LIGHT_DEFAULT_DELAY            		(500)   /*500 ms */
#define PROX_DEFAULT_DELAY            		(200)  
#define SENSOR_MAX_DELAY                		(2000)  /* 2000 ms */

/*sensor max value to report*/
#define LIGHT_MAX_VALUE 	(1024 * 64 * 1000)
#define PROX_MAX_VALUE 	(10 * 1000)

/*sensor type*/
#define LIGHT_SENSOR 0
#define PROX_SENSOR 1

/*sensor mode*/
#define SENSOR_NONE_MODE   	0
#define SENSOR_LIGHT_MODE  	1
#define SENSOR_PROX_MODE     	2
#define SENSOR_ALL_MODE		3

/*sensor enable flag*/
#define SENSOR_DISABLE 	              0
#define SENSOR_ENABLE 	              1

#define PROX_FLAG                         0x08
#define INT_FLAG                            0x04

/*input data event definition*/
#define ABS_STATUS                      	(ABS_BRAKE)
#define ABS_WAKE                           (ABS_MISC)
#define ABS_CONTROL_REPORT       (ABS_THROTTLE)

#define delay_to_jiffies(d) 	       ((d)?msecs_to_jiffies(d):1)
#define actual_delay(d)     	       (jiffies_to_msecs(delay_to_jiffies(d)))

#define PROX_THRESHOLD_COUNT 4
#define LED_OFF_THRESHOLD 500


/*light sensor or prox sensor struct*/
struct sensor_data {
	int enable;
	int delay;
	struct mutex enable_lock;
	struct mutex data_lock;
	struct input_dev *input_data;
	int data;
	int sensor;
	int level;
};

/*gp2ap struct*/
struct gp2ap_data {
	struct sensor_data light;
	struct sensor_data prox;
	struct work_struct  prox_work; 
	struct delayed_work  light_delayed_work; 
	struct i2c_client *client;
	struct mutex lock;
	spinlock_t irq_lock;
	int sensor_mode;
	int suspend_mode;
	unsigned int irq;
	int prox_calib_value;
	int prox_threshold;
	int measure_flag;
	int first_measure_flag;
};


/*the gp2ap workqueue*/
static struct workqueue_struct *gp2ap_wq;


static int gp2ap_i2c_read_byte(struct i2c_client *client,unsigned char adr)
{
	char buf;
	int ret;

	buf = adr;

	ret =i2c_master_send(client, &buf, 1);
	if(ret != 1) {
		dev_err(&client->dev, "failed to transmit instructions to gp2ap in gp2ap_i2c_read_byte.\n");
		return -1;
	}
	ret = i2c_master_recv(client, &buf, 1);
	if (ret != 1) {
		dev_err(&client->dev, "failed to receive response from gp2ap in gp2ap_i2c_read_byte.\n");
		return -1;
	}

	return ret = buf;
}


static int gp2ap_i2c_write_byte(struct i2c_client *client,char adr,char data)
{
	char buf[2];
	int ret;

	buf[0] = adr;
	buf[1] = data;
	ret = i2c_master_send(client, buf, 2);
	if(ret != 2) {
		dev_err(&client->dev, "failed to transmit instructions to  gp2ap in gp2ap_i2c_write_byte.\n");
		return -1;
	}

	return ret;
}


static int gp2ap_i2c_read_multiplebytes(struct i2c_client *client,unsigned char adr,unsigned char * buf,unsigned char size)
{   
	int ret = -1;   

	if(size == 0) return -1; 

	adr = adr | 0x80;

	ret = i2c_master_send(client, &adr, 1); 
	if(ret != 1) {       
		dev_err(&client->dev, "failed to transmit instructions to lis3dh.\n");      
		return -1; 
	}       

	ret = i2c_master_recv(client, buf, size);
	if (ret != size) {
		dev_err(&client->dev, "failed to receive response from lis3dh.\n");
		return -1;
	}        

	return ret;
}


static int gp2ap_i2c_write_multiplebytes(struct i2c_client *client,unsigned char adr,unsigned char * send_buf,unsigned char size)
{   
	char buf[size + 1];
	int ret = -1, i;

	if (size == 0) return -1;

	buf[0] = adr;
	for (i = 0; i < size; i++) buf[i + 1] = send_buf[i];

	ret = i2c_master_send(client, buf, size + 1);

	if(ret != (size + 1)) {
		dev_err(&client->dev, "failed to transmit instructions to  gp2ap in gp2ap_i2c_write_byte.\n");
		return -1;
	}

	return ret;
}

static int gp2ap_detect(struct i2c_client *client)
{
	int ret = 0;
	ret = gp2ap_i2c_read_byte(client,COMMAND1_REG);	
	return ret;
}


static int get_adc_data(struct gp2ap_data *gp2ap)
{
	int reg_data, ret;
	u8 buf[2];

	ret = gp2ap_i2c_read_multiplebytes(gp2ap->client, DATA_LSB_REG, buf, 2);
	if (ret < 0) {
		printk("read proximity data error in get_adc_data()!\n");
		return ret;
	}

	reg_data = buf[1] << 8;
	reg_data = reg_data  + buf[0];

	return reg_data;
}


#if 0
static void print_all_registers(struct i2c_client *client)
{
	int data, i;

	for (i = 0; i < 9; i++)
	{
		data = gp2ap_i2c_read_byte(client, i);
		g_dbg("%d:0x%.2x ", i, data);	
	}
	g_dbg("\n");
}
#endif


static void gp2ap_set_light(struct i2c_client *client)
{
	u8 buf[4];

	buf[0] = 0;
	buf[1] = RESOLUTION16 | RANGE_1024_X1;
	buf[2] = INTVAL_1 | DETECTION_OUTPUT | LED_FREQUENCY_327_5;

	gp2ap_i2c_write_multiplebytes(client, COMMAND1_REG, buf, 3);

	buf[0] = 0xfe;
	buf[1] = 0xff;
	buf[2] = 0xff;
	buf[3] = 0xff;

	gp2ap_i2c_write_multiplebytes(client, LT_LSB_REG, buf, 4);

	buf[0] = OPERATING_MODE | AUTO_SHUTDOWN_MODE | ALS_MODE | MEASURE_CYCLES1;
	gp2ap_i2c_write_byte(client, COMMAND1_REG,  buf[0]);
}

#if 0
static void gp2ap_set_proximity(struct i2c_client *client, int threshold)
{
	unsigned char data;

	printk("in gp2ap_set_proximity: threshold is %d.\n", threshold);

	data = 0;
	gp2ap_i2c_write_byte(client, COMMAND1_REG, data);

	data = RESOLUTION12 | RANGE_131072_X16;
	gp2ap_i2c_write_byte(client, COMMAND2_REG, data);

	data = INTVAL_1 | LED_CURRENT_240 | INTERRUPT_OUTPUT | LED_FREQUENCY_327_5;
	gp2ap_i2c_write_byte(client, COMMAND3_REG, data);

	data = (threshold - 6) % 256;;
	gp2ap_i2c_write_byte(client, LT_LSB_REG, data);

	data = ((threshold - 6) / 256) & 0xff;;
	gp2ap_i2c_write_byte(client, LT_MSB_REG, data);

	/*set adc data 600, 0x258*/
	data = threshold % 256;
	gp2ap_i2c_write_byte(client, HT_LSB_REG, data);	

	data = (threshold  / 256) & 0xff;  
	gp2ap_i2c_write_byte(client, HT_MSB_REG, data); 

	data = OPERATING_MODE | CONTINUE_OPERATING_MODE | PROXIMITY_MODE | MEASURE_CYCLES4;
	gp2ap_i2c_write_byte(client, COMMAND1_REG,  data);
}
#endif

static void gp2ap_set_led_off_mode(struct i2c_client *client)
{
	u8 buf[3];

	buf[0] = 0;
	buf[1] = RESOLUTION12 | RANGE_131072_X16;
	buf[2] = INTVAL_1 | LED_CURRENT_240 | DETECTION_OUTPUT | LED_FREQUENCY_327_5;

	gp2ap_i2c_write_multiplebytes(client, COMMAND1_REG, buf, 3);

	buf[0] = OPERATING_MODE | AUTO_SHUTDOWN_MODE | LED_OFF_MODE| MEASURE_CYCLES1;
	gp2ap_i2c_write_byte(client, COMMAND1_REG,  buf[0]);
}



static void gp2ap_set_led_on_mode(struct i2c_client *client)
{
	u8 buf[3];

	buf[0] = 0;
	buf[1] = RESOLUTION12 | RANGE_131072_X16;
	buf[2] = INTVAL_1 | LED_CURRENT_240 | DETECTION_OUTPUT | LED_FREQUENCY_327_5;

	gp2ap_i2c_write_multiplebytes(client, COMMAND1_REG, buf, 3);

	buf[0] = OPERATING_MODE | AUTO_SHUTDOWN_MODE | LED_ON_MODE| MEASURE_CYCLES1;
	gp2ap_i2c_write_byte(client, COMMAND1_REG,  buf[0]);
}


static void gp2ap_set_shutdown_mode(struct i2c_client *client)
{
	u8 buf[3];

	buf[0] = 0;
	buf[1] = RESOLUTION12 | RANGE_131072_X16;
	buf[2] = INTVAL_1 | LED_CURRENT_240 | DETECTION_OUTPUT | LED_FREQUENCY_327_5;

	gp2ap_i2c_write_multiplebytes(client, COMMAND1_REG, buf, 3);

	buf[0] = OPERATING_MODE | AUTO_SHUTDOWN_MODE | PROXIMITY_MODE | MEASURE_CYCLES1;
	gp2ap_i2c_write_byte(client, COMMAND1_REG,  buf[0]);
}


/*
   static void power_up(struct gp2ap_data *gp2ap)
   {
   int command;

   TRACE_FUNC();

   command =  gp2ap_i2c_read_byte(gp2ap->client, COMMAND1_REG);
   command |= OPERATING_MODE;
   gp2ap_i2c_write_byte(gp2ap->client,COMMAND1_REG, command); 
   }
   */

static void power_down(struct gp2ap_data *gp2ap)
{
	int command;

	TRACE_FUNC();

	command =  gp2ap_i2c_read_byte(gp2ap->client, COMMAND1_REG);
	command &= (~OPERATING_MODE);
	gp2ap_i2c_write_byte(gp2ap->client,COMMAND1_REG, command); 
}

static void prox_data_init(struct sensor_data *prox)
{
	mutex_lock(&prox->data_lock);
	prox->data = 10;
	mutex_unlock(&prox->data_lock);
}

static int read_prox_calibvalue(void) 
{
	struct file *fp;
	char buf[256];
	loff_t  pos = 0;
	ssize_t rb;
	int calib_value = 100;

	//here to read calib_value
	fp  = filp_open(PROXIMITY_CALIB_FILE, O_RDONLY, 0);
	if (IS_ERR(fp)) {   //if error, set shut_down mode to read one
		printk("open %s error!\n", PROXIMITY_CALIB_FILE);
	} else {
		rb = kernel_read(fp, pos, buf, sizeof(buf));
		if (rb>0) {
			buf[rb] = '\0';
			calib_value = simple_strtol(buf, NULL, 10);
			printk(KERN_DEBUG "in read_prox_calibvalue: rb is %d, read buf is %s, calib_value is %d\n", rb, buf, calib_value);
		} else {
			printk("read %s file error!, rb is %d\n", PROXIMITY_CALIB_FILE, rb);
		}
		filp_close(fp, NULL);   //recover fs
	}

	return calib_value;
}

static int get_prox_threshold(void)
{
	return (read_prox_calibvalue() + 20);
}

static void light_data_init(struct sensor_data *light)
{
	mutex_lock(&light->data_lock);
	light->data = 0;
	mutex_unlock(&light->data_lock);

	light->level = -1;
}

static void prox_set_enable(struct sensor_data *data, int enable)
{
	struct gp2ap_data *gp2ap = container_of(data, struct gp2ap_data, prox);
	int sensor_mode;

	TRACE_FUNC();

	mutex_lock(&data->enable_lock);

	if (enable) {                   /* enable if state will be changed */
		if (data->enable) {
			g_dbg("prox sensor has been enabled!\n");
			goto out;		
		}

		mutex_lock(&gp2ap->lock);        //get lock, change mode

		sensor_mode = gp2ap->sensor_mode;
		switch (sensor_mode) {
			case SENSOR_NONE_MODE:
				prox_data_init(&gp2ap->prox);
				if (!gp2ap->prox_threshold)
					gp2ap->prox_threshold = get_prox_threshold();

				gp2ap->sensor_mode = SENSOR_PROX_MODE;
				gp2ap->measure_flag = 1;
				gp2ap->first_measure_flag = 1;
				queue_work(gp2ap_wq, &gp2ap->prox_work);

				g_dbg("change to sensor prox mode. \n");
				break;
			case SENSOR_LIGHT_MODE:
				cancel_delayed_work_sync(&gp2ap->light_delayed_work);
				prox_data_init(&gp2ap->prox);
				if (!gp2ap->prox_threshold)
					gp2ap->prox_threshold = get_prox_threshold();

				gp2ap->sensor_mode = SENSOR_ALL_MODE;
				gp2ap->measure_flag = 1;
				gp2ap->first_measure_flag = 1;
				queue_work(gp2ap_wq, &gp2ap->prox_work);

				g_dbg("change to sensor all mode.\n");
				break;
			default:
				break;
		}
		mutex_unlock(&gp2ap->lock);
	} else {                    /* disable if state will be changed */
		if (!data->enable) {
			g_dbg("prox sensor has been disable!\n");
			goto out;	
		}

		mutex_lock(&gp2ap->lock);

		sensor_mode = gp2ap->sensor_mode;
		switch (sensor_mode) {
			case SENSOR_PROX_MODE:
				gp2ap->measure_flag = 0;
				cancel_work_sync(&gp2ap->prox_work);
				gp2ap->first_measure_flag = 0;

				gp2ap->sensor_mode = SENSOR_NONE_MODE;
				power_down(gp2ap);
				g_dbg("change to sensor none mode.\n");
				break;
			case SENSOR_ALL_MODE:
				gp2ap->measure_flag = 0;
				cancel_work_sync(&gp2ap->prox_work);
				gp2ap->first_measure_flag = 0;

				light_data_init(&gp2ap->light);
				gp2ap_set_light(gp2ap->client);
				gp2ap->sensor_mode =  SENSOR_LIGHT_MODE;
				queue_delayed_work(gp2ap_wq, &gp2ap->light_delayed_work, data->delay);
				g_dbg("change to sensor light mode.\n");
				break;
			default:
				break;
		}
		mutex_unlock(&gp2ap->lock);
	}

	data->enable = enable;
out:
	mutex_unlock(&data->enable_lock);
}


static void light_set_enable(struct sensor_data *data, int enable)
{
	struct gp2ap_data *gp2ap = container_of(data, struct gp2ap_data, light);
	int sensor_mode;

	TRACE_FUNC();

	mutex_lock(&data->enable_lock);

	if (enable) {                   /* enable if state will be changed */
		if (data->enable) {
			g_dbg("light sensor has been enabled!\n");
			goto out;		
		}

		mutex_lock(&gp2ap->lock);

		sensor_mode = gp2ap->sensor_mode;
		switch (sensor_mode) {
			case SENSOR_NONE_MODE:
				light_data_init(&gp2ap->light);
				gp2ap_set_light(gp2ap->client);
				gp2ap->sensor_mode = SENSOR_LIGHT_MODE;
				queue_delayed_work(gp2ap_wq, &gp2ap->light_delayed_work, data->delay);
				g_dbg("change to sensor light mode.\n");
				break;
			case SENSOR_PROX_MODE:
				gp2ap->sensor_mode = SENSOR_ALL_MODE;
				g_dbg("change to sensor all mode.\n");
				break;
			default:
				break;
		}
		mutex_unlock(&gp2ap->lock);		
	} else {                    /* disable if state will be changed */
		if (!data->enable) {
			g_dbg("light sensor has been disable!\n");
			goto out;
		}

		mutex_lock(&gp2ap->lock);

		sensor_mode = gp2ap->sensor_mode;
		switch (sensor_mode) {
			case SENSOR_LIGHT_MODE:
				cancel_delayed_work_sync(&gp2ap->light_delayed_work);
				gp2ap->sensor_mode = SENSOR_NONE_MODE;
				power_down(gp2ap);
				g_dbg("change to sensor none mode.\n");
				break;
			case SENSOR_ALL_MODE:
				gp2ap->sensor_mode = SENSOR_PROX_MODE;
				g_dbg("change to sensor prox mode.\n");
				break;
			default:
				break;
		}
		mutex_unlock(&gp2ap->lock);
	}

	data->enable = enable;
out:
	mutex_unlock(&data->enable_lock);
}


static ssize_t gp2ap_enable_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int enable;
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);

	TRACE_FUNC();

	mutex_lock(&data->enable_lock);
	enable = data->enable;
	mutex_unlock(&data->enable_lock);

	return sprintf(buf, "%d\n", enable);
}

static ssize_t gp2ap_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	int enable = simple_strtoul(buf, NULL, 10);

	TRACE_FUNC();

	if ((enable != 0) && (enable != 1)) {
		return count;
	}

	if (data->sensor == LIGHT_SENSOR) light_set_enable(data, enable);
	else prox_set_enable(data, enable);

	return count;
}


static ssize_t gp2ap_delay_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);

	TRACE_FUNC();

	return sprintf(buf, "%d\n",  data->delay);
}


static ssize_t gp2ap_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	TRACE_FUNC();

	return count;
}


static ssize_t gp2ap_wake_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	static int cnt = 1;

	TRACE_FUNC();

	input_report_abs(input_data, ABS_WAKE, cnt++);
	return count;
}

static ssize_t gp2ap_data_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
#if 1
	int value;
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);

	TRACE_FUNC();

	mutex_lock(&data->data_lock);
	value = data->data;
	mutex_unlock(&data->data_lock);

	return sprintf(buf, "%d\n", value);
#endif
}

static ssize_t gp2ap_status_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", 3);
}


static ssize_t gp2ap_threshold_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	struct gp2ap_data *gp2ap = container_of(data, struct gp2ap_data, prox);
	int threshold;

	TRACE_FUNC();

	mutex_lock(&gp2ap->lock);
	threshold = get_prox_threshold();
	gp2ap->prox_threshold = threshold;
	mutex_unlock(&gp2ap->lock);

	return sprintf(buf, "%d\n", threshold);
}


static ssize_t gp2ap_CalibValue_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	struct gp2ap_data *gp2ap = container_of(data, struct gp2ap_data, prox);

	TRACE_FUNC();

	mutex_lock(&gp2ap->lock);
	if (!gp2ap->prox_calib_value)
		gp2ap->prox_calib_value = read_prox_calibvalue();
	mutex_unlock(&gp2ap->lock);

	return sprintf(buf, "%d\n", gp2ap->prox_calib_value);
}

static ssize_t gp2ap_CalibValue_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	struct gp2ap_data *gp2ap = container_of(data, struct gp2ap_data, prox);
	int value = simple_strtoul(buf, NULL, 10);

	gp2ap->prox_calib_value = value;
	gp2ap->prox_threshold = value + 20;

	printk("gp2ap proximity threshold is set to %d.\n", gp2ap->prox_threshold);

	return count;
}


static ssize_t gp2ap_LedOffData_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	struct gp2ap_data *gp2ap = container_of(data, struct gp2ap_data, prox);
	int reg_data = 0;

	TRACE_FUNC();

	mutex_lock(&gp2ap->lock);

	gp2ap_set_led_off_mode(gp2ap->client);

	msleep(50);    //delay 50ms to mesure, actually time is 6.3 * 2 ms

	reg_data = get_adc_data(gp2ap);
	if (reg_data < 0) {
		pr_err("%s: get adc data error!(ret = %d)\n", __func__, reg_data);
		reg_data = -1;
	}

	power_down(gp2ap);

	mutex_unlock(&gp2ap->lock);

	return sprintf(buf, "%d\n", reg_data);
}


static ssize_t gp2ap_LedOnData_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	struct gp2ap_data *gp2ap = container_of(data, struct gp2ap_data, prox);
	int reg_data = 0;

	TRACE_FUNC();

	mutex_lock(&gp2ap->lock);

	gp2ap_set_led_on_mode(gp2ap->client);

	msleep(50);    //delay 50ms to mesure, actually time is 6.3 * 2 ms

	reg_data = get_adc_data(gp2ap);
	if (reg_data < 0) {
		pr_err("%s: get adc data error!(ret = %d)\n", __func__, reg_data);
		reg_data = -1;
	}

	power_down(gp2ap);

	mutex_unlock(&gp2ap->lock);

	return sprintf(buf, "%d\n", reg_data);
}


static ssize_t gp2ap_ReflectData_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	struct gp2ap_data *gp2ap = container_of(data, struct gp2ap_data, prox);
	int reg_data = 0;

	TRACE_FUNC();

	mutex_lock(&gp2ap->lock);

	gp2ap_set_shutdown_mode(gp2ap->client);

	msleep(50);    //delay 50ms to mesure, actually time is 6.3 * 2 ms

	reg_data = get_adc_data(gp2ap);
	if (reg_data < 0) {
		pr_err("%s: get adc data error!(ret = %d)\n", __func__, reg_data);
		reg_data = -1;
	}

	power_down(gp2ap);

	mutex_unlock(&gp2ap->lock);

	return sprintf(buf, "%d\n", reg_data);
}



#ifdef GP2AP_DEBUG
static ssize_t gp2ap_registers_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);

	struct gp2ap_data *gp2ap;

	if (data->sensor == LIGHT_SENSOR)
		gp2ap = container_of(data, struct gp2ap_data, light);
	else gp2ap = container_of(data, struct gp2ap_data, prox);
	print_all_registers(gp2ap->client);

	return sprintf(buf, "%d\n", 0);
}
#endif


//for light and proximity
static DEVICE_ATTR(enable, S_IRUGO|S_IWUGO,
		gp2ap_enable_show, gp2ap_enable_store);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUGO,
		gp2ap_delay_show, gp2ap_delay_store);
static DEVICE_ATTR(wake, S_IWUGO,
		NULL, gp2ap_wake_store);
static DEVICE_ATTR(data, S_IRUGO, gp2ap_data_show, NULL);
static DEVICE_ATTR(status, S_IRUGO, gp2ap_status_show, NULL);
static DEVICE_ATTR(CalibValue, S_IRUGO | S_IWUGO, gp2ap_CalibValue_show, gp2ap_CalibValue_store);
static DEVICE_ATTR(LedOffData, S_IRUGO, gp2ap_LedOffData_show, NULL);
static DEVICE_ATTR(LedOnData, S_IRUGO, gp2ap_LedOnData_show, NULL);
static DEVICE_ATTR(ReflectData, S_IRUGO, gp2ap_ReflectData_show, NULL);
static DEVICE_ATTR(threshold, S_IRUGO, gp2ap_threshold_show, NULL);

#ifdef GP2AP_DEBUG
static DEVICE_ATTR(registers, S_IRUGO, gp2ap_registers_show, NULL);
#endif

static struct attribute *gp2ap_light_attributes[] = {
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_wake.attr,
	&dev_attr_data.attr,
	&dev_attr_status.attr,
#ifdef GP2AP_DEBUG
	&dev_attr_registers.attr,
#endif
	NULL
};

static struct attribute *gp2ap_prox_attributes[] = {
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_wake.attr,
	&dev_attr_data.attr,
	&dev_attr_status.attr,
	&dev_attr_CalibValue.attr,
	&dev_attr_LedOffData.attr, 
	&dev_attr_LedOnData.attr, 
	&dev_attr_ReflectData.attr, 
	&dev_attr_threshold.attr, 
#ifdef GP2AP_DEBUG
	&dev_attr_registers.attr,
#endif
	NULL
};


static struct attribute_group gp2ap_light_attribute_group = {
	.attrs = gp2ap_light_attributes
};

static struct attribute_group gp2ap_prox_attribute_group = {
	.attrs = gp2ap_prox_attributes
};


/*
 * report light data to application layer, if the new data percent is the same as the old, don't report 
 */
static void report_light_data(struct sensor_data *light, int data)
{
	mutex_lock(&light->data_lock);

	input_report_abs(light->input_data, ABS_X, data);
	input_sync(light->input_data);         
	light->data = data;

	mutex_unlock(&light->data_lock);	
}


static void light_work_func(struct work_struct *work)
{
	struct gp2ap_data *gp2ap = container_of((struct delayed_work *)work, struct gp2ap_data, light_delayed_work);
	struct sensor_data *light = &gp2ap->light;
	int data;

	//	TRACE_FUNC();

	data = gp2ap_i2c_read_byte(gp2ap->client, DATA_MSB_REG) << 8;
	data = data  + gp2ap_i2c_read_byte(gp2ap->client, DATA_LSB_REG);

	//	g_dbg("light data count is %d, gp2ap delay is %d ms.\n", data, light->delay);

	report_light_data(&gp2ap->light, data);

	gp2ap_i2c_write_byte(gp2ap->client,COMMAND1_REG,  0x80);   //set auto shut-down again 

	queue_delayed_work(gp2ap_wq, &gp2ap->light_delayed_work, delay_to_jiffies(light->delay));
}

static void prox_work_func(struct work_struct *work)
{
	struct gp2ap_data *gp2ap = container_of(work, struct gp2ap_data, prox_work);
	int reg_data;
	struct sensor_data *prox = &gp2ap->prox;
	int no_prox_count = 0;
	int prox_count = 0;
	int prox_flag = 0;
	struct cpufreq_policy cpu_policy;
	static int cpu_cur_level = 0;

	cpufreq_get_policy(&cpu_policy, 0);
	cpu_cur_level = cpufreq_driver_getlevel(&cpu_policy, 0);

	TRACE_FUNC();

	while (gp2ap->measure_flag) {
		gp2ap_set_shutdown_mode(gp2ap->client);
		msleep(15);

		reg_data = get_adc_data(gp2ap);

		g_dbg("prox adc data is %d.\n", reg_data);

		if (reg_data >= gp2ap->prox_threshold) {
			no_prox_count = 0;    //when reg_data is larger than threshold, reset no_prox_count

			prox_count++;
			if (prox_count >= PROX_THRESHOLD_COUNT) {
				prox_count = 0;
				if (!prox_flag) {    //when in no proximity state
					gp2ap_set_led_off_mode(gp2ap->client);
					msleep(15);
					reg_data = get_adc_data(gp2ap);
					g_dbg("led off data is %d.\n", reg_data);

					if (reg_data < LED_OFF_THRESHOLD) {
						prox_flag = 1;
						pr_debug("Proximity sensor : ************ in proximity state ************\n");
						input_report_abs(prox->input_data, ABS_X, 0);
						input_sync(prox->input_data);

						mutex_lock(&prox->data_lock);
						prox->data = 0;
						mutex_unlock(&prox->data_lock);
						if (cpu_cur_level != CPU_FREQ_LEVEL_LOW)
							cpufreq_driver_setlevel(&cpu_policy, CPU_FREQ_LEVEL_LOW);
					}
				}
			}
		} else if (reg_data <= (gp2ap->prox_threshold - 6)) {
			prox_count = 0;    //when reg_data is less than threshold, reset prox_count

			no_prox_count++;
			if (no_prox_count >= PROX_THRESHOLD_COUNT) {
				no_prox_count = 0;
				if (gp2ap->first_measure_flag || prox_flag) {        //when in proximity state
					gp2ap->first_measure_flag = 0;
					prox_flag = 0;
					pr_debug("Proximity sensor : ************ in no proximity state ************\n");
					input_report_abs(prox->input_data, ABS_X, 10);
					input_sync(prox->input_data);

					mutex_lock(&prox->data_lock);
					prox->data = 10;
					mutex_unlock(&prox->data_lock);
				}
			}
		} else {
			prox_count = 0;
			no_prox_count = 0;
		}
	}

	cpufreq_driver_setlevel(&cpu_policy, cpu_cur_level);
	pr_debug("end prox_work_func() while loop!\n");
}

static void gp2ap_input_free(struct input_dev *input)
{
	input_unregister_device(input);
	input_free_device(input);	
}


static int gp2ap_sensor_init(int sensor, struct sensor_data *data,  int delay, struct i2c_client *client)
{
	int ret;
	int max_value;
	struct input_dev *input_data;

	TRACE_FUNC();

	if ((sensor != LIGHT_SENSOR) && (sensor != PROX_SENSOR))
	{
		g_err("unknow sensor!\n");
		return -1;
	}

	data->enable = SENSOR_DISABLE;
	mutex_init(&data->enable_lock);
	mutex_init(&data->data_lock);
	data->delay = delay;
	data->sensor = sensor;
	data->level = -1;

	input_data = input_allocate_device();
	if (input_data == NULL)  return -ENOMEM;

	set_bit(EV_ABS, input_data->evbit);
	input_set_capability(input_data, EV_ABS, ABS_X);
	input_set_capability(input_data, EV_ABS, ABS_STATUS); /* status */
	input_set_capability(input_data, EV_ABS, ABS_WAKE); /* wake */
	input_set_capability(input_data, EV_ABS, ABS_CONTROL_REPORT); /* enabled/delay */

	if (sensor == LIGHT_SENSOR) max_value = LIGHT_MAX_VALUE;
	else max_value = PROX_MAX_VALUE;
	input_set_abs_params(input_data, ABS_X, 0, max_value, 0, 0);  

	if (sensor == LIGHT_SENSOR) input_data->name = LIGHT_SENSOR_NAME;
	else  input_data->name = PROXIMITY_SENSOR_NAME;

	input_data->dev.parent = &client->dev;
	ret = input_register_device(input_data);
	if (ret < 0) {
		g_err("%s register input device error!\n", input_data->name);
		goto err_1;
	}
	data->input_data = input_data;
	input_set_drvdata(data->input_data, data);

	if (sensor == LIGHT_SENSOR) {
		ret = sysfs_create_group(&data->input_data->dev.kobj, &gp2ap_light_attribute_group);
		if (ret < 0) {
			g_err("%s create sysfs error!\n", input_data->name);
			goto err_2;
		}
	}
	else {
		ret = sysfs_create_group(&data->input_data->dev.kobj, &gp2ap_prox_attribute_group);
		if (ret < 0) {
			g_err("%s create sysfs error!\n", input_data->name);
			goto err_2;
		}
	}

	return 0;
err_2:
	input_unregister_device(input_data);
err_1:
	input_free_device(input_data);
	return ret;
}


static void gp2ap_sensor_free(struct sensor_data *data)
{
	gp2ap_input_free(data->input_data);
	if (data->sensor == LIGHT_SENSOR) 
		sysfs_remove_group(&data->input_data->dev.kobj, &gp2ap_light_attribute_group);	
	else
		sysfs_remove_group(&data->input_data->dev.kobj, &gp2ap_prox_attribute_group);	
}


static int gp2ap_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	struct gp2ap_data *gp2ap;

	TRACE_FUNC();

	if (!i2c_check_functionality(client->adapter,I2C_FUNC_I2C)) {
		printk("i2c bus does not support %s sensor .\n", GP2AP_DRV_NAME);
		return -ENODEV;
	}

	ret = gp2ap_detect(client);
	if (ret < 0)
	{
		printk("gp2ap012a00f sensor doesn't exist!\n");
		return ret;
	}	

	gp2ap = kzalloc(sizeof(struct gp2ap_data), GFP_KERNEL);
	if (gp2ap == NULL) return -ENOMEM;

	gp2ap->client = client;
	i2c_set_clientdata(client, gp2ap);

	ret = gp2ap_sensor_init(LIGHT_SENSOR, &gp2ap->light, LIGHT_DEFAULT_DELAY, client);
	if (ret < 0)
	{
		printk("light sensor init failed!\n");
		goto err_1;
	}

	ret = gp2ap_sensor_init(PROX_SENSOR, &gp2ap->prox, PROX_DEFAULT_DELAY, client);
	if (ret < 0)
	{
		printk("prox sensor init failed!\n");
		goto err_2;
	}

	gp2ap->sensor_mode = SENSOR_NONE_MODE;
	gp2ap->suspend_mode = SENSOR_NONE_MODE;
	mutex_init(&gp2ap->lock);

	gp2ap_wq = create_singlethread_workqueue("gp2ap_wq");
	if(gp2ap_wq == NULL)
	{
		printk("create_singlethread_workqueue faild!\n");
		ret = -ENOMEM;
		goto err_3;
	}

	INIT_WORK(&gp2ap->prox_work, prox_work_func);
	INIT_DELAYED_WORK(&gp2ap->light_delayed_work, light_work_func);

	gp2ap->prox_calib_value = 0;
	gp2ap->prox_threshold = 0;
	gp2ap->measure_flag = 0;

	printk("gp2ap sensor probe succeed!\n");

	return 0;

err_3:
	gp2ap_sensor_free(&gp2ap->prox);
err_2:
	gp2ap_sensor_free(&gp2ap->light);
err_1:
	kfree(gp2ap);

	printk("gp2ap sensor probe fail!\n");

	return ret;
}


static int __devexit gp2ap_remove(struct i2c_client *client)
{
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);
	int sensor_mode;	

	TRACE_FUNC();

#ifdef KTHREAD_DEBUG
	kthread_stop(gp2ap->kthread);
#endif

	mutex_lock(&gp2ap->lock);
	sensor_mode = gp2ap->sensor_mode;
	mutex_unlock(&gp2ap->lock);

	if (sensor_mode != SENSOR_NONE_MODE) {
		if (sensor_mode == SENSOR_LIGHT_MODE) cancel_delayed_work_sync(&gp2ap->light_delayed_work);
		else cancel_work_sync(&gp2ap->prox_work);
	}
	destroy_workqueue(gp2ap_wq);
	free_irq(gp2ap->irq, gp2ap);
	gp2ap_sensor_free(&gp2ap->light);
	gp2ap_sensor_free(&gp2ap->prox);
	kfree(gp2ap);

	return 0;
}


#ifdef CONFIG_PM
static int gp2ap_suspend(struct i2c_client *client, pm_message_t mesg)
{	
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);

	TRACE_FUNC();

	mutex_lock(&gp2ap->lock);
	gp2ap->suspend_mode= gp2ap->sensor_mode;	
	mutex_unlock(&gp2ap->lock);		

	g_dbg("in gp2ap_suspend: gp2ap sensor_mode is %d. suspend_mode is %d.\n", gp2ap->sensor_mode, gp2ap->suspend_mode);

	switch (gp2ap->suspend_mode) {
		case SENSOR_NONE_MODE:
			break;
		case SENSOR_LIGHT_MODE:
			light_set_enable(&gp2ap->light, 0);
			break;
		case SENSOR_PROX_MODE:
			prox_set_enable(&gp2ap->prox, 0);
			break;
		case SENSOR_ALL_MODE:
			light_set_enable(&gp2ap->light, 0);
			prox_set_enable(&gp2ap->prox, 0);
			break;
		default:
			printk("unknown gp2ap mode in gp2ap_suspend!\n");
			break;
	}

	power_down(gp2ap);

	return 0;
}

static int gp2ap_resume(struct i2c_client *client)
{	
	struct gp2ap_data *gp2ap = i2c_get_clientdata(client);

	TRACE_FUNC();

	g_dbg("in gp2ap_resume: gp2ap sensor_mode is %d. suspend_mode is %d.\n", gp2ap->sensor_mode, gp2ap->suspend_mode);

	switch (gp2ap->suspend_mode) {
		case SENSOR_NONE_MODE:
			break;
		case SENSOR_LIGHT_MODE:
			light_set_enable(&gp2ap->light, 1);
			break;
		case SENSOR_PROX_MODE:
			prox_set_enable(&gp2ap->prox, 1);
			break;
		case SENSOR_ALL_MODE:
			light_set_enable(&gp2ap->light, 1);
			prox_set_enable(&gp2ap->prox, 1);
			break;
		default:
			printk("unknown gp2ap mode in gp2ap_resume!\n");
			break;
	}       

	return 0;
}

#else
#define gp2ap_suspend		NULL
#define gp2ap_resume		NULL
#endif


static const struct i2c_device_id gp2ap_id[] = {
	{ "sharp-gp2ap012a00f", 0 },
	{ }
};


static struct i2c_driver gp2ap_driver = {
	.driver = {
		.name	= GP2AP_DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe	= gp2ap_probe,
	.remove	= gp2ap_remove,
#ifdef CONFIG_PM
	.suspend = gp2ap_suspend,
	.resume	= gp2ap_resume,
#endif

	.id_table = gp2ap_id,
};


static int __init gp2ap_init(void)
{
	return i2c_add_driver(&gp2ap_driver);
}


static void __exit gp2ap_exit(void)
{
	i2c_del_driver(&gp2ap_driver);
}


module_init(gp2ap_init);
module_exit(gp2ap_exit);

MODULE_DESCRIPTION("sharp ambient light and proximity sensor driver");
MODULE_AUTHOR("Meizu");
MODULE_LICENSE("GPL");
