#ifndef __JF_PI_CAM_H__
#define __JF_PI_CAM_H__

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver structures
struct pi_cam_device {
	struct device	*pDev;
	
	struct list_head list;
};

enum pi_cam_version {
	PI_CAM_1_3,
	PI_CAM_2_1
};

#ifndef PI_CAM_NO_EXPORT
extern struct pi_cam_device* stpg_get(struct device_node* np);
extern void pi_cam_put(struct stpg_device *pDevice);

extern int pi_cam_set_resolution(struct stpg_device *pDevice, unsigned width, unsigned height);
extern int pi_cam_start(struct stpg_device *pDevice);
#endif // PI_CAM_NO_EXPORT

#endif //__JF_PI_CAM_H__