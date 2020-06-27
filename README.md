# vidi.h
vidi.h is a single header-file library that simplifies integration of V4L2 into C/C++ projects.
![a vidi example](https://raw.githubusercontent.com/mrpossoms/vidi.h/master/.wave.gif)

## Usage
```C

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

	// request the camera to capture a frame
	vidi_request_frame(&cam);

	// Do something while you wait...

	// this function blocks until a frame pointer is returned
	void* frame = vidi_wait_frame(&cam);

```
