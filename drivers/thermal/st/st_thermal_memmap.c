#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/of.h>
#include <linux/module.h>
#ifdef MY_DEF_HERE
#include <linux/of_address.h>
#endif  

#include "st_thermal.h"

#define STIH416_THSENS(num)			(num * 4)
#define STIH416_MPE_CONF			STIH416_THSENS(0)
#define STIH416_MPE_STATUS			STIH416_THSENS(1)
#define STIH416_MPE_INT_THRESH			STIH416_THSENS(2)
#define STIH416_MPE_INT_EN			STIH416_THSENS(3)
#define STIH416_MPE_INT_EVT			STIH416_THSENS(4)

#define THERMAL_PDN				BIT(4)
#define THERMAL_SRSTN				BIT(10)

static const struct reg_field st_thermal_memmap_regfields[MAX_REGFIELDS] = {
	 
	[INT_THRESH_HI] = REG_FIELD(STIH416_MPE_INT_THRESH, 0, 7),
	[DCORRECT] = REG_FIELD(STIH416_MPE_CONF, 5, 9),
	[OVERFLOW] = REG_FIELD(STIH416_MPE_STATUS, 9, 9),
	[DATA] = REG_FIELD(STIH416_MPE_STATUS, 11, 18),
#ifdef MY_DEF_HERE
	[DATARDY] = REG_FIELD(STIH416_MPE_STATUS, 10, 10),
#endif  
	[INT_ENABLE] = REG_FIELD(STIH416_MPE_INT_EN, 0, 0),
	[INT_THRESH_LOW] = REG_FIELD(STIH416_MPE_INT_THRESH, 8, 15),
#ifdef MY_DEF_HERE
	[DC_CALIB] = REG_FIELD(0, 22, 26),
#endif  
};

static irqreturn_t st_thermal_thresh_int(int irq, void *data)
{
	struct st_thermal_sensor *sensor = data;
	struct device *dev = sensor_to_dev(sensor);

	if (sensor->cdev) {
		 
		kobject_uevent(&sensor->th_dev->device.kobj, KOBJ_CHANGE);
		dev_info(dev, "passive cooling threshold:(%d) crossed\n",
			 sensor->passive_temp);

		mutex_lock(&sensor->th_dev->lock);
		sensor->th_dev->polling_delay = 1000;
		mutex_unlock(&sensor->th_dev->lock);
	}
	thermal_zone_device_update(sensor->th_dev);

	return IRQ_HANDLED;
}

static int st_memmap_power_ctrl(struct st_thermal_sensor *sensor,
				enum st_thermal_power_state power_state)
{
	const unsigned int mask = (THERMAL_PDN | THERMAL_SRSTN);
	const unsigned int val = power_state ? mask : 0;

#ifdef MY_DEF_HERE
	return regmap_update_bits(sensor->regmap[TH_REGS], STIH416_MPE_CONF,
#else  
	return regmap_update_bits(sensor->regmap, STIH416_MPE_CONF,
#endif  
				  mask, val);
}

static int st_memmap_alloc_regfields(struct st_thermal_sensor *sensor)
{
	struct device *dev = sensor_to_dev(sensor);
#ifdef MY_DEF_HERE
	struct regmap *regmap = sensor->regmap[TH_REGS];
#else  
	struct regmap *regmap = sensor->regmap;
#endif  
	const struct reg_field *reg_fields = sensor->data->reg_fields;
	int ret;

	ret = st_thermal_common_alloc_regfields(sensor);
	if (ret)
		return ret;

	sensor->int_thresh_hi = devm_regmap_field_alloc(dev, regmap,
					reg_fields[INT_THRESH_HI]);
	sensor->int_thresh_low = devm_regmap_field_alloc(dev, regmap,
					reg_fields[INT_THRESH_LOW]);
	sensor->int_enable = devm_regmap_field_alloc(dev, regmap,
					reg_fields[INT_ENABLE]);

	if (IS_ERR(sensor->int_thresh_hi) ||
	    IS_ERR(sensor->int_thresh_low) ||
	    IS_ERR(sensor->int_enable)) {
#ifdef MY_DEF_HERE
		dev_err(dev, "failed to alloc reg field(s)\n");
#else  
		dev_err(dev, "%s,failed to alloc reg field(s)\n", __func__);
#endif  
		return -EINVAL;
	}

	return 0;
}

static int st_memmap_register_irq(struct st_thermal_sensor *sensor)
{
	struct device *dev = sensor_to_dev(sensor);
	struct platform_device *pdev = to_platform_device(dev);
	int ret;

	sensor->irq = platform_get_irq(pdev, 0);
	if (sensor->irq < 0)
		return sensor->irq;

	ret = devm_request_threaded_irq(dev, sensor->irq,
					NULL, st_thermal_thresh_int,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					dev->driver->name, sensor);
	if (ret) {
		dev_err(dev, "IRQ %d register failed\n", sensor->irq);
		return ret;
	}

	return sensor->ops->enable_irq(sensor);
}

static int st_memmap_enable_irq(struct st_thermal_sensor *sensor)
{
	int ret;
	unsigned int threshold_temp = sensor->passive_temp;

	if (!sensor->cdev)
		threshold_temp = sensor->data->crit_temp;

	ret = regmap_field_write(sensor->int_thresh_hi,
				 threshold_temp
				 - sensor->data->temp_adjust_val);
	if (ret)
		return ret;

	ret = regmap_field_write(sensor->int_thresh_low,
				 threshold_temp
				 - sensor->data->temp_adjust_val);
	if (ret)
		return ret;

	return regmap_field_write(sensor->int_enable, 1);
}

static void st_memmap_clear_irq(struct thermal_zone_device *th,
				unsigned int temp)
{
	struct st_thermal_sensor *sensor = thzone_to_sensor(th);

	if (th->polling_delay &&
	    (temp < mcelsius(sensor->passive_temp))) {
		sensor->th_dev->polling_delay = 0;
		 
#ifdef MY_DEF_HERE
		regmap_write(sensor->regmap[TH_REGS], STIH416_MPE_INT_EVT, 1);
#else  
		regmap_write(sensor->regmap, STIH416_MPE_INT_EVT, 1);
#endif  
	}
}

static struct regmap_config st_416mpe_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static int st_memmap_do_memmap_regmap(struct st_thermal_sensor *sensor)
{
	struct device *dev = sensor_to_dev(sensor);
#ifdef MY_DEF_HERE
	struct device_node *np = dev->of_node;
	struct resource res;
	void __iomem *iobase;

	if (of_address_to_resource(np, 0, &res))
		return -EINVAL;

	iobase = devm_ioremap_resource(dev, &res);
	if (IS_ERR(iobase)) {
		dev_err(dev, "unable to map iomem\n");
		return PTR_ERR(iobase);
	}
	sensor->regmap[TH_REGS] =
		devm_regmap_init_mmio(dev, iobase,
				      &st_416mpe_regmap_config);
	if (IS_ERR(sensor->regmap[TH_REGS])) {
		dev_err(dev, "regmap init failed\n");
		return PTR_ERR(sensor->regmap[TH_REGS]);
	}

	return 0;
#else  
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "no memory resources defined\n");
		ret = -ENODEV;
		return ret;
	}

	sensor->mmio_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(sensor->mmio_base)) {
		ret = PTR_ERR(sensor->mmio_base);
		return ret;
	}

	sensor->regmap = devm_regmap_init_mmio(dev, sensor->mmio_base,
				&st_416mpe_regmap_config);
	if (IS_ERR(sensor->regmap)) {
		dev_err(dev, "regmap init failed\n");
		ret = PTR_ERR(sensor->regmap);
	}

	return ret;
#endif  
}

static struct st_thermal_sensor_ops st_memmap_sensor_ops = {
	.power_ctrl = st_memmap_power_ctrl,
	.alloc_regfields = st_memmap_alloc_regfields,
	.do_memmap_regmap = st_memmap_do_memmap_regmap,
	.register_irq = st_memmap_register_irq,
	.enable_irq = st_memmap_enable_irq,
	.clear_irq = st_memmap_clear_irq,
};

struct st_thermal_compat_data st_416mpe_data = {
	.reg_fields = st_thermal_memmap_regfields,
	.ops = &st_memmap_sensor_ops,
	.calibration_val = 14,
	.temp_adjust_val = -95,
	.passive_temp = 80,
	.crit_temp = 125,
};

struct st_thermal_compat_data st_407_data = {
	.reg_fields = st_thermal_memmap_regfields,
	.ops = &st_memmap_sensor_ops,
	.calibration_val = 16,
	.temp_adjust_val = -95,
	.passive_temp = 80,
	.crit_temp = 125,
};
