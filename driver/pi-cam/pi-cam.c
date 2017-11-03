#include <linux/module.h>			// included for all kernel modules
#include <linux/init.h>				// included for __init and __exit macros
#include <linux/platform_device.h>
#include <linux/export.h>
#include <linux/i2c.h>

#define PI_CAM_NO_EXPORT
#include "pi-cam.h"
#include "sensor_config.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Johannes Feldmann");
MODULE_DESCRIPTION("Simple Test Pattern Generator Driver");


#define CAMERA_V1_3_IIC_ADDRESS		0x36
#define CAMERA_V2_1_IIC_ADDRESS		0x10

#define CS_CMMN_CHIP_ID_H			0x300A
#define CS_CMMN_CHIP_ID_L			0x300B

///////////////////////////////////////////////////////////////////////////////////////////////////
// Devicetree structures
/*
i2c_pi_cam: pi_cam@50 {
	compatible = "jf,pi_cam-1.0.0";
	#address-cells = <1>;
	#size-cells = <0>;
	reg = <50>;
};
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver structures

static LIST_HEAD(pi_cam_list);
static DEFINE_MUTEX(pi_cam_lock);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Function definitions

//-------------------------------------------------------------------------------------------------
// Get device / Put device
static struct pi_cam_device* pi_cam_get(struct device_node* np)
{
	struct device_node *pi_cam_node;
	struct pi_cam_device *found = NULL;
	struct pi_cam_device *pi_cam;

	if(!of_find_property(np, "jf,pi_cam", NULL))
	{
		return NULL;
	}	
	
	pi_cam_node = of_parse_phandle(np, "jf,pi_cam", 0);
	if (pi_cam_node == NULL)
		return ERR_PTR(-EINVAL);

	mutex_lock(&pi_cam_lock);
	list_for_each_entry(pi_cam, &pi_cam_list, list) {
		if (pi_cam->pDev->of_node == pi_cam_node) {
			found = pi_cam;
			break;
		}
	}
	mutex_unlock(&pi_cam_lock);

	of_node_put(pi_cam_node);

	if (!found)
		return ERR_PTR(-EPROBE_DEFER);

	return found;
}
EXPORT_SYMBOL_GPL(pi_cam_get);

static void pi_cam_put(struct pi_cam_device *pDevice)
{
}
EXPORT_SYMBOL_GPL(pi_cam_put);

//-------------------------------------------------------------------------------------------------
// Helper functions

static int pi_cam_write(struct i2c_client *client, u16 reg, u8 data)
{
	u8 buf[3] = {reg>>8, reg&0xFF, data};
	
	return i2c_master_send(client, buf, 3);
}

static int pi_cam_read(struct i2c_client *client, u16 reg, u8* data)
{
	u8 buf[2] = {reg>>8, reg&0xFF};

	struct i2c_msg msg[2];
	
	
	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = buf;
	
	msg[1].addr = client->addr;
	msg[1].flags = client->flags & I2C_M_TEN;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data;
	
	return i2c_transfer(client->adapter, msg, 2);
}

static bool pi_cam_check_1_3(struct i2c_client *client)
{
	u8 data = 0;
	int ret;
	
	ret = pi_cam_read(client, CS_CMMN_CHIP_ID_H, &data);
	
	dev_warn(&client->dev, "pi_cam_check_1_3: data: %02X ret: %d\n", data, ret);
	
	ret = pi_cam_read(client, CS_CMMN_CHIP_ID_L, &data);
	
	dev_warn(&client->dev, "pi_cam_check_1_3: data: %02X ret: %d\n", data, ret);
	
	return true;
}

static bool pi_cam_check_2_1(struct i2c_client *client)
{
	u16 model_id, chip_id;
	u32 lot_id;
	u8 data = 0;
	int ret;
	
	ret = pi_cam_read(client, 0x0000, &data);	// Model ID high byte
	if(ret != 2)
	{
		dev_err(&client->dev, "Reading Model ID high byte failed\n");
		return false;
	}
	model_id = data << 8;
	
	ret = pi_cam_read(client, 0x0001, &data);	// Model ID low byte
	if(ret != 2)
	{
		dev_err(&client->dev, "Reading Model ID low byte failed\n");
		return false;
	}
	model_id |= data;
	
	ret = pi_cam_read(client, 0x0004, &data);	// Lot ID high byte
	if(ret != 2)
	{
		dev_err(&client->dev, "Reading Lot ID high byte failed\n");
		return false;
	}
	lot_id = data << 16;
	
	ret = pi_cam_read(client, 0x0005, &data);	// Lot ID mid byte
	if(ret != 2)
	{
		dev_err(&client->dev, "Reading Lot ID mid byte failed\n");
		return false;
	}
	lot_id |= data << 8;
	
	ret = pi_cam_read(client, 0x0006, &data);	// Lot ID low byte
	if(ret != 2)
	{
		dev_err(&client->dev, "Reading Lot ID low byte failed\n");
		return false;
	}
	lot_id |= data;
	
	ret = pi_cam_read(client, 0x000D, &data);	// Chip ID high byte
	if(ret != 2)
	{
		dev_err(&client->dev, "Reading Chip ID high byte failed\n");
		return false;
	}
	chip_id = data << 8;
	
	ret = pi_cam_read(client, 0x000E, &data);	// Chip ID low byte
	if(ret != 2)
	{
		dev_err(&client->dev, "Reading Chip ID low byte failed\n");
		return false;
	}
	chip_id |= data;
	
	if(model_id != 0x0219)
	{
		dev_err(&client->dev, "Model not supported (0x%04X)", model_id);
		return false;
	}
	
	dev_warn(&client->dev, "Found 2.1 Camera Model ID 0x%04X, Lot ID 0x%06X, Chip ID 0x%04X\n", model_id, lot_id, chip_id);
	
	return true;
}

static bool pi_cam_imx219_crop(struct i2c_client *client, struct sensor_rect crop_rect)
{
	pi_cam_write(client, 0x0164, crop_rect.left >> 8);
	pi_cam_write(client, 0x0165, crop_rect.left & 0xff);
	pi_cam_write(client, 0x0166, (crop_rect.width - 1) >> 8);
	pi_cam_write(client, 0x0167, (crop_rect.width - 1) & 0xff);
	pi_cam_write(client, 0x0168, crop_rect.top >> 8);
	pi_cam_write(client, 0x0169, crop_rect.top & 0xff);
	pi_cam_write(client, 0x016A, (crop_rect.height - 1) >> 8);
	pi_cam_write(client, 0x016B, (crop_rect.height - 1) & 0xff);
	pi_cam_write(client, 0x016C, crop_rect.width >> 8);
	pi_cam_write(client, 0x016D, crop_rect.width & 0xff);
	pi_cam_write(client, 0x016E, crop_rect.height >> 8);
	pi_cam_write(client, 0x016F, crop_rect.height & 0xff);
	
	return true;
}

static bool pi_cam_write_cfg(struct i2c_client *client, struct sensor_cmd* set)
{
	int i;
	for(i=0; set[i].reg != TABLE_END; i++)
	{
		pi_cam_write(client, set[i].reg, set[i].val);
	}
	
	return true;
}

//-------------------------------------------------------------------------------------------------
// Device usage

static int pi_cam_set_resolution(struct pi_cam_device *pDevice, unsigned width, unsigned height)
{
	return 0;
}
EXPORT_SYMBOL_GPL(pi_cam_set_resolution);

static int pi_cam_start(struct pi_cam_device *pDevice)
{
	return 0;
}
EXPORT_SYMBOL_GPL(pi_cam_start);

//-------------------------------------------------------------------------------------------------
// Registration and Unregistration
 
static void pi_cam_register_device(struct pi_cam_device *pDevice)
{
	mutex_lock(&pi_cam_lock);
	list_add_tail(&pDevice->list, &pi_cam_list);
	mutex_unlock(&pi_cam_lock);
}

static void pi_cam_unregister_device(struct pi_cam_device *pDevice)
{
	mutex_lock(&pi_cam_lock);
	list_del(&pDevice->list);
	mutex_unlock(&pi_cam_lock);
}

//-------------------------------------------------------------------------------------------------
// Probe
 
static int pi_cam_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
  struct device *dev = &client->dev;
	struct pi_cam_device* pData;

	dev_warn(dev, "Probe of %s (0x%2X) starting\n", client->name, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		dev_err(dev, "I2C functionality check failed\n");
		return -ENODEV;
	}
	
	if(pi_cam_check_2_1(client))
	{
		pi_cam_imx219_crop(client, imx219_center_1280x720_rect);
		pi_cam_write_cfg(client, imx219_720p_regs);
		//pi_cam_write_cfg(client, imx219_test_color_bar);
		pi_cam_write_cfg(client, imx219_start);
		
	}
	
	// Allocate memory for driver structure
	/*pDevice = devm_kzalloc(&pdev->dev, sizeof(*pDevice), GFP_KERNEL);
	if(!pDevice)
	{
		dev_err(&pdev->dev, "Allocation error\n");
		return -ENOMEM;
	}
	
	// Save device
	pDevice->pDev = &pdev->dev;
	
	// Set driver data
	platform_set_drvdata(pdev, pDevice);	

	// Register device
	pi_cam_register_device(pDevice);
	*/
	dev_warn(dev, "Probe successful\n");
	
	return 0;
}

static int pi_cam_remove(struct i2c_client *client)
{
	/*struct pi_cam_device* pDevice = platform_get_drvdata(pdev);

	pi_cam_unregister_device(pDevice);
	*/
	dev_info(&client->dev, "Removal successful\n");

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver definition
static const struct i2c_device_id pi_cam_idtable[] = {
	{ "pi-cam-1.3", 0x36 },
	{ "pi-cam-2.1", 0x10 },
 {}
};

#ifdef CONFIG_OF
static const struct of_device_id pi_cam_of_match[] = {
	{.compatible = "jf,pi-cam-1.0.0"},
	{},
};
MODULE_DEVICE_TABLE(of, pi_cam_of_match);
#endif

static struct i2c_driver pi_cam_driver = {
	.driver = {
		.name = "pi_cam",
		.owner = THIS_MODULE,
    .of_match_table = of_match_ptr(pi_cam_of_match),
	},
  .id_table = pi_cam_idtable,
	.probe = pi_cam_probe,
	.remove = pi_cam_remove,
};

module_i2c_driver(pi_cam_driver);
