//#define UDP_SOCKET_PORT 5445
//#define TCP_SOCKET_PORT 4435
#pragma once
#include <string>

#define SOCKET_SCREEN_PORT 5343
#define SOCKET_CONTROL_PORT 4354

enum OperationCode : uint16_t {
	NONE = 0,
	PROCESS_LIST,
	PROCESS_SUSPEND,
	PROCESS_RESUME,
	PROCESS_KILL,
	KEYBOARD_DOWN,
	MOUSE_MOVE,
	MOUSE_LEFT_DOWN,
	MOUSE_LEFT_UP,
	MOUSE_MIDDLE_DOWN,
	MOUSE_MIDDLE_UP,
	MOUSE_RIGHT_DOWN,
	MOUSE_RIGHT_UP,
	MOUSE_WHEEL_V,
	MOUSE_WHEEL_H,
	FS_LIST,
	FS_COPY,
	FS_MOVE,
	FS_WRITE,
	FS_DELETE
};

struct ScreenInfo {
	int width;
	int height;
};

struct MouseImage {
	void *data;
	int width;
	int height;
	int size;
};

struct ScreenBuffer {
	void *screen;
	int screen_size;
	int mouse_x;
	int mouse_y;
	int mouse_changed;
	MouseImage mouse_image;
};

struct ControlBuffer {
	uint16_t opcode;
	int size;
};

struct ProcessInfo {
	int pid;
	std::string name;
	char type;
};