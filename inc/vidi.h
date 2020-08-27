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

#ifndef DISABLE_LIBAV
#include <unistd.h>
#include "libavcodec/avcodec.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#define INBUF_SIZE 4096
#endif

typedef enum {
	VIDI_V4L2 = 0,
	VIDI_LIBAV = 1,
} vidi_src_t;

typedef struct {
	int width, height;
	int frames_per_sec;
	int pixel_format;
	const char* path;
	vidi_src_t src_type;
	struct {
		int fd;
		int last_width, last_height;
		struct {
			void* frame[10];
			size_t size[10];
			struct v4l2_buffer info[10];
			size_t count;
		} buffer;

#ifndef DISABLE_LIBAV
		// https://libav.org/documentation/doxygen/master/decode_video_8c-example.html
		struct {
			AVCodecContext* dec_ctx;
			AVCodec *codec;
			AVCodecParserContext* parser;
			AVFrame* frame;
			AVPacket* pkt;
		} av;
#endif

	} sys;
} vidi_cfg_t;

typedef union {
	struct { uint8_t r, g, b; };
	uint8_t v[3];
} vidi_rgb_t;

typedef union {
	struct { 
		uint8_t y, uv;
	};
	uint8_t v[2];	
} vidi_yuyv_t;


static int vidi_request_frame(vidi_cfg_t* cfg)
{
	cfg->sys.buffer.info[0].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cfg->sys.buffer.info[0].memory = V4L2_MEMORY_MMAP;

	return ioctl(cfg->sys.fd, VIDIOC_QBUF, &cfg->sys.buffer.info[0]);
}


static void* vidi_wait_frame(vidi_cfg_t* cfg)
{
	uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE] = {};

	switch(cfg->src_type)
	{
		case VIDI_V4L2:
		{
			if (0 == ioctl(cfg->sys.fd, VIDIOC_DQBUF, &cfg->sys.buffer.info[0]))
			{
				return cfg->sys.buffer.frame[0];
			}

			return NULL;
		}
		case VIDI_LIBAV:
		{
			while (1)
			{
				/* read raw data from the input file */
				ssize_t data_size = read(cfg->sys.fd, inbuf, INBUF_SIZE);
				if (!data_size) { break; }
				/* use the parser to split the data into frames */
				uint8_t* data = inbuf;
				while (data_size > 0)
				{
					int ret = av_parser_parse2(
						cfg->sys.av.parser,
						cfg->sys.av.dec_ctx,
						&cfg->sys.av.pkt->data,
						&cfg->sys.av.pkt->size,
						data,
						data_size,
						AV_NOPTS_VALUE,
						AV_NOPTS_VALUE,
						0
					);

					if (ret < 0)
					{
						fprintf(stderr, "Error while parsing\n");
						exit(1);
					}

					data      += ret;
					data_size -= ret;

					if (cfg->sys.av.pkt->size)
					{
						int ret;
						ret = avcodec_send_packet(cfg->sys.av.dec_ctx, cfg->sys.av.pkt);
						if (ret < 0)
						{
							fprintf(stderr, "Error sending a packet for decoding\n");
							exit(1);
						}
						while (ret >= 0)
						{
							ret = avcodec_receive_frame(cfg->sys.av.dec_ctx, cfg->sys.av.frame);
							if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
							{
								break;
							}
							else if (ret < 0)
							{
								fprintf(stderr, "Error during decoding\n");
								exit(1);
							}
							
							/* the picture is allocated by the decoder. no need to
							   free it */
							// snprintf(buf, sizeof(buf), filename, cfg->sys.av.dec_ctx->frame_number);
							// pgm_save(frame->data[0], frame->linesize[0],
							// 		 frame->width, frame->height, buf);

							cfg->width = cfg->sys.av.frame->width;
							cfg->height = cfg->sys.av.frame->height;

							printf("frame %3d format: %d (%dx%d)\n", cfg->sys.av.dec_ctx->frame_number, cfg->sys.av.frame->format == AV_PIX_FMT_YUV420P, cfg->width, cfg->height);
							fflush(stdout);

							return cfg->sys.av.frame->data[0]; // return frame
						}
					}
				}
			}
		}
	}

	return NULL;
}


static size_t vidi_row_bytes(vidi_cfg_t* cfg)
{
	switch(cfg->src_type)
	{
		case VIDI_V4L2:
			return cfg->sys.buffer.size[0] / cfg->height;
		case VIDI_LIBAV:
			return cfg->sys.av.frame->linesize[0];
	}

	return 0;
}


static int _vidi_check_src(vidi_cfg_t* cfg)
{
		struct v4l2_capability cap;

		int res = ioctl(cfg->sys.fd, VIDIOC_QUERYCAP, &cap);
		if(0 == res)
		{
			if((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
			{
				return VIDI_V4L2;
			}
		}

		// fallback to decoding a video file
		return VIDI_LIBAV;
}


static int _vidi_v4l2_cfg(vidi_cfg_t* cfg, int fd)
{

	{ // do configuration
		struct v4l2_format format;

		// if no format is selected, use rgb
		if (0 == cfg->pixel_format) { cfg->pixel_format = V4L2_PIX_FMT_RGB24; }

		format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		format.fmt.pix.pixelformat = cfg->pixel_format;
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
	const size_t buf_count = 1;
	struct v4l2_requestbuffers bufrequest = {};
	bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufrequest.memory = V4L2_MEMORY_MMAP;
	bufrequest.count = cfg->sys.buffer.count = buf_count;

	if(ioctl(fd, VIDIOC_REQBUFS, &bufrequest) < 0)
	{
		return -7; // request buffer count can't be provided
	}


	if(bufrequest.count < buf_count)
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

			int res = ioctl(fd, VIDIOC_QBUF, &bufferinfo);
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
}

#ifndef DISABLE_LIBAV
static int _vidi_libav_cfg(vidi_cfg_t* cfg, int fd)
{
	uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE] = {};

	avcodec_register_all();
	cfg->sys.av.pkt = av_packet_alloc();
	if (!cfg->sys.av.pkt) { return -1; }
	/* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
	memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
	/* find the MPEG-1 video decoder */
	AVCodec* codec = cfg->sys.av.codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
	if (!codec)
	{
		fprintf(stderr, "codec not found\n");
		return -2;
	}
	AVCodecParserContext* parser = cfg->sys.av.parser = av_parser_init(cfg->sys.av.codec->id);
	if (!parser)
	{
		fprintf(stderr, "parser not found\n");
		return -3;
	}
	AVCodecContext *c = cfg->sys.av.dec_ctx = avcodec_alloc_context3(codec);
	AVFrame* picture = cfg->sys.av.frame = av_frame_alloc();
	/* For some codecs, such as msmpeg4 and mpeg4, width and height
	   MUST be initialized there because this information is not
	   available in the bitstream. */
	/* open it */
	if (avcodec_open2(c, codec, NULL) < 0)
	{
		fprintf(stderr, "could not open codec\n");
		return -4;
	}

	return 0;
}
#else
static int _vidi_libav_cfg(vidi_cfg_t* cfg, int fd) { return -100; }	
#endif


/**
 * @brief      { function_description }
 *
 * @param      settings  The settings
 *
 * @return     { description_of_the_return_value }
 */
static int vidi_config(vidi_cfg_t* cfg)
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
	}

	switch (cfg->src_type = _vidi_check_src(cfg))
	{
		case VIDI_V4L2:
			return _vidi_v4l2_cfg(cfg, fd);
		case VIDI_LIBAV:
			return _vidi_libav_cfg(cfg, fd);
		default:
			return -101;
	}
}

static void vidi_bias(size_t r, size_t c, const vidi_rgb_t from[r][c], vidi_rgb_t to[r][c], int bias)
{
	for(int ri = r; ri--;)
	for(int ci = c; ci--;)
	{
		to[ri][ci].r = from[ri][ci].r + bias;
		to[ri][ci].g = from[ri][ci].g + bias;
		to[ri][ci].b = from[ri][ci].b + bias;
	}
}

static void vidi_yuyv_to_rgb(size_t r, size_t c, const vidi_yuyv_t from[r][c], vidi_rgb_t to[r][c])
{
#define B_CLAMP(x) ((x) > 255 ? 255 : ((x) < 0 ? 0 : (x)))

	uint8_t u = from[0][0].uv, v = from[0][1].uv;

	for(int ri = r; ri--;)
	for(int ci = c; ci--;)
	{
		int i = ri * c + ci;
		int j = ri * (c >> 1) + (ci >> 1);

		uint8_t v = ci & 0x1 ? from[ri][ci - 1].uv : from[ri][ci].uv;
		uint8_t u = ci & 0x1 ? from[ri][ci].uv : from[ri][ci + 1].uv;
		uint8_t luma = from[ri][ci].y;

		to[ri][ci].r = B_CLAMP(luma + 1.14 * (u - 128));
		to[ri][ci].g = B_CLAMP(luma - 0.395 * (v - 128) - (0.581 * (u - 128)));
		to[ri][ci].b = B_CLAMP(luma + 2.033 * (v - 128));
	}
#undef B_CLAMP
}

#endif
