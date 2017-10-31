#include <linux/module.h>			// included for all kernel modules
#include <linux/init.h>				// included for __init and __exit macros
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include "../xilinx-vtc/xilinx-vtc.h"
#include "../stpg/stpg.h"
#include <linux/dma/xilinx_dma.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Johannes Feldmann");
MODULE_DESCRIPTION("This module manages the video output to hdmi");

///////////////////////////////////////////////////////////////////////////////////////////////////
// Devicetree structures
/*
video_output_0: video_output {
	compatible = "jf,video-output-1.0.0";
	
	jf,stpg = <&stpg_0>;
	xlnx,vtc = <&vtc_0>;
	dmas = <&axi_vdma_0 0>;
	dma-names = "vdma0";
};
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver structures
struct video_output_state {
	struct device*		pDev;
	struct xvtc_device* pVtc;
	struct stpg_device* pStpg;
	struct dma_chan*	pVDMA;
	u32*				pBuffer;
	dma_addr_t 			dma_addr;	
	dma_cookie_t 		tx_cookie;
};

// Test
static void Test(void *arg)
{
	pr_debug("Got tx callback\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Function definitions

// Save Clean Up function
void CleanUp(struct video_output_state* pState)
{
	enum dma_status status;
	
	// Stop VDMA
	const struct xilinx_vdma_config vdma_cfg = {
		.frm_dly = 0,				// @frm_dly: Frame delay
		.gen_lock = 0,				// @gen_lock: Whether in gen-lock mode
		.master = 0,				// @master: Master that it syncs to
		.frm_cnt_en = 0,			// @frm_cnt_en: Enable frame count enable
		.park = 0,					// @park: Whether wants to park
		.park_frm = 0,				// @park_frm: Frame to park on
		.coalesc = 0,				// @coalesc: Interrupt coalescing threshold
		.delay = 0,					// @delay: Delay counter
		.reset = 1,					// @reset: Reset Channel
		.ext_fsync = 0,				// @ext_fsync: External Frame Sync source
	};
	xilinx_vdma_channel_set_config(pState->pVDMA, &vdma_cfg);
	
	// Unmap VDMA
	if(pState->pVDMA && pState->pBuffer)
	{
		dma_free_coherent(pState->pVDMA->device->dev, pState->pBuffer, 1280*720*sizeof(u32), DMA_MEM_TO_DEV);
	}
	
	// Release vdma channel
	if(pState->pVDMA)
	{
		dma_release_channel(pState->pVDMA);
	}
	
	// Free memory
	if(pState->pBuffer)
	{	
		kfree(pState->pBuffer);
	}	
}

//-------------------------------------------------------------------------------------------------
// Probe 
static int video_output_probe(struct platform_device *pdev)
{
	struct video_output_state* pState;
	struct dma_interleaved_template dma_it;
	struct dma_async_tx_descriptor *txd = NULL;
	int x,y, ret;

	dev_warn(&pdev->dev, "Probe started\n");
	
	// Allocate memory for driver structure
	pState = devm_kzalloc(&pdev->dev, sizeof(*pState), GFP_KERNEL);
	if(!pState)
		return -ENOMEM;
	
	// Allocate memory for picture
	pState->pBuffer = kcalloc(1280*720, sizeof(u32), GFP_KERNEL);
	if(!pState->pBuffer)
	{
		CleanUp(pState);
		dev_err(&pdev->dev, "Picture Buffer allocation failed\n");
		return -ENOMEM;
	}
	dev_warn(&pdev->dev, "Buffer created at %08X\n", pState->pBuffer);
	
	// Insert random data
	for(y = 0; y < 720; y++)
	{
		for(x = 0; x < 1280; x++)
		{
			u8 r, g, b;
			r = x/5;
			g = 255 - x/5;
			b = y/3;
			pState->pBuffer[x+y*1280] = (r << 0) + (g << 8) + (b << 16);
		}
	}
	// Save device
	pState->pDev = &pdev->dev;

	// Get Simple Test Pattern Generator
	pState->pStpg = stpg_get(pdev->dev.of_node);
	if(pState->pStpg == NULL || pState->pStpg == ERR_PTR(-EINVAL) || pState->pStpg == ERR_PTR(-EPROBE_DEFER))
	{
		CleanUp(pState);
		dev_err(&pdev->dev, "Failed to get stpg device (%d,%d,%d)", (int)pState->pStpg, EINVAL, EPROBE_DEFER);
		return (int)pState->pVtc;
	}
	
	// Set Resolution
	stpg_set_resolution(pState->pStpg, 1280, 720);
	
	// Start Generator
	stpg_start(pState->pStpg);
	
	// Get VDMA channel
	pState->pVDMA = dma_request_slave_channel(&pdev->dev, "vdma0");
	if(!pState->pVDMA)
	{
		CleanUp(pState);
		dev_err(&pdev->dev, "No Tx VDMA channel\n");
		return PTR_ERR(pState->pVDMA);
	}
	dev_warn(&pdev->dev, "Slave channel :%08X\n", pState->pVDMA);
	
	// Configure VDMA
	const struct xilinx_vdma_config vdma_cfg = {
		.frm_dly = 0,				// @frm_dly: Frame delay
		.gen_lock = 0,				// @gen_lock: Whether in gen-lock mode
		.master = 0,				// @master: Master that it syncs to
		.frm_cnt_en = 0,			// @frm_cnt_en: Enable frame count enable
		.park = 1,					// @park: Whether wants to park
		.park_frm = 0,				// @park_frm: Frame to park on
		.coalesc = 0,				// @coalesc: Interrupt coalescing threshold
		.delay = 0,					// @delay: Delay counter
		.reset = 0,					// @reset: Reset Channel
		.ext_fsync = 0,				// @ext_fsync: External Frame Sync source
	};
	xilinx_vdma_channel_set_config(pState->pVDMA, &vdma_cfg);
	
	pState->dma_addr = dma_map_single(pState->pVDMA->device->dev, pState->pBuffer, 1280*720*sizeof(u32), DMA_MEM_TO_DEV);
	if(dma_mapping_error(pState->pVDMA->device->dev, pState->dma_addr))
	{
		CleanUp(pState);
		dev_err(&pdev->dev, "Mapping failed\n");
		return PTR_ERR(pState->dma_addr);
	}
	dev_warn(&pdev->dev, "Mapping done %08X to %08X", (unsigned)pState->pBuffer, (unsigned)pState->dma_addr);
	
	dma_it.src_start = pState->dma_addr;		// @src_start: Bus address of source for the first chunk.
	dma_it.dir = DMA_MEM_TO_DEV;				// @dir: Specifies the type of Source and Destination.
	dma_it.src_inc = false;						// @src_inc: If the source address increments after reading from it.
	dma_it.src_sgl = false;						// @src_sgl: If the 'icg' of sgl[] applies to Source (scattered read).
												// Otherwise, source is read contiguously (icg ignored).
												// Ignored if src_inc is false.
	dma_it.numf = 720;							// @numf: Number of frames in this template.
	dma_it.sgl[0].size = 1280 * sizeof(u32);	// @size: Number of bytes to read from source.
	dma_it.sgl[0].icg = 0;						// @icg: Number of bytes to jump after last src/dst address of this 
												// chunk and before first src/dst address for next chunk.
	dma_it.frame_size = 1;						// @frame_size: Number of chunks in a frame i.e, size of sgl[].
		
	txd = pState->pVDMA->device->device_prep_interleaved_dma(pState->pVDMA, &dma_it, DMA_CTRL_ACK);
	if(!txd)
	{
		CleanUp(pState);
		dev_err(&pdev->dev, "Async transaction descriptor invalid\n");
		return -1;
	}
	dev_warn(&pdev->dev, "Device preperation txd: %08X", (unsigned)txd);
	
	txd->callback = Test;
	txd->callback_param = 0;

	pState->tx_cookie = txd->tx_submit(txd);
	if(dma_submit_error(pState->tx_cookie))
	{
		CleanUp(pState);
		dev_err(&pdev->dev, "DMA submit error\n");
		return -1;
	}
	
	
	// Get vtc device
	pState->pVtc = xvtc_of_get(pdev->dev.of_node);
	if(pState->pVtc == NULL || pState->pVtc == ERR_PTR(-EINVAL) || pState->pVtc == ERR_PTR(-EPROBE_DEFER))
	{
		CleanUp(pState);
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
		CleanUp(pState);
		dev_err(&pdev->dev, "Failed to enable vtc generator");
		return ret;
	}
	dma_async_issue_pending(pState->pVDMA);
	
	// Set driver data
	platform_set_drvdata(pdev, pState);	

	dev_warn(&pdev->dev, "Probe successful\n");
	
	return 0;
}

static int video_output_remove(struct platform_device *pdev)
{
	struct video_output_state* pState = platform_get_drvdata(pdev);
	
	CleanUp(pState);
	
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
