#include <linux/module.h>			// included for all kernel modules
#include <linux/init.h>				// included for __init and __exit macros
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/io.h>

#define STPG_NO_EXPORT
#include "stpg.h"
#include "stpg_hw.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Johannes Feldmann");
MODULE_DESCRIPTION("Simple Test Pattern Generator Driver");

///////////////////////////////////////////////////////////////////////////////////////////////////
// Devicetree structures
/*
stpg_0: stpg@12345678 {
	compatible = "jf,stpg-1.0.0";
	reg = <0x12345678 0x10000>;
	clocks = <&clkc 15>;
};
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver structures

static LIST_HEAD(stpg_list);
static DEFINE_MUTEX(stpg_lock);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Function definitions

//-------------------------------------------------------------------------------------------------
// Get device / Put device
static struct stpg_device* stpg_get(struct device_node* np)
{
	struct device_node *stpg_node;
	struct stpg_device *found = NULL;
	struct stpg_device *stpg;

	if(!of_find_property(np, "jf,stpg", NULL))
	{
		return NULL;
	}	
	
	stpg_node = of_parse_phandle(np, "jf,stpg", 0);
	if (stpg_node == NULL)
		return ERR_PTR(-EINVAL);

	mutex_lock(&stpg_lock);
	list_for_each_entry(stpg, &stpg_list, list) {
		if (stpg->pDev->of_node == stpg_node) {
			found = stpg;
			break;
		}
	}
	mutex_unlock(&stpg_lock);

	of_node_put(stpg_node);

	if (!found)
		return ERR_PTR(-EPROBE_DEFER);

	return found;
}
EXPORT_SYMBOL_GPL(stpg_get);

static void stpg_put(struct stpg_device *pDevice)
{
}
EXPORT_SYMBOL_GPL(stpg_put);

//-------------------------------------------------------------------------------------------------
// Device usage

static int stpg_set_resolution(struct stpg_device *pDevice, unsigned width, unsigned height)
{
	// Set width
	iowrite32(width, pDevice->pIoMem + XSTPG_AXILITES_ADDR_WIDTH_V_DATA);
	
	// Set height
	iowrite32(height, pDevice->pIoMem + XSTPG_AXILITES_ADDR_HEIGHT_V_DATA);
		
	return 0;
}
EXPORT_SYMBOL_GPL(stpg_set_resolution);

static int stpg_start(struct stpg_device *pDevice)
{
	// Set start and auto restart
	iowrite32(0x81, pDevice->pIoMem + XSTPG_AXILITES_ADDR_AP_CTRL);
	
	return 0;
}
EXPORT_SYMBOL_GPL(stpg_start);

//-------------------------------------------------------------------------------------------------
// Registration and Unregistration
 
static void stpg_register_device(struct stpg_device *pDevice)
{
	mutex_lock(&stpg_lock);
	list_add_tail(&pDevice->list, &stpg_list);
	mutex_unlock(&stpg_lock);
}

static void stpg_unregister_device(struct stpg_device *pDevice)
{
	mutex_lock(&stpg_lock);
	list_del(&pDevice->list);
	mutex_unlock(&stpg_lock);
}

//-------------------------------------------------------------------------------------------------
// Probe
 
static int stpg_probe(struct platform_device *pdev)
{
	struct stpg_device* pDevice;
	struct resource *pRes;

	// Allocate memory for driver structure
	pDevice = devm_kzalloc(&pdev->dev, sizeof(*pDevice), GFP_KERNEL);
	if(!pDevice)
	{
		dev_err(&pdev->dev, "Allocation error\n");
		return -ENOMEM;
	}
	
	// Enable Clock
	pDevice->pClk = devm_clk_get(&pdev->dev, NULL);
	if(IS_ERR(pDevice->pClk))
	{
		dev_err(&pdev->dev, "Clock error\n");
		return PTR_ERR(pDevice->pClk);
	}
	clk_prepare_enable(pDevice->pClk);
	
	// Init IO resources
	pRes = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pDevice->pIoMem = devm_ioremap_resource(&pdev->dev, pRes);
	if(IS_ERR(pDevice->pIoMem))
	{
		dev_err(&pdev->dev, "IORESOURCE_MEM error\n");
		return PTR_ERR(pDevice->pIoMem);
	}
	
	// Save device
	pDevice->pDev = &pdev->dev;
	
	// Set driver data
	platform_set_drvdata(pdev, pDevice);	

	// Register device
	stpg_register_device(pDevice);
	
	dev_warn(&pdev->dev, "Probe successful\n");
	
	return 0;
}

static int stpg_remove(struct platform_device *pdev)
{
	struct stpg_device* pDevice = platform_get_drvdata(pdev);

	stpg_unregister_device(pDevice);
	
	clk_disable_unprepare(pDevice->pClk);
	
	dev_info(&pdev->dev, "video-output: Removal successful\n");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver definition
static const struct of_device_id stpg_of_ids[] = {
	{ .compatible = "jf,stpg-1.0.0",},
	{}
};

static struct platform_driver stpg_driver = {
	.driver = {
		.name = "stpg",
		.owner = THIS_MODULE,
		.of_match_table = stpg_of_ids,
	},
	.probe = stpg_probe,
	.remove = stpg_remove,
};

module_platform_driver(stpg_driver);
