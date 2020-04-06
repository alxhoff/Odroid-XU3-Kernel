//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  I2C INA231(Sensor) driver
//  2013.07.17
// 
//
//[*]--------------------------------------------------------------------------------------------------[*]
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>

#include <linux/platform_data/ina231.h>

//[*]--------------------------------------------------------------------------------------------------[*]
#include "ina231-i2c.h"
#include "ina231-misc.h"

//[*]--------------------------------------------------------------------------------------------------[*]
//
//   sysfs function prototype define
//
//[*]--------------------------------------------------------------------------------------------------[*]
static int get_delay(struct ina231_sensor *sensor);
void set_sensor_period(struct ina231_sensor *sensor, unsigned int period);

static	ssize_t show_name	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(sensor_name, S_IRWXUGO, show_name, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_power			(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(sensor_W, S_IRWXUGO, show_power, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_current		(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(sensor_A, S_IRWXUGO, show_current, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_voltage		(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(sensor_V, S_IRWXUGO, show_voltage, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_max_power		(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(sensor_maxW, S_IRWXUGO, show_max_power, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_max_current	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(sensor_maxA, S_IRWXUGO, show_max_current, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_max_voltage	(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(sensor_maxV, S_IRWXUGO, show_max_voltage, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_enable         (struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_enable          (struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(enable, S_IRWXUGO, show_enable, set_enable);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_period 		(struct device *dev, struct device_attribute *attr, char *buf);
static  ssize_t set_period      (struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(update_period, S_IRWXUGO, show_period, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_delay 		(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(delay, S_IRWXUGO, show_delay, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_available_averages 		(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(available_averages, S_IRWXUGO, show_available_averages, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_available_ct 		(struct device *dev, struct device_attribute *attr, char *buf);
static	DEVICE_ATTR(available_ct, S_IRWXUGO, show_available_ct, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_averages 		(struct device *dev, struct device_attribute *attr, char *buf);
static  ssize_t set_averages      (struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(averages, S_IRWXUGO, show_averages, set_averages);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_vbus_ct 		(struct device *dev, struct device_attribute *attr, char *buf);
static  ssize_t set_vbus_ct      (struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(vbus_ct, S_IRWXUGO, show_vbus_ct, set_vbus_ct);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_vsh_ct 		(struct device *dev, struct device_attribute *attr, char *buf);
static  ssize_t set_vsh_ct      (struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(vsh_ct, S_IRWXUGO, show_vsh_ct, set_vsh_ct);

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static struct attribute *ina231_sysfs_entries[] = {
	&dev_attr_sensor_name.attr,
	&dev_attr_sensor_W.attr,
	&dev_attr_sensor_A.attr,
	&dev_attr_sensor_V.attr,
	&dev_attr_sensor_maxW.attr,
	&dev_attr_sensor_maxA.attr,
	&dev_attr_sensor_maxV.attr,
	&dev_attr_enable.attr,
	&dev_attr_update_period.attr,
	&dev_attr_delay.attr,
	&dev_attr_available_averages.attr,
	&dev_attr_available_ct.attr,
	&dev_attr_averages.attr,
	&dev_attr_vbus_ct.attr,
	&dev_attr_vsh_ct.attr,
	NULL
};

const unsigned short INA231_AVERAGES[] = {
    eAVG_CON_1,
    eAVG_CON_4,
    eAVG_CON_16,
    eAVG_CON_64,
    eAVG_CON_128,
    eAVG_CON_256,
    eAVG_CON_512,
    eAVG_CON_1024,
};

const unsigned short INA231_VBUS_CT[] = {
    eVBUS_CON_140uS,
    eVBUS_CON_204uS,
    eVBUS_CON_332uS,
    eVBUS_CON_588uS,
    eVBUS_CON_1100uS,
    eVBUS_CON_2116uS,
    eVBUS_CON_4156uS,
    eVBUS_CON_8244uS,
};

const unsigned short INA231_VSH_CT[] = {
    eVSH_CON_140uS,
    eVSH_CON_204uS,
    eVSH_CON_332uS,
    eVSH_CON_588uS,
    eVSH_CON_1100uS,
    eVSH_CON_2116uS,
    eVSH_CON_4156uS,
    eVSH_CON_8244uS,
};


static struct attribute_group ina231_attr_group = {
	.name   = NULL,
	.attrs  = ina231_sysfs_entries,
};

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_name			(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina231_sensor 	*sensor = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", sensor->pd->name);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_power			(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina231_sensor 	*sensor = dev_get_drvdata(dev);
	unsigned int            value;
	
	mutex_lock(&sensor->mutex); value = sensor->cur_uW; mutex_unlock(&sensor->mutex);

	return sprintf(buf, "%d.%06d\n", (value/1000000), (value%1000000));
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_max_power		(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina231_sensor 	*sensor = dev_get_drvdata(dev);
	unsigned int            value;
	
	mutex_lock(&sensor->mutex); value = sensor->max_uW; mutex_unlock(&sensor->mutex);

	return sprintf(buf, "%d.%06d\n", (value/1000000), (value%1000000));
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_current		(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina231_sensor 	*sensor = dev_get_drvdata(dev);
	unsigned int            value;
	
	mutex_lock(&sensor->mutex); value = sensor->cur_uA; mutex_unlock(&sensor->mutex);

	return sprintf(buf, "%d.%06d\n", (value/1000000), (value%1000000));
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_max_current	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina231_sensor 	*sensor = dev_get_drvdata(dev);
	unsigned int            value;
	
	mutex_lock(&sensor->mutex); value = sensor->max_uA; mutex_unlock(&sensor->mutex);

	return sprintf(buf, "%d.%06d\n", (value/1000000), (value%1000000));
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_voltage		(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina231_sensor 	*sensor = dev_get_drvdata(dev);
	unsigned int            value;
	
	mutex_lock(&sensor->mutex); value = sensor->cur_uV; mutex_unlock(&sensor->mutex);

	return sprintf(buf, "%d.%06d\n", (value/1000000), (value%1000000));
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_max_voltage	(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina231_sensor 	*sensor = dev_get_drvdata(dev);
	unsigned int            value;
	
	mutex_lock(&sensor->mutex); value = sensor->max_uV; mutex_unlock(&sensor->mutex);

	return sprintf(buf, "%d.%06d\n", (value/1000000), (value%1000000));
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_enable         (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina231_sensor 	*sensor = dev_get_drvdata(dev);
	
	return	sprintf(buf, "%d\n", sensor->pd->enable);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_enable          (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct ina231_sensor 	*sensor = dev_get_drvdata(dev);
    
    if(simple_strtol(buf, NULL, 10) != 0)   {
        if(!sensor->pd->enable) {
            sensor->pd->enable = 1;     ina231_i2c_enable(sensor);
        }
    }
    else    {
        if(sensor->pd->enable)  {
            sensor->pd->enable = 0;     
        }   
    }
	return  count;
}
//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_period     (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina231_sensor 	*sensor = dev_get_drvdata(dev);
    unsigned int            value;

    mutex_lock(&sensor->mutex); value = sensor->pd->update_period; mutex_unlock(&sensor->mutex);
	
	return	sprintf(buf, "%d usec\n", value);
}

void set_sensor_period(struct ina231_sensor *sensor, unsigned int period)
{
    unsigned int old_period = sensor->pd->update_period;
   
    sensor->pd->update_period = period;
    sensor->timer_sec  = sensor->pd->update_period / 1000000;
    sensor->timer_nsec = (sensor->pd->update_period % 1000000) * 1000;

    hrtimer_start(&sensor->timer, ktime_set(sensor->timer_sec, sensor->timer_nsec), HRTIMER_MODE_REL);
    
    printk(KERN_DEBUG "[INA231] Changed INA231 update period from %u to %u\n", 
            old_period, sensor->pd->update_period);

}

static  ssize_t set_period      (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    // INA231 period is determined as twice the conversion time * the average mode
    // Twice the conversion time is needed as the INA first conversts a voltage 
    // reading and then a current reading, making a power reading available
    // every two conversion periods.
    struct ina231_sensor    *sensor = dev_get_drvdata(dev);

    mutex_lock(&sensor->mutex); 
    set_sensor_period(sensor, simple_strtol(buf, NULL, 10));
    mutex_unlock(&sensor->mutex);

    return count;
}

static int get_delay(struct ina231_sensor *sensor)
{
        return CONVERSION_DELAY( 
            INA231_VBUS_CT[INA231_CONFIG_VBUS_CT(sensor->pd->config)],
            INA231_VSH_CT[INA231_CONFIG_VSH_CT(sensor->pd->config)],
            INA231_AVERAGES[INA231_CONFIG_AVERAGES(sensor->pd->config)]
            );
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct ina231_sensor *sensor = dev_get_drvdata(dev);
    unsigned int value;

    mutex_lock(&sensor->mutex); 
    value = get_delay(sensor);
    mutex_unlock(&sensor->mutex);

    return sprintf(buf, "%d usec\n", value);
}

static	ssize_t show_available_averages(struct device *dev, struct device_attribute *attr, char *buf)
{
    int chars = 0;
    int i;

    for(i = 0; i < INA231_NUM_AVERAGES; i++)
        chars += sprintf(buf + chars, "%d\n", INA231_AVERAGES[i]);

    return chars;
}

static	ssize_t show_available_ct(struct device *dev, struct device_attribute *attr, char *buf)
{
    int chars = 0;
    int i;

    for(i = 0; i < INA231_NUM_VSH_CT; i++)
        chars += sprintf(buf + chars, "%d\n", INA231_VSH_CT[i]);

    return chars;
}

static struct ina231_sensor *get_config_reg(struct device *dev)
{
    struct ina231_sensor    *sensor = dev_get_drvdata(dev);
    unsigned short          rc;

    mutex_lock(&sensor->mutex); 
    if ((rc = ina231_i2c_read(sensor->client, REG_CONFIG)) < 0 ) return NULL;

    sensor->pd->config = rc;
    mutex_unlock(&sensor->mutex);

    return sensor;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_averages(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned short value;
    struct ina231_sensor *sensor = get_config_reg(dev);

    mutex_lock(&sensor->mutex); 
    value = INA231_CONFIG_AVERAGES(sensor->pd->config);
    mutex_unlock(&sensor->mutex);

    return sprintf(buf, "%d\n", (int) INA231_AVERAGES[value]);
}

static int valid_average(int average)
{
    int i;

    for(i = 0; i < INA231_NUM_AVERAGES; i++)
        if((int) INA231_AVERAGES[i] == average)
            return i;

    return -1;
}

static  ssize_t set_averages(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int value = simple_strtol(buf, NULL, 10);
    struct ina231_sensor *sensor;
    int i, delay;
    unsigned short config, old_config;

    if((i = valid_average(value)) >= 0){
        sensor = get_config_reg(dev);
        
        mutex_lock(&sensor->mutex); 
        old_config = sensor->pd->config;
        config = (sensor->pd->config & ~AVG_BIT(0b111)) | AVG_BIT(i);
        sensor->pd->config = config;
        ina231_i2c_write(sensor->client, REG_CONFIG, sensor->pd->config);

        printk(KERN_DEBUG "[INA231] Updated averages from %h to %h\n",
                INA231_CONFIG_AVERAGES(old_config), INA231_CONFIG_AVERAGES(config));

        delay = get_delay(sensor);
        set_sensor_period(sensor, delay);

        mutex_unlock(&sensor->mutex);
    }
    
    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static ssize_t show_vbus_ct(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned short value;
    struct ina231_sensor *sensor = get_config_reg(dev);

    mutex_lock(&sensor->mutex); 
    value = INA231_CONFIG_VBUS_CT(sensor->pd->config);
    mutex_unlock(&sensor->mutex);

    return sprintf(buf, "%d uS\n", (int) INA231_VBUS_CT[value]);
}

static int valid_vbus_ct(int vbus)
{
    int i;

    for(i = 0; i < INA231_NUM_VBUS_CT; i++)
        if((int) INA231_VBUS_CT[i] == vbus)
            return i;

    return -1;
}

static ssize_t set_vbus_ct(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int value = simple_strtol(buf, NULL, 10);
    struct ina231_sensor *sensor;
    int i, delay;
    unsigned short config, old_config;

    if((i = valid_vbus_ct(value)) >= 0){
        sensor = get_config_reg(dev);

        mutex_lock(&sensor->mutex); 
        old_config = sensor->pd->config;
        config = (sensor->pd->config & ~VBUS_CT(0b111)) | VBUS_CT(i);
        sensor->pd->config = config;
        ina231_i2c_write(sensor->client, REG_CONFIG, sensor->pd->config);

        printk(KERN_DEBUG "[INA231] Updated Vbus CT from %d to %d\n",
                (int) INA231_CONFIG_VBUS_CT(old_config), (int) INA231_CONFIG_VBUS_CT(config));

        delay = get_delay(sensor);
        set_sensor_period(sensor, delay);

        mutex_unlock(&sensor->mutex);
    }
    
    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static ssize_t show_vsh_ct(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned short value;
    struct ina231_sensor *sensor = get_config_reg(dev);

    mutex_lock(&sensor->mutex); 
    value = INA231_CONFIG_VSH_CT(sensor->pd->config);
    mutex_unlock(&sensor->mutex);

    return sprintf(buf, "%d uS\n", (int) INA231_VSH_CT[value]);
}

static int valid_vsh_ct(int vsh)
{
    int i;

    for(i = 0; i < INA231_NUM_VSH_CT; i++)
        if((int) INA231_VSH_CT[i] == vsh)
            return i;

    return -1;
}

static  ssize_t set_vsh_ct(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int value = simple_strtol(buf, NULL, 10);
    struct ina231_sensor *sensor;
    int i, delay;
    unsigned short config, old_config;

    if((i = valid_vsh_ct(value)) >= 0){
        sensor = get_config_reg(dev);

        mutex_lock(&sensor->mutex); 
        old_config = sensor->pd->config;
        config = (sensor->pd->config & ~VSH_CT(0b111)) | VSH_CT(i);
        sensor->pd->config = config;
        ina231_i2c_write(sensor->client, REG_CONFIG, sensor->pd->config);

        printk(KERN_DEBUG "[INA231] Updated Vsh CT from %d to %d\n",
                (int) INA231_CONFIG_VSH_CT(old_config), (int) INA231_CONFIG_VSH_CT(config));

        delay = get_delay(sensor);
        set_sensor_period(sensor, delay);

        mutex_unlock(&sensor->mutex);
    }
    
    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int		ina231_sysfs_create		(struct device *dev)	
{
	return	sysfs_create_group(&dev->kobj, &ina231_attr_group);
}

//[*]--------------------------------------------------------------------------------------------------[*]
void	ina231_sysfs_remove		(struct device *dev)	
{
    sysfs_remove_group(&dev->kobj, &ina231_attr_group);
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
