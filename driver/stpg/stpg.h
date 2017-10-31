#ifndef __JF_STPG_H__
#define __JF_STPG_H__

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver structures
struct stpg_device {
	struct device	*pDev;
	void __iomem 	*pIoMem;
	struct clk 		*pClk;

	
	struct list_head list;
};

#ifndef STPG_NO_EXPORT
extern struct stpg_device* stpg_get(struct device_node* np);
extern void stpg_put(struct stpg_device *pDevice);

extern int stpg_set_resolution(struct stpg_device *pDevice, unsigned width, unsigned height);
extern int stpg_start(struct stpg_device *pDevice);
#endif // STPG_NO_EXPORT

#endif //__JF_STPG_H__