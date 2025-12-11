#include <linux/module.h>
#include <linux/slab.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-vmalloc.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define DESIRED_WIDTH		800
#define DESIRED_HEIGHT		600
#define DESIRED_IMG_SIZE	(800 * 600 * 2)
#define DESIRED_BUFFER_NUM	8

static int virtual_video_querycap(struct file *file, void *fh,
		struct v4l2_capability *cap);

static int virtual_video_enum_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_fmtdesc *f);

static int virtual_video_g_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f);

static int virtual_video_s_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f);

static int virtual_video_queue_setup(struct vb2_queue *vq,
		unsigned int *nbuffers,
		unsigned int *nplanes, unsigned int sizes[], struct device *alloc_devs[]);

static void virtual_video_buf_queue(struct vb2_buffer *vb);

static int virtual_video_start_streaming(struct vb2_queue *vq, unsigned int count);

static void virtual_video_stop_streaming(struct vb2_queue *vq);

static const struct v4l2_ioctl_ops virtual_video_ioctl_ops = {
	.vidioc_querycap          = virtual_video_querycap,

	.vidioc_enum_fmt_vid_cap  = virtual_video_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap     = virtual_video_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = virtual_video_s_fmt_vid_cap,

	.vidioc_reqbufs           = vb2_ioctl_reqbufs,
	.vidioc_create_bufs       = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf       = vb2_ioctl_prepare_buf,
	.vidioc_querybuf          = vb2_ioctl_querybuf,
	.vidioc_qbuf              = vb2_ioctl_qbuf,
	.vidioc_dqbuf             = vb2_ioctl_dqbuf,

	.vidioc_streamon          = vb2_ioctl_streamon,
	.vidioc_streamoff         = vb2_ioctl_streamoff,

	.vidioc_subscribe_event   = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
	.vidioc_log_status        = v4l2_ctrl_log_status,
};

static const struct v4l2_file_operations virtual_video_fops = {
	.owner                    = THIS_MODULE,
	.open                     = v4l2_fh_open,
	.release                  = vb2_fop_release,
	.read                     = vb2_fop_read,
	.poll                     = vb2_fop_poll,
	.mmap                     = vb2_fop_mmap,
	.unlocked_ioctl           = video_ioctl2,
};

static const struct vb2_ops virtual_video_vb2_ops = {
	.queue_setup            = virtual_video_queue_setup,
	.buf_queue              = virtual_video_buf_queue,
	.start_streaming        = virtual_video_start_streaming,
	.stop_streaming         = virtual_video_stop_streaming,
	.wait_prepare           = vb2_ops_wait_prepare,
	.wait_finish            = vb2_ops_wait_finish,
};

static struct video_device g_video_device = {
	.name = "virtual video",
	.release = video_device_release_empty,
	.fops = &virtual_video_fops,
	.ioctl_ops = &virtual_video_ioctl_ops,
};

struct virtual_video_frame_buf {
	/* common v4l buffer stuff -- must be first */
	struct vb2_v4l2_buffer vb;
	struct list_head list;
};

static struct v4l2_device g_v4l2_device;
static struct vb2_queue g_vb2_queue;

static struct mutex v4l2_lock; 
static struct mutex vb_queue_lock;

static spinlock_t		g_queued_bufs_lock;
static struct list_head g_queued_bufs;

static struct timer_list g_timer;

extern unsigned char red[8230];
static unsigned int red_len = sizeof(red);

extern unsigned char green[8265];
static unsigned int green_len = sizeof(green);

extern unsigned char blue[8267];
static unsigned int blue_len = sizeof(blue);

static unsigned int g_color_index = 0;

static int virtual_video_querycap(struct file *file, void *fh,
		struct v4l2_capability *cap)
{
	strscpy(cap->driver, KBUILD_MODNAME, sizeof(cap->driver));
	strscpy(cap->card, "virtual video", sizeof(cap->card));
	
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int virtual_video_enum_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_fmtdesc *f)
{
	if (f->index >= 1)
		return -EINVAL;

	f->pixelformat = V4L2_PIX_FMT_MJPEG;

	return 0;
}

static int virtual_video_g_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f)
{
	f->fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	f->fmt.pix.sizeimage = DESIRED_IMG_SIZE;
	f->fmt.pix.width = DESIRED_WIDTH;
	f->fmt.pix.height = DESIRED_HEIGHT;
	return 0;
}

static int virtual_video_s_fmt_vid_cap(struct file *file, void *priv,
		struct v4l2_format *f)
{
	if (vb2_is_busy(&g_vb2_queue)) {
		return -EBUSY;
	}

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return -EINVAL;
	}

	if (f->fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
		return -EINVAL;
	}
	
	f->fmt.pix.width = DESIRED_WIDTH;
	f->fmt.pix.height = DESIRED_HEIGHT;
	f->fmt.pix.sizeimage = DESIRED_IMG_SIZE;
	return 0;
}

static int virtual_video_queue_setup(struct vb2_queue *vq,
		unsigned int *nbuffers,
		unsigned int *nplanes, unsigned int sizes[], struct device *alloc_devs[])
{
	unsigned int q_num_bufs = vb2_get_num_buffers(vq);

	/* Need at least 8 buffers */
	if (q_num_bufs + *nbuffers < DESIRED_BUFFER_NUM)
		*nbuffers = DESIRED_BUFFER_NUM - q_num_bufs;
	*nplanes = 1;
	
	sizes[0] = PAGE_ALIGN(DESIRED_IMG_SIZE);
	
	return 0;
}

static void virtual_video_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct virtual_video_frame_buf *buf =
			container_of(vbuf, struct virtual_video_frame_buf, vb);
	unsigned long flags;

	spin_lock_irqsave(&g_queued_bufs_lock, flags);
	list_add_tail(&buf->list, &g_queued_bufs);
	spin_unlock_irqrestore(&g_queued_bufs_lock, flags);
}

static struct virtual_video_frame_buf *virtual_video_get_next_fill_buf(void)
{
	unsigned long flags;
	struct virtual_video_frame_buf *buf = NULL;

	spin_lock_irqsave(&g_queued_bufs_lock, flags);
	if (list_empty(&g_queued_bufs))
		goto leave;

	buf = list_entry(g_queued_bufs.next,
			struct virtual_video_frame_buf, list);
	list_del(&buf->list);
leave:
	spin_unlock_irqrestore(&g_queued_bufs_lock, flags);
	return buf;
}

static void virtual_video_timer_callback(struct timer_list *t)
{
	struct virtual_video_frame_buf * buffer = virtual_video_get_next_fill_buf();
	void *ptr = NULL;
	int len = 0;
	unsigned char *jpg = NULL;

	if (buffer != NULL) {
		/* fill framebuffer */
		ptr = vb2_plane_vaddr(&buffer->vb.vb2_buf, 0);

		if (ptr == NULL) {
			vb2_buffer_done(&buffer->vb.vb2_buf, VB2_BUF_STATE_ERROR);
			goto err;
		}

		if (g_color_index < 60) {
			jpg = red;
			len = red_len;
		} else if (g_color_index < 120) {
			jpg = green;
			len = green_len;
		} else {
			jpg = blue;
			len = blue_len;
		}
		
		memcpy(ptr, jpg, len);
		vb2_set_plane_payload(&buffer->vb.vb2_buf, 0, len);
		vb2_buffer_done(&buffer->vb.vb2_buf, VB2_BUF_STATE_DONE);
	}

err:
	g_color_index++;

	if (g_color_index >= 180) {
		g_color_index = 0;
	}
	mod_timer(&g_timer, jiffies + HZ / 30);
}

static int virtual_video_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	g_color_index = 0;
	timer_setup(&g_timer, virtual_video_timer_callback, 0);
	mod_timer(&g_timer, jiffies + HZ / 30);

	return 0;
}

static void virtual_video_stop_streaming(struct vb2_queue *vq)
{
	struct virtual_video_frame_buf *buffer = virtual_video_get_next_fill_buf();
	
	del_timer_sync(&g_timer);

	while (buffer != NULL) {
		vb2_buffer_done(&buffer->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		buffer = virtual_video_get_next_fill_buf();
	}
}

static void virtual_video_release(struct v4l2_device *v)
{
	v4l2_device_unregister(v);
}


static int virtual_video_drv_init(void)
{
	int ret = 0;

	printk(KERN_INFO"In virtual_video_drv_init \n");
	
	mutex_init(&v4l2_lock);
	mutex_init(&vb_queue_lock);

	spin_lock_init(&g_queued_bufs_lock);
	INIT_LIST_HEAD(&g_queued_bufs);

	/* Init videobuf2 queue structure */
	g_vb2_queue.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
	g_vb2_queue.io_modes = VB2_MMAP | VB2_USERPTR | VB2_READ;
	g_vb2_queue.drv_priv = NULL;
	g_vb2_queue.buf_struct_size = sizeof(struct virtual_video_frame_buf);
	g_vb2_queue.ops = &virtual_video_vb2_ops;
	g_vb2_queue.mem_ops = &vb2_vmalloc_memops;
	g_vb2_queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	ret = vb2_queue_init(&g_vb2_queue);
	if (ret) {
		printk(KERN_INFO "Could not initialize vb2 queue\n");
		goto err;
	}	
	
	g_video_device.queue = &g_vb2_queue;
	g_video_device.queue->lock = &vb_queue_lock;
	
	g_v4l2_device.release = virtual_video_release;
	strcpy(g_v4l2_device.name, "virtual_v4l2");
	ret = v4l2_device_register(NULL, &g_v4l2_device);
	if (ret) {
		printk(KERN_INFO "Failed to register v4l2-device (%d)\n", ret);
		goto err;
	}

	g_video_device.v4l2_dev = &g_v4l2_device;
	g_video_device.lock = &v4l2_lock;
	g_video_device.device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING |
			      V4L2_CAP_READWRITE;

	ret = video_register_device(&g_video_device, VFL_TYPE_VIDEO, -1);
	if (ret) {
		printk(KERN_INFO "Failed to register as video device (%d)\n",
				ret);
		goto  err;
	}

err:
	return ret;
}

static void virtual_video_drv_exit(void)
{
	printk(KERN_INFO"In virtual_video_drv_exit \n");

	mutex_lock(&vb_queue_lock);
	mutex_lock(&v4l2_lock);
	
	v4l2_device_disconnect(&g_v4l2_device);
	video_unregister_device(&g_video_device);
	
	mutex_unlock(&v4l2_lock);
	mutex_unlock(&vb_queue_lock);

	v4l2_device_put(&g_v4l2_device);
}

module_init(virtual_video_drv_init);
module_exit(virtual_video_drv_exit);
MODULE_LICENSE("GPL");
