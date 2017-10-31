#include <linux/module.h>			// included for all kernel modules
#include <linux/init.h>				// included for __init and __exit macros
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include "../xilinx-vtc/xilinx-vtc.h"
#include "../stpg/stpg.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Johannes Feldmann");
MODULE_DESCRIPTION("This module manages the video output to hdmi");

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
struct video_output_state {
	struct device	*pDev;
	struct xvtc_device* pVtc;
	struct stpg_device* pStpg;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Function definitions
static int video_output_probe(struct platform_device *pdev)
{
	struct device_node *xvtc_node;
	struct video_output_state* pState;
	int ret;

	dev_warn(&pdev->dev, "Probe started\n");
	
	// Allocate memory for driver structure
	pState = devm_kzalloc(&pdev->dev, sizeof(*pState), GFP_KERNEL);
	if(!pState)
		return -ENOMEM;
	
	// Save device
	pState->pDev = &pdev->dev;

	// Get Simple Test Pattern Generator
	pState->pStpg = stpg_get(pdev->dev.of_node);
	if(pState->pStpg == NULL || pState->pStpg == ERR_PTR(-EINVAL) || pState->pStpg == ERR_PTR(-EPROBE_DEFER))
	{
		dev_err(&pdev->dev, "Failed to get stpg device (%d,%d,%d)", (int)pState->pStpg, EINVAL, EPROBE_DEFER);
		return (int)pState->pVtc;
	}
	
	// Set Resolution
	stpg_set_resolution(pState->pStpg, 1280, 720);
	
	// Start Generator
	stpg_start(pState->pStpg);
	
	// Get vtc device
	pState->pVtc = xvtc_of_get(pdev->dev.of_node);
	if(pState->pVtc == NULL || pState->pVtc == ERR_PTR(-EINVAL) || pState->pVtc == ERR_PTR(-EPROBE_DEFER))
	{
		dev_err(&pdev->dev, "Failed to get vtc device (%d,%d,%d)", (int)pState->pVtc, EINVAL, EPROBE_DEFER);
		return (int)pState->pVtc;
	}
	
	// Setup vtc config
	const struct xvtc_config cfg = {
		.hblank_start = 1280,
		.hsync_start = 1390,
		.hsync_end = 1430,
		.hsize = 1650,
		.vblank_start = 720,
		.vsync_start = 724,
		.vsync_end = 729,
		.vsize = 750
	};
	
	// Enable vtc Generator
	ret = xvtc_generator_start(pState->pVtc, &cfg);
	if(ret != 0)
	{
		dev_err(&pdev->dev, "Failed to enable vtc generator");
		return ret;
	}
	
	// Set driver data
	platform_set_drvdata(pdev, pState);	

	dev_warn(&pdev->dev, "Probe successful\n");
	
	return 0;
}

static int video_output_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Removal successful\n");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver definition
static const struct of_device_id video_output_of_ids[] = {
	{ .compatible = "jf,video-output-1.0.0",},
	{}
};

static struct platform_driver video_output_driver = {
	.driver = {
		.name = "video-output",
		.owner = THIS_MODULE,
		.of_match_table = video_output_of_ids,
	},
	.probe = video_output_probe,
	.remove = video_output_remove,
};

module_platform_driver(video_output_driver);
