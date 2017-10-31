#include <linux/module.h>			// included for all kernel modules
#include <linux/init.h>				// included for __init and __exit macros
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Johannes Feldmann");
MODULE_DESCRIPTION("This module populates all child nodes.");

///////////////////////////////////////////////////////////////////////////////////////////////////
// Devicetree structures
/*
container {
	compatible = "simple-container";
};
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver structures
struct sc_state {
	struct device	*pDev;
};



static int sc_match(struct device *dev, struct device_driver *drv)
{
	dev_info(dev, "Match\n");
	return 0;
}


struct bus_type sc_type = {
       .name	= "simple-container",
       .match	= sc_match,
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Function definitions
static int sc_notify(struct notifier_block *nb,
				unsigned long action, void *arg)
{
	pr_err("Notify\n");
	
	return NOTIFY_OK;
}

static struct notifier_block sf_notifier = {
	.notifier_call = sc_notify,
};

static int sc_probe(struct platform_device *pdev)
{
	struct sc_state* pState;
	int ret;

	// Allocate memory for driver structure
	pState = devm_kzalloc(&pdev->dev, sizeof(*pState), GFP_KERNEL);
	if(!pState)
		return -ENOMEM;
	
	ret = of_platform_default_populate(pdev->dev.of_node, NULL, NULL);
	if(ret != 0)
	{
		pr_err("Device population failed (%d)", ret);
		return ret;
	}
	
	ret = of_reconfig_notifier_register(&sf_notifier);
	if(ret != 0)
	{
		pr_err("Register notifier failed (%d)", ret);
		return ret;		
	}
	
	/*ret = bus_register(&sc_type);
	if(ret != 0)
	{
		pr_err("Bus register failed (%d)", ret);
		return ret;
	}*/

	// Set driver data
	platform_set_drvdata(pdev, pState);	
	
	/*// Allocate memory for driver structure
	pState = devm_kzalloc(&pdev->dev, sizeof(*pState), GFP_KERNEL);
	if(!pState)
		return -ENOMEM;
	
	// Save device
	pState->pDev = &pdev->dev;
	
	// Set driver data
	platform_set_drvdata(pdev, pState);
	
	// Get clock
	pState->pClk = devm_clk_get(&pdev->dev, NULL);
	if(IS_ERR(pState->pClk))
	{
		dev_err(&pdev->dev, "Failed to get clock");
		return PTR_ERR(pState->pClk);
	}
	
	// Get Clock frequency
	of_property_read_u32(pdev->dev.of_node, "freq-in-hz", &pState->nFreq);
	
	ret = clk_set_rate(pState->pClk, pState->nFreq);
	if(ret != 0)
	{
		dev_err(&pdev->dev, "Frequency set to %d failed", pState->nFreq);
		return ret;
	}
	
	dev_info(&pdev->dev, "Frequency set to %d", pState->nFreq);
	
	// Enable clock
	//ret = clk_prepare_enable(pState->pClk);
	if(ret != 0)
	{
		dev_err(&pdev->dev, "Failed to enalbe Clock");
		return ret;
	}
	*/
	dev_info(&pdev->dev, "Probe successful for (%s)\n", pdev->name);
	
	return 0;
}

static int sc_resume(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Resume\n");
	return 0;
}

static int sc_suspend(struct platform_device *pdev, pm_message_t state)
{
	dev_info(&pdev->dev, "Suspend\n");
	return 0;
}

static void sc_shutdown(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Shutdown\n");
}

static int sc_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Removal successful\n");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver definition
static const struct of_device_id sc_of_ids[] = {
	{ .compatible = "simple-container",},
	{}
};

static struct platform_driver sc_driver = {
	.driver = {
		.name = "simple-container",
		.owner = THIS_MODULE,
		.of_match_table = sc_of_ids,
	},
	.probe = sc_probe,
	.remove = sc_remove,
	.shutdown = sc_shutdown,
	.suspend = sc_suspend,
	.resume = sc_resume,
};

module_platform_driver(sc_driver);
