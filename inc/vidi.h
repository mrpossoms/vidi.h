#ifndef __VIDI_H__
#define __VIDI_H__

#include <strings.h>

#include <fcntl.h>
#include <stddef.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>
#include <inttypes.h>

typedef struct {
	int width, height;
	int frames_per_sec;
	const char* path;

	struct {
		int fd;
		int last_width, last_height;
		struct {
			void* frame[10];
			size_t size[10];
			struct v4l2_buffer info[10];
			size_t count;
		} buffer;
	} sys;
} vidi_cfg_t;


int vidi_request_frame(vidi_cfg_t* cfg)
{
	cfg->sys.buffer.info[0].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cfg->sys.buffer.info[0].memory = V4L2_MEMORY_MMAP;

	return ioctl(cfg->sys.fd, VIDIOC_QBUF, &cfg->sys.buffer.info[0]);
}


void* vidi_wait_frame(vidi_cfg_t* cfg)
{
	if (0 == ioctl(cfg->sys.fd, VIDIOC_DQBUF, &cfg->sys.buffer.info[0]))
	{
		return cfg->sys.buffer.frame[0];
	}

	return NULL;
}

/**
 * @brief      { function_description }
 *
 * @param      settings  The settings
 *
 * @return     { description_of_the_return_value }
 */
int vidi_config(vidi_cfg_t* cfg)
{
	if (NULL == cfg) { return -1; }

	int fd = cfg->sys.fd;
	int res = 0;

	// If this is not an open fd do the opening and
	// setup needed.
	if (fd <= 0)
	{
		fd = cfg->sys.fd = open(cfg->path, O_RDWR);

		if(fd < 0)
		{
			return -2;
		}

		struct v4l2_capability cap;

		res = ioctl(fd, VIDIOC_QUERYCAP, &cap);
		if(res < 0)
		{
			return -3;
		}

		if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
		{
			return -4; // lacks video capability
		}		
	}

	{ // do configuration
		struct v4l2_format format;

		format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
		format.fmt.pix.width = cfg->width;
		format.fmt.pix.height = cfg->height;

		if(ioctl(fd, VIDIOC_S_FMT, &format) < 0)
		{
			return -5; // configuration failed
		}

		struct v4l2_streamparm parm = {};

		parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		parm.parm.capture.timeperframe.numerator = 1;
		parm.parm.capture.timeperframe.denominator = cfg->frames_per_sec;

		if(ioctl(fd, VIDIOC_S_PARM, &parm))
		{
			return -6; // framerate config failed
		}
	}

	// Inform v4l about the buffers we want to receive data through
	struct v4l2_requestbuffers bufrequest = {};
	bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufrequest.memory = V4L2_MEMORY_MMAP;
	bufrequest.count = cfg->sys.buffer.count = 2;

	if(ioctl(fd, VIDIOC_REQBUFS, &bufrequest) < 0)
	{
		return -7; // request buffer count can't be provided
	}


	if(bufrequest.count < 2)
	{
		return -8; // request buffer count insufficent
	}

	// Has the desired frame size changed?
	if (cfg->sys.last_width != cfg->width ||
		cfg->sys.last_height != cfg->height)
	{
		for (size_t i = cfg->sys.buffer.count; i--;)
		{
			munmap(cfg->sys.buffer.frame[i], cfg->sys.buffer.size[i]);
			cfg->sys.buffer.frame[i] = NULL;
			cfg->sys.buffer.size[i] = 0;
		}

		struct v4l2_buffer bufferinfo = {};
		for(int i = bufrequest.count; i--;)
		{
			bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			bufferinfo.memory = V4L2_MEMORY_MMAP;
			bufferinfo.index = i;

			if(ioctl(fd, VIDIOC_QUERYBUF, &bufferinfo) < 0)
			{
				return -9; // quert buffer failed
			}

			cfg->sys.buffer.frame[i] = mmap(
				NULL,
				cfg->sys.buffer.size[i] = bufferinfo.length,
				PROT_READ | PROT_WRITE,
				MAP_SHARED,
				fd,
				bufferinfo.m.offset
			);

			if(MAP_FAILED == cfg->sys.buffer.frame[i])
			{
				return -10; // mmap failed
			}

			bzero(cfg->sys.buffer.frame[i], cfg->sys.buffer.size[i]);

			res = ioctl(fd, VIDIOC_QBUF, &bufferinfo);
			// if (res) printf("cam_open(): VIDIOC_QBUF");

			cfg->sys.buffer.info[i] = bufferinfo;
		}
	}

	vidi_wait_frame(cfg);

	int type = cfg->sys.buffer.info[0].type;
	if(ioctl(fd, VIDIOC_STREAMON, &type) < 0)
	{
		return -11; // error starting streaming
	}

	return 0;
}

#endif
