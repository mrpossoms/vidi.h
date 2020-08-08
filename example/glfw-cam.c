#include "vidi.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <GLFW/glfw3.h>


#define WIN_W (640)
#define WIN_H (480)

#define FRAME_W (640)
#define FRAME_H (480)


GLFWwindow* WIN;
GLuint FRAME_TEX;

static void setup_gl()
{
	glShadeModel(GL_SMOOTH);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
}


static void create_texture(GLuint* tex)
{
	glGenTextures(1, tex);
	glBindTexture(GL_TEXTURE_2D, *tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}


void frame_to_canon(float x_frame, float y_frame, float* x, float* y)
{
	*x = ((x_frame / (float)WIN_W) - 0.5) * 2;
	*y = ((y_frame / (float)WIN_H) - 0.5) * -2;
}


void frame_to_pix(float x_frame, float y_frame, int* x, int* y)
{
	*x = (x_frame / (float)WIN_W) * FRAME_W;
	*y = (y_frame / (float)WIN_H) * FRAME_H;
}


int main(int argc, char* argv[])
{
	// define a configuration object for a camera, here
	// you can request frame size, pixel format, frame rate
	// and the camera which you wish to use.
	vidi_cfg_t cam = {
		.width = FRAME_W,
		.height = FRAME_H,
		.frames_per_sec = 60,
		.path = argv[1],
		.pixel_format = V4L2_PIX_FMT_YUYV
	};

	// vidi_config is used to open and configure the camera
	// device, but can also be used to reconfigure an
	// existing vidi_cfg_t camera instance.
	assert(0 == vidi_config(&cam));


	if (!glfwInit())
	{
		return -1;
	}

	WIN = glfwCreateWindow(WIN_W, WIN_H, "glfw-cam", NULL, NULL);

	if (!WIN)
	{
		glfwTerminate();
		return -2;
	}

	glfwMakeContextCurrent(WIN);
	setup_gl();
	create_texture(&FRAME_TEX);

	vidi_rgb_t rgb[FRAME_W * FRAME_H] = {};

	int img_fd = 0;

	while(!glfwWindowShouldClose(WIN))
	{
		// request the camera to capture a frame
		vidi_request_frame(&cam);

		// this function blocks until a frame pointer is returned
		uint8_t* raw_frame = vidi_wait_frame(&cam);
		size_t row_size = vidi_row_bytes(&cam);
		
		// copy the raw frame into the YUYV frame
		vidi_yuyv_t yuyv_frame[FRAME_H][FRAME_W];
		for (int r = 0; r < FRAME_H; r++)
		{
			memcpy(yuyv_frame[r], raw_frame + (r * row_size), row_size);
		}

		vidi_rgb_t rgb_frame[FRAME_H][FRAME_W];
		vidi_yuyv_to_rgb(FRAME_H, FRAME_W, yuyv_frame, rgb_frame);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGB,
			FRAME_W,
			FRAME_H,
			0,
			GL_RGB,
			GL_UNSIGNED_BYTE,
			rgb_frame
		);

		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_TEXTURE_2D);

		glBegin(GL_QUADS);
			glTexCoord2f(1, 0);
			glVertex2f( 1,  1);

			glTexCoord2f(0, 0);
			glVertex2f(-1,  1);

			glTexCoord2f(0, 1);
			glVertex2f(-1, -1);

			glTexCoord2f(1, 1);
			glVertex2f( 1, -1);
		glEnd();

		glfwPollEvents();
		glfwSwapBuffers(WIN);
	}

	return 0;
}
