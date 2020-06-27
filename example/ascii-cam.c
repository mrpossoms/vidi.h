#include <assert.h>
#include "vidi.h"

int main (int argc, const char* argv[])
{
	vidi_cfg_t cam = {
		.width = 640,
		.height = 480,
		.frames_per_sec = 60,
		.path = argv[1]
	};

	assert(0 == vidi_config(&cam));
		
	return 0;
}
