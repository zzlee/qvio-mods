#ifndef __QVIO_VIDEO_H__
#define __QVIO_VIDEO_H__

#include "queue.h"
#include "device.h"
#include "umods.h"

#include <media/v4l2-device.h>
#include <linux/videodev2.h>

struct qvio_video {
	struct kref ref;

	struct qvio_device* qdev;

	// v4l2
	struct qvio_queue queue;
	struct mutex device_mutex;
	struct v4l2_device v4l2_dev;
	struct video_device *vdev;
	int vfl_dir;
	char card_name[32];
	char bus_info[32];
	u32 device_caps;
	enum v4l2_buf_type buffer_type;
	int halign, valign;
	struct v4l2_format current_format;
	unsigned int current_inout;
	struct v4l2_streamparm current_parm;

	struct qvio_umods umods;
};

struct qvio_video* qvio_video_new(void);
struct qvio_video* qvio_video_get(struct qvio_video* self);
void qvio_video_put(struct qvio_video* self);

int qvio_video_start(struct qvio_video* self);
void qvio_video_stop(struct qvio_video* self);

int qvio_video_enum_fmt(struct qvio_video* self, struct v4l2_fmtdesc *format);
int qvio_video_g_fmt(struct qvio_video* self, struct v4l2_format *format);
int qvio_video_s_fmt(struct qvio_video* self, struct v4l2_format *format);
int qvio_video_try_fmt(struct qvio_video* self, struct v4l2_format *format);
int qvio_video_enum_input(struct qvio_video* self, struct v4l2_input *input);
int qvio_video_g_input(struct qvio_video* self, unsigned int *input);
int qvio_video_s_input(struct qvio_video* self, unsigned int input);
int qvio_video_enum_output(struct qvio_video* self, struct v4l2_output *output);
int qvio_video_g_output(struct qvio_video* self, unsigned int *output);
int qvio_video_s_output(struct qvio_video* self, unsigned int output);
int qvio_video_g_parm(struct qvio_video* self, struct v4l2_streamparm *param);
int qvio_video_s_parm(struct qvio_video* self, struct v4l2_streamparm *param);
int qvio_video_enum_framesizes(struct qvio_video* self, struct v4l2_frmsizeenum *frame_sizes);
int qvio_video_enum_frameintervals(struct qvio_video* self, struct v4l2_frmivalenum *frame_intervals);

#endif // __QVIO_VIDEO_H__