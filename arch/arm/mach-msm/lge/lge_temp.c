/*
 *
 *
 *
 */

#include <linux/platform_device.h>
#include <asm/io.h>

extern int lge_erase_block(int ebnum);
extern int lge_write_block(int ebnum, unsigned char *buf, size_t size);
extern int lge_read_block(int ebnum, unsigned char *buf);
extern int init_mtd_access(int partition, int block);

static int tolk_store(struct device *dev, struct device_attribute *attr, const char *buf, ssize_t count)
{
	unsigned int magic_number = 0;
	unsigned *vir_addr;
	sscanf(buf, "%d\n", &magic_number);

	vir_addr = ioremap(0x2ffff000, PAGE_SIZE);
	(*(unsigned int *)vir_addr) = magic_number;

	return count;
}
DEVICE_ATTR(tolk, 0777, NULL, tolk_store);

static unsigned int lcdbe = 1;
static int lcdbe_mode(char *test)
{
	if(!strncmp("off", test, 3))
		lcdbe = 0;
	else
		lcdbe = 1;
}
__setup("lge.lcd=", lcdbe_mode);

static int lcdbe_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", lcdbe);
}
DEVICE_ATTR(lcdis, 0777, lcdbe_show, NULL);

static unsigned int test_result = 0;
static int result_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned char buf11[2048];
	unsigned int a, b, c, d;

	init_mtd_access(4, 7);
	lge_read_block(7, buf11);

	a = buf11[0] & 0x000000ff;
	b = (buf11[1] << 8) & 0x0000ff00;
	c = (buf11[2] << 16) & 0x00ff0000;
	d = (buf11[3] << 24) & 0xff000000;

	test_result = a + b + c + d;

	return sprintf(buf, "%d\n", test_result);
}

static int result_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned char buf11[2048];

	memset((void *)buf11, 0, sizeof(buf11));

	sscanf(buf, "%d\n", &test_result);

	buf11[0] = 0x000000ff & test_result;
	buf11[1] = 0x000000ff & (test_result>>8);
	buf11[2] = 0x000000ff & (test_result>>16);
	buf11[3] = 0x000000ff & (test_result>>24);

	init_mtd_access(4, 7);
	lge_erase_block(7);
	lge_write_block(7, buf11, 2048);

	return count;
}
DEVICE_ATTR(result, 0777, result_show, result_store);

static unsigned int g_flight = 0;
static int flight_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	return sprintf(buf, "%d\n", g_flight);
}

static int flight_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int test_result=0;
	
	sscanf(buf, "%d\n", &test_result);
	g_flight = test_result;	
	
	return count;
}
DEVICE_ATTR(flight, 0777, flight_show, flight_store);

static int __init lge_tempdevice_probe(struct platform_device *pdev)
{
	int err;

	err = device_create_file(&pdev->dev, &dev_attr_result);
	if (err < 0) 
		printk("%s : Cannot create the sysfs\n", __func__);
	
	err = device_create_file(&pdev->dev, &dev_attr_lcdis);
	if (err < 0) 
		printk("%s : Cannot create the sysfs\n", __func__);

	err = device_create_file(&pdev->dev, &dev_attr_tolk);
	if (err < 0) 
		printk("%s : Cannot create the sysfs\n", __func__);

	err = device_create_file(&pdev->dev, &dev_attr_flight);
	if (err < 0) 
		printk("%s : Cannot create the sysfs\n", __func__);
	
}

static struct platform_device lgetemp_device = {
	.name = "autoall",
	.id		= -1,
};

static struct platform_driver this_driver = {
	.probe = lge_tempdevice_probe,
	.driver = {
		.name = "autoall",
	},
};

int __init lge_tempdevice_init(void)
{
	printk("%s\n", __func__);
	platform_device_register(&lgetemp_device);

	return platform_driver_register(&this_driver);
}

module_init(lge_tempdevice_init);
MODULE_DESCRIPTION("Just temporal code for PV");
MODULE_AUTHOR("MoonCheol Kang <neo.kang>");
MODULE_LICENSE("GPL");
