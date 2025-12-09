#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <poll.h>
#include <stdlib.h>

#define SUPPORT_FMT			V4L2_PIX_FMT_MJPEG
#define DESIRED_WIDTH		1280
#define DESIRED_HEIGHT		768
#define DESIRED_BUFFER_CNT	32

struct video_buffer {
	void	*memap;
	int		len;
};

int running = 1;
void sigint_handler(int sig) {
	printf("\nReceived Ctrl+C signal, exiting gracefully...\n");
	running = 0;
}



static void print_usage(const char *app)
{
	printf("%s <video path>\n", app);
	printf("ex: %s /dev/video0\n", app);
}

int main(int argc, char **argv)
{
	int ret = -1;
	int fd = -1;
	struct v4l2_fmtdesc fmtdesc;
	struct v4l2_capability capability;
	struct v4l2_frmsizeenum frmsizeenum;
	struct v4l2_format	format;
	struct v4l2_requestbuffers requestbuffers;
	struct v4l2_buffer buffer; 
	int index = 0;
	int frame_index = 0;
	int found_support_fmt = 0;
	int buffer_cnt = 0;
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int stream_started = 0;
	struct video_buffer *video_buffer = NULL;
	struct pollfd pollfd[1];
	const char *img_file_name = "image_%04d.jpg";
	char file_name[64] = {0};
	int file_idx = 0;
	int fd_imag = 0;

	if (argc != 2) {
		print_usage(argv[0]);
		goto done;
	}

	// Register signal handler for Ctrl+C
	signal(SIGINT, sigint_handler);

	fd = open(argv[1], O_RDWR);

	if (fd < 0) {
		printf("Failed to open %s\n", argv[1]);
		goto done;
	}

	if (ioctl(fd, VIDIOC_QUERYCAP, &capability) != 0) {
		printf("Failed to query capability\n");
		goto done;
	}

	if (!(capability.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		printf("%s is not a video capture device\n", argv[1]);
		goto done;
	}

	if (!(capability.capabilities & V4L2_CAP_STREAMING)) {
		printf("%s doesn't support streaming.\n", argv[1]);
		goto done;
	}

	while (1) {
		fmtdesc.index = index;
		fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		
		if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
			
			if (fmtdesc.pixelformat == SUPPORT_FMT) {
				found_support_fmt = 1;
			}

			frame_index = 0;
			while (1) {
				memset(&frmsizeenum, 0, sizeof(struct v4l2_frmsizeenum));
				frmsizeenum.index = frame_index;
				frmsizeenum.pixel_format = fmtdesc.pixelformat;

				if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) == 0) {
					printf("Format %s, index:%d, %ux%u\n", (char *)fmtdesc.description, 
						frame_index, frmsizeenum.discrete.width, frmsizeenum.discrete.height);
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
	
	if (!found_support_fmt) {
		printf("Cannot find MJPG format in this uvc camera.\n");
		goto done;
	}

	printf("Found supported format.\n");

	memset(&format, 0, sizeof(struct v4l2_format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width = DESIRED_WIDTH;
	format.fmt.pix.height = DESIRED_HEIGHT;
	format.fmt.pix.pixelformat = SUPPORT_FMT;
	format.fmt.pix.field = V4L2_FIELD_ANY;
	
	if (ioctl(fd, VIDIOC_S_FMT, &format) == 0) {
		printf("Set stream format to %u X %u\n", format.fmt.pix.width, format.fmt.pix.height);
	} else {
		printf("Failed to set stream format.\n");
		goto done;
	}

	//Reguest buffer
	memset(&requestbuffers, 0, sizeof(struct v4l2_requestbuffers));
	requestbuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	requestbuffers.count = DESIRED_BUFFER_CNT;
	requestbuffers.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_REQBUFS, &requestbuffers) != 0) {
		printf("Failed to reguest buffer\n.");
		goto done;
	} else if (requestbuffers.count == 0) {
		printf("No buffer is requested.\n");
		goto done;
	}

	printf("Buffer %d requested.\n", requestbuffers.count);

	buffer_cnt = requestbuffers.count;
	video_buffer = (struct video_buffer *) malloc(buffer_cnt * sizeof(struct video_buffer));
	if (!video_buffer) {
		printf("Failed to alloc video_buffer\n");
		goto done;
	}
	
	for (int i = 0; i < buffer_cnt; i++)
		video_buffer[i].memap = MAP_FAILED;

	for (int i = 0; i < buffer_cnt; i++) {
		buffer.index = i;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) == 0) {
			
			video_buffer[i].memap = mmap(0, buffer.length, 
				PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset);
			
			if (video_buffer[i].memap == MAP_FAILED) {
				printf("Failed to do mmap\n");
				goto done;
			}

			video_buffer[i].len = buffer.length;

		} else {
			printf("Failed to query buffer, %d\n", i);
			goto done;
		}
	}

	printf("Map %d buffer OK\n", buffer_cnt);

	for (int i = 0; i < buffer_cnt; i++) {
		memset(&buffer, 0, sizeof(struct v4l2_buffer));
		buffer.index = i;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (ioctl(fd, VIDIOC_QBUF, &buffer) != 0) {
			printf("Failed to queue buffer\n");
			goto done;
		}
	}

	printf("Queue buffer ok.\n");

	if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
		printf("Failed to start steam.\n");
		goto done;
	}

	stream_started = 1;
	printf("Start stream ok.\n");

	while (running) {
		memset(pollfd, 0 , sizeof(pollfd));
		pollfd[0].fd = fd;
		pollfd[0].events = POLLIN;
		
		if (poll(pollfd, 1, 1000) == 1) {
			memset(&buffer, 0, sizeof(struct v4l2_buffer));
			buffer.memory = V4L2_MEMORY_MMAP;
			buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (ioctl(fd, VIDIOC_DQBUF, &buffer) != 0) {
				printf("Failed to dqueue buffer\n");
				goto done;
			}

			sprintf(file_name, img_file_name, file_idx++);
			if ((fd_imag = open(file_name, O_RDWR | O_CREAT, 0666)) >= 0) {
				write(fd_imag, video_buffer[buffer.index].memap,
						buffer.bytesused);

				printf("file %s saved.\n",file_name);
				close(fd_imag);
			}
			
			if (ioctl(fd, VIDIOC_QBUF, &buffer) != 0) {
				printf("Failed to queue buffer\n");
				goto done;
			}
		}
	}

	printf("Stopping stream.\n");

done:
	
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (stream_started && ioctl(fd, VIDIOC_STREAMOFF, &type) == 0) {
		printf("Stop stream ok.\n");
	} else {
		printf("Stop stream failed or video is not streaming.\n");
	}

	if (buffer_cnt && video_buffer != NULL) {
		for (int i = 0; i < buffer_cnt; i++) {
			if (video_buffer[i].memap != MAP_FAILED) {
				munmap(video_buffer[i].memap, video_buffer[i].len);
			}
		}

		free(video_buffer);
		video_buffer = NULL;
	}

	if (fd >= 0) {
		close(fd);
	}

	return ret;
}