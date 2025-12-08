#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <string.h>
#include <unistd.h>

static void print_usage(const char *app)
{
	printf("%s <video path>\n", app);
	printf("ex: %s /dev/video0\n", app);
}

int main(int argc, char **argv)
{
	int ret = -1;
	int fd = -1;
	struct v4l2_fmtdesc fmt_desc;
	struct v4l2_capability cap;
	struct v4l2_frmsizeenum frm_size;
	int index = 0;
	int frame_index = 0;

	if (argc != 2) {
		print_usage(argv[0]);
		goto done;
	}

	fd = open(argv[1], O_RDWR);

	if (fd < 0) {
		printf("Failed to open %s\n", argv[1]);
		goto done;
	}

	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) != 0) {
		printf("Failed to query capability\n");
		goto done;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		printf("%s is not a video capture device\n", argv[1]);
		goto done;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		printf("%s doesn't support streaming.\n", argv[1]);
		goto done;
	}

	while (1) {
		fmt_desc.index = index;
		fmt_desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		
		if (ioctl(fd, VIDIOC_ENUM_FMT, &fmt_desc) == 0) {
			
			frame_index = 0;
			while (1) {
				memset(&frm_size, 0, sizeof(frm_size));
				frm_size.index = frame_index;
				frm_size.pixel_format = fmt_desc.pixelformat;

				if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frm_size) == 0) {
					printf("Format %s, index:%d, %ux%u\n", (char *)fmt_desc.description, 
						frame_index, frm_size.discrete.width, frm_size.discrete.height);
				} else {
					break;
				}

				frame_index ++;
			}

		} else {
			break;
		}
		index ++;
	}
	
done:
	if (fd >= 0) {
		close(fd);
	}

	return ret;
}