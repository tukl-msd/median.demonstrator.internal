#include <linux/module.h>			// included for all kernel modules
#include <linux/init.h>				// included for __init and __exit macros
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include "../xilinx-vtc/xilinx-vtc.h"
#include "../stpg/stpg.h"
#include <linux/dma/xilinx_dma.h>

// File access
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Johannes Feldmann");
MODULE_DESCRIPTION("This module manages the video output to hdmi");

///////////////////////////////////////////////////////////////////////////////////////////////////
// Devicetree structures
/*
vio_0: vio {
	compatible = "jf,video-output-1.0.0";
	
	xlnx,vtc = <&vtc_0>;
	dmas = <&axi_vdma_out 0 &axi_vdma_in 0>;
	dma-names = "vdma_out", "vdma_in";
};
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver structures

typedef enum ePicState
{
	PIC_CONFIG,
	PIC_DATA
};

struct picture_pos {
	unsigned x;
	unsigned y;
	unsigned col;
};

struct picture_state {
	enum ePicState state;
	int nWidth;
	int nHeight;
	int nBmpWidth;
	int nBmpHeight;
	struct picture_pos CurPos;
};

struct vio_state {
	struct device*			pDev;
	struct xvtc_device* 	pVtc;
	struct stpg_device* 	pStpg;
	struct dma_chan*		pVDMA;
	u32*					pBuffer;
	dma_addr_t 				dma_addr;	
	dma_cookie_t 			tx_cookie;
	struct picture_state	pic;
};

struct __attribute__((__packed__)) bmp_header {
    u16 bfType;
    u32 bfSize;
    u32 _reserved;
	u32 bfOffBits;
	u32 biSize;
	u32 biWidth;
	u32 biHeight;
	u16 biPlanes;
	u16 biBitCount;
	u32 biCompression;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver attributes

ssize_t picture_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct vio_state* pState = platform_get_drvdata(to_platform_device(dev));
	struct bmp_header* pHeader = (struct bmp_header*) buf; 
	int i;
	unsigned buffer_size = pState->pic.nWidth*pState->pic.nHeight*sizeof(u32);
	u8* out = (u8*)pState->pBuffer;
	
	//printk(KERN_INFO "Count = %u\n", count);
	
	if(pState->pic.state == PIC_CONFIG)
	{
		// Bitmap header is 54 Bytes, so that should be at least the size of the incoming data.
		if(count < 54)
		{
			dev_err(dev, "Size too small\n");
			return count;
		}
		
		// Test the type
		if(pHeader->bfType != 0x4D42) // 0x424D = "BM" <- BMP Identifier
		{
			dev_err(dev, "No BMP file\n");
			return count;
		}
		
		// Save Width, Height
		pState->pic.nBmpWidth = pHeader->biWidth;
		pState->pic.nBmpHeight = pHeader->biHeight;
		
		dev_info(dev, "Width: %d Height: %d\n", pState->pic.nBmpWidth, pState->pic.nBmpHeight);
		
		// Save number of bits per pixel
		if(pHeader->biBitCount != 24)
		{
			dev_err(dev, "Number of Bits is not 24\n");
			return count;			
		}
		
		// Test if this picture is compressed -> not supported
		if(pHeader->biCompression != 0)
		{
			dev_err(dev, "BMP is compressed - not supported\n");
			return count;
		}
		
		pState->pic.state = PIC_DATA;
		pState->pic.CurPos.x = 0;
		pState->pic.CurPos.y = 0;
		pState->pic.CurPos.col = 0;

		dev_info(dev, "Goto %u\n", pHeader->bfOffBits);
		
		// Clear output
		memset(pState->pBuffer, 0, buffer_size);
		
		// Type OK, use the offset to jump to data:
		return pHeader->bfOffBits;
	}
	else
	{			
		// Get Data
		i = 0;
		while(i < count)
		{
			out[(pState->pic.nBmpHeight - pState->pic.CurPos.y - 1) * pState->pic.nWidth * sizeof(u32) + \
				(pState->pic.CurPos.x * sizeof(u32)) + (2 - pState->pic.CurPos.col)] = buf[i];
			
			pState->pic.CurPos.col++;
			if(pState->pic.CurPos.col > 2)
			{
				pState->pic.CurPos.col = 0;
				pState->pic.CurPos.x++;				
				if(pState->pic.CurPos.x == pState->pic.nBmpWidth)
				{
					pState->pic.CurPos.x = 0;
					pState->pic.CurPos.y++;
					if(pState->pic.CurPos.y == pState->pic.nBmpHeight)
					{
						// We are done
						dev_info(dev, "BMP picture loaded\n");
						pState->pic.state = PIC_CONFIG;
						return count;
					}
				}
			}
			
			i++;
		}
		
		dev_info(dev, "BMP loading data (%u)\n", count);
		
		return count;
	}
};

static DEVICE_ATTR_WO(picture);


static struct attribute *dev_attrs[] = {
	&dev_attr_picture.attr,
	NULL,
};

static struct attribute_group dev_group = {
//    .name = "",
    .attrs = dev_attrs,
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Function definitions

// Save Clean Up function
void CleanUp(struct vio_state* pState)
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
}

//-------------------------------------------------------------------------------------------------
// Probe 
static int vio_probe(struct platform_device *pdev)
{
	struct vio_state* pState;
	struct dma_interleaved_template dma_it;
	struct dma_async_tx_descriptor *txd = NULL;
	int x,y, ret;

	dev_warn(&pdev->dev, "Probe started\n");
	
	// Allocate memory for driver structure
	pState = devm_kzalloc(&pdev->dev, sizeof(*pState), GFP_KERNEL);
	if(!pState)
		return -ENOMEM;
	
	// Allocate memory for picture
	pState->pBuffer = dma_zalloc_coherent(&pdev->dev, 1280*720*sizeof(u32), &pState->dma_addr, GFP_KERNEL);
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
	/*pState->pStpg = stpg_get(pdev->dev.of_node);
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
	*/
	// Get VDMA channel
	pState->pVDMA = dma_request_slave_channel(&pdev->dev, "vdma_out");
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
	pState->pic.state = PIC_CONFIG;
	pState->pic.nWidth = 1280;
	pState->pic.nHeight = 720;
	platform_set_drvdata(pdev, pState);	

	ret = sysfs_create_group(&pdev->dev.kobj, &dev_group);
    if (ret) {
        dev_err(&pdev->dev, "sysfs creation failed\n");
        return ret;
    }
	
	dev_warn(&pdev->dev, "Probe successful\n");
	
	return 0;
}

static int vio_remove(struct platform_device *pdev)
{
	struct vio_state* pState = platform_get_drvdata(pdev);
	
	CleanUp(pState);
	
	sysfs_remove_group(&pdev->dev.kobj, &dev_group);
	
	dev_info(&pdev->dev, "Removal successful\n");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver definition
static const struct of_device_id vio_of_ids[] = {
	{ .compatible = "jf,vio-1.0.0",},
	{}
};

static struct platform_driver vio_driver = {
	.driver = {
		.name = "vio",
		.owner = THIS_MODULE,
		.of_match_table = vio_of_ids,
	},
	.probe = vio_probe,
	.remove = vio_remove,
};

module_platform_driver(vio_driver);
