#include <linux/module.h>			// included for all kernel modules
#include <linux/init.h>				// included for __init and __exit macros
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Johannes Feldmann");
MODULE_DESCRIPTION("This module configures an fclk with settings from a device tree");

///////////////////////////////////////////////////////////////////////////////////////////////////
// Devicetree structures
/*
fclk_0: fclk@0 {
	compatible = "jf,fclk-1.0.0";
	reg = <0>;
	
	clocks = <&clkc 15>;
	freq-in-hz = <200000000>;
};
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver structures
struct fclk_state {
	struct device	*pDev;
	struct clk		*pClk;
	u32				nFreq;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Function definitions
static int fclk_probe(struct platform_device *pdev)
{
	struct fclk_state *pState;
	int ret;

	// Allocate memory for driver structure
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
	
	dev_info(&pdev->dev, "Probe successful for (%s)\n", pdev->name);
	
	return 0;
}

static int fclk_remove(struct platform_device *pdev)
{
	//struct fclk_state *pState = platform_get_drvdata(pdev);

	//clk_disable_unprepare(pState->pClk);
	
	dev_info(&pdev->dev, "Removal successful\n");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver definition
static const struct of_device_id fclk_of_ids[] = {
	{ .compatible = "jf,fclk-1.0.0",},
	{}
};

static struct platform_driver fclk_driver = {
	.driver = {
		.name = "fclk",
		.owner = THIS_MODULE,
		.of_match_table = fclk_of_ids,
	},
	.probe = fclk_probe,
	.remove = fclk_remove,
};

module_platform_driver(fclk_driver);
