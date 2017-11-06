#include <linux/module.h>			// included for all kernel modules
#include <linux/init.h>				// included for __init and __exit macros
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include "../xilinx-vtc/xilinx-vtc.h"
#include <linux/dma/xilinx_dma.h>

#include "../stpg/stpg.h"
#include "../pi-cam/pi-cam.h"

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

enum ePicState
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

struct cam_state {
	struct i2c_client*		pCam;
	struct dma_async_tx_descriptor* pDesc;
	bool					bEnabled;
};

struct vio_state {
	struct device*			pDev;
	struct xvtc_device* 	pVtc;
	struct stpg_device* 	pStpg;
	struct dma_chan*		pVMDA_out;
	struct dma_chan*		pVMDA_in;
	u32*					pBuffer;
	dma_addr_t 				dma_addr;	
	struct dma_async_tx_descriptor* txd;
	struct dma_async_tx_descriptor* rxd;
	dma_cookie_t 			tx_cookie;
	dma_cookie_t 			rx_cookie;
	struct picture_state	pic;
	struct cam_state		cam;
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
// Driver helper functions

static void WriteBuffer(struct vio_state* pState)
{
	int x,y;
	// Insert fancy colors
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
}

static void SetTxVDMAConfig(struct vio_state* pState)
{
	// Configure VDMA
	struct xilinx_vdma_config vdma_cfg = {
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
	xilinx_vdma_channel_set_config(pState->pVMDA_out, &vdma_cfg);	
}

static void SetRxVDMAConfig(struct vio_state* pState)
{
	// Configure VDMA
	struct xilinx_vdma_config vdma_cfg = {
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
	xilinx_vdma_channel_set_config(pState->pVMDA_in, &vdma_cfg);
}

static bool SetTxVDMASetup(struct vio_state* pState)
{
	struct dma_async_tx_descriptor* txd = NULL;
	static struct dma_interleaved_template dma_it;
	
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
		
	txd = pState->pVMDA_out->device->device_prep_interleaved_dma(pState->pVMDA_out, &dma_it, DMA_CTRL_ACK);
	if(!txd)
	{
		dev_err(pState->pDev, "Async transaction descriptor TX invalid\n");
		return false;
	}

	pState->tx_cookie = txd->tx_submit(txd);
	if(dma_submit_error(pState->tx_cookie))
	{
		dev_err(pState->pDev, "DMA TX submit error\n");
		return false;
	}
	
	return true;
}

static bool SetRxVDMASetup(struct vio_state* pState)
{
	struct dma_async_tx_descriptor* rxd = NULL;
	static struct dma_interleaved_template dma_it;

	dma_it.dst_start = pState->dma_addr;		// @dst_start: Bus address of source for the first chunk.
	dma_it.dir = DMA_DEV_TO_MEM;				// @dir: Specifies the type of Source and Destination.
	dma_it.src_inc = false;						// @src_inc: If the source address increments after reading from it.
	dma_it.src_sgl = false;						// @src_sgl: If the 'icg' of sgl[] applies to Source (scattered read).
												// Otherwise, source is read contiguously (icg ignored).
												// Ignored if src_inc is false.
	dma_it.numf = 720;							// @numf: Number of frames in this template.
	dma_it.sgl[0].size = 1280 * sizeof(u32);	// @size: Number of bytes to read from source.
	dma_it.sgl[0].icg = 0;						// @icg: Number of bytes to jump after last src/dst address of this 
												// chunk and before first src/dst address for next chunk.
	dma_it.frame_size = 1;						// @frame_size: Number of chunks in a frame i.e, size of sgl[].
	
	rxd = pState->pVMDA_in->device->device_prep_interleaved_dma(pState->pVMDA_in, &dma_it, DMA_CTRL_ACK);
	if(!rxd)
	{
		dev_err(pState->pDev, "Async transaction descriptor RX invalid\n");
		return false;
	}

	pState->rx_cookie = rxd->tx_submit(rxd);
	if(dma_submit_error(pState->rx_cookie))
	{
		dev_err(pState->pDev, "DMA RX submit error\n");
		return false;
	}
	
	return true;
}

static void StartVDMA(struct dma_chan* chan)
{
	dma_async_issue_pending(chan);
}

static void StopVDMA(struct dma_chan* chan)
{
	dmaengine_terminate_async(chan);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver attributes

ssize_t picture_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct vio_state* pState = platform_get_drvdata(to_platform_device(dev));
	struct bmp_header* pHeader = (struct bmp_header*) buf; 
	int i;
	unsigned buffer_size = pState->pic.nWidth*pState->pic.nHeight*sizeof(u32);
	u8* out = (u8*)pState->pBuffer;
	
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
		pState->pic.CurPos.y = pState->pic.nBmpHeight - 1;
		pState->pic.CurPos.col = 2;

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
			out[(pState->pic.CurPos.y * pState->pic.nWidth + \
				(pState->pic.CurPos.x)) * sizeof(u32) + pState->pic.CurPos.col] = buf[i];
			
			pState->pic.CurPos.col--;
			if(pState->pic.CurPos.col == -1)
			{
				pState->pic.CurPos.col = 2;
				pState->pic.CurPos.x++;				
				if(pState->pic.CurPos.x == pState->pic.nBmpWidth)
				{
					pState->pic.CurPos.x = 0;
					pState->pic.CurPos.y--;
					if(pState->pic.CurPos.y == -1)
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
		
		return count;
	}
};

ssize_t pi_cam_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct vio_state* pState = platform_get_drvdata(to_platform_device(dev));

	if(buf[0] == '1')
	{
		if(SetRxVDMASetup(pState))
		{
			StartVDMA(pState->pVMDA_in);
			pi_cam_start(pState->cam.pCam);
			pState->cam.bEnabled = true;
		}
	}
	else if(buf[0] == '0')
	{
		pi_cam_stop(pState->cam.pCam);
		pState->cam.bEnabled = false;
		StopVDMA(pState->pVMDA_in);
		WriteBuffer(pState);
	}
	
	return count;
};

ssize_t pi_cam_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct vio_state* pState = platform_get_drvdata(to_platform_device(dev));

	if(pState->cam.bEnabled)
	{
		buf[0] = '1';
	}
	else
	{
		buf[0] = '0';
	}
	
	return 1;
};

static struct device_attribute dev_attr_picture = {
	.attr ={
		.name = "picture",
		.mode = S_IWUGO,
	},
	.store = picture_store,
};

static struct device_attribute dev_attr_pi_cam = {
	.attr ={
		.name = "pi_cam",
		.mode = S_IWUGO | S_IRUGO,
	},
	.show = pi_cam_show,
	.store = pi_cam_store,
};

static struct attribute *dev_attrs[] = {
	&dev_attr_picture.attr,
	&dev_attr_pi_cam.attr,
	NULL,
};

static struct attribute_group dev_group = {
//    .name = "",
    .attrs = dev_attrs,
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Function definitions

// Save Clean Up function
static void CleanUp(struct vio_state* pState)
{
	struct xilinx_vdma_config vdma_cfg = {
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
		
	// Release Buffer
	if(pState->pBuffer)
	{
		dma_free_coherent(pState->pDev, 1280*720*sizeof(u32),  pState->pBuffer, pState->dma_addr);
	}
	
	// Release vdma channels
	if(pState->pVMDA_out)
	{
		// Reset VDMA
		xilinx_vdma_channel_set_config(pState->pVMDA_out, &vdma_cfg);
		
		dma_release_channel(pState->pVMDA_out);
	}
	if(pState->pVMDA_in)
	{
		// Reset VDMA
		xilinx_vdma_channel_set_config(pState->pVMDA_in, &vdma_cfg);
		
		dma_release_channel(pState->pVMDA_in);
	}
}

//-------------------------------------------------------------------------------------------------
// Probe 
static int vio_probe(struct platform_device *pdev)
{
	struct vio_state* pState;
	int ret;

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
	
	WriteBuffer(pState);
	
	// Save device
	pState->pDev = &pdev->dev;

	//.............................................................................................
	// Get Simple Test Pattern Generator
	pState->pStpg = stpg_get(pdev->dev.of_node);
	if(pState->pStpg != NULL && pState->pStpg != ERR_PTR(-EINVAL) && pState->pStpg != ERR_PTR(-EPROBE_DEFER))
	{
		// Set Resolution
		stpg_set_resolution(pState->pStpg, 1280, 720);
		
		// Start Generator
		stpg_start(pState->pStpg);
	}
	else
	{
		// No stpg available
		pState->pStpg = 0;
	}
	
	//.............................................................................................
	// Get vtc device
	pState->pVtc = xvtc_of_get(pdev->dev.of_node);
	if(pState->pVtc == NULL || pState->pVtc == ERR_PTR(-EINVAL) || pState->pVtc == ERR_PTR(-EPROBE_DEFER))
	{
		CleanUp(pState);
		dev_err(&pdev->dev, "Failed to get vtc device (%d,%d,%d)", (int)pState->pVtc, EINVAL, EPROBE_DEFER);
		return -ENODEV;
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
	
	//.............................................................................................
	// Get VDMA channels
	pState->pVMDA_out = dma_request_slave_channel(&pdev->dev, "vdma_out");
	if(!pState->pVMDA_out)
	{
		dev_err(&pdev->dev, "No Tx VDMA channel\n");
		CleanUp(pState);
		return -ENODEV ;
	}
	
	pState->pVMDA_in = dma_request_slave_channel(&pdev->dev, "vdma_in");
	if(!pState->pVMDA_in)
	{
		dev_err(&pdev->dev, "No Rx VDMA channel\n");
		CleanUp(pState);
		return -ENODEV ;
	}
	
	// Configure VDMAs
	SetTxVDMAConfig(pState);
	SetRxVDMAConfig(pState);
	
	// Setup VDMA channels
	if(!SetTxVDMASetup(pState) || !SetRxVDMASetup(pState))
	{
		dev_err(&pdev->dev, "Setup VDMA channels failed\n");	
		CleanUp(pState);
		return -ENODEV;
	}
	
	StartVDMA(pState->pVMDA_out);
	
	//.............................................................................................
	// Get Camera I2C Interface
	pState->cam.pCam = pi_cam_get(pdev->dev.of_node);
	if(pState->cam.pCam == NULL || pState->cam.pCam == ERR_PTR(-EINVAL) || pState->cam.pCam == ERR_PTR(-EPROBE_DEFER))
	{
		CleanUp(pState);
		dev_err(&pdev->dev, "Failed to get pi-cam I2C device");
		return -ENODEV;		
	}
	
	// Reset Camera
	pi_cam_stop(pState->cam.pCam);
	pState->cam.bEnabled = false;
	
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
	
	// stop Camera
	pi_cam_stop(pState->cam.pCam);
	
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
