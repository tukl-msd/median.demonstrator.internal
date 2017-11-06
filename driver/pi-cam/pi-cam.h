#ifndef __JF_PI_CAM_H__
#define __JF_PI_CAM_H__

#include <linux/i2c.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// Driver structures

enum pi_cam_version {
	PI_CAM_1_3,
	PI_CAM_2_1
};

#ifndef PI_CAM_NO_EXPORT
extern struct i2c_client* pi_cam_get(struct device_node* np);
extern void pi_cam_put(struct i2c_client *pDevice);

extern bool pi_cam_start(struct i2c_client *pDevice);
extern bool pi_cam_stop(struct i2c_client *pDevice);
#endif // PI_CAM_NO_EXPORT

#endif //__JF_PI_CAM_H__