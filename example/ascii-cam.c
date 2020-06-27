#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "vidi.h"

typedef union {
	struct {
		uint8_t r, g, b;
	};
	uint8_t v[3];
} rgb_t;

int main (int argc, const char* argv[])
{
	// define a configuration object for a camera, here
	// you can request frame size, pixel format, frame rate
	// and the camera which you wish to use.
	vidi_cfg_t cam = {
		.width = 640,
		.height = 480,
		.frames_per_sec = 60,
		.path = argv[1]
	};

	// vidi_config is used to open and configure the camera
	// device, but can also be used to reconfigure an
	// existing vidi_cfg_t camera instance.
	assert(0 == vidi_config(&cam));
		
	rgb_t frame[480][640] = {};
	int rows_drawn = 0;

	while (1)
	{
		// request the camera to capture a frame
		vidi_request_frame(&cam);

		// this function blocks until a frame pointer is returned
		uint8_t* raw_frame = vidi_wait_frame(&cam);
		size_t row_size = vidi_row_bytes(&cam);

		for (int r = 0; r < 480; r++)
		{
			memcpy(frame[r], raw_frame + (r * row_size), row_size);
		}

		{
			static char move_up[16] = {};
			sprintf(move_up, "\033[%dA", rows_drawn);
			fprintf(stderr, "%s", move_up);
			rows_drawn = 0;
		}

		const char spectrum[] = "  .,':;|[{+*x88";

		for (int r = 0; r < 480; r += 15)
		{
			for (int c = 0; c < 640; c += 5)
			{
				int grey = (frame[r][c].r + frame[r][c].g + frame[r][c].b) / 3;
				putc(spectrum[grey / 18], stderr);
			}
			putc('\n', stderr);
			rows_drawn += 1;	
		}

	}

	return 0;
}
