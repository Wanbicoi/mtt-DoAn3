#include <iostream>
#include <asio.hpp>
#include <functional>
#include "types.h"
#include "client.h"
using asio::ip::tcp;

asio::io_context io_context;
tcp::socket screen_socket(io_context);
tcp::socket control_socket(io_context);

ScreenClient::ScreenClient() {

}

void ScreenClient::getData(void *data, int size) {
	asio::error_code error;
	asio::read(screen_socket, asio::buffer(data, size), error);
}

void ScreenClient::handleRead(std::error_code error) {
	if (!error) {
		asio::error_code ignored_error;
		getData(&mouse_x, sizeof(int));
		getData(&mouse_y, sizeof(int));
		int mouse_changed_tmp = 0;
		getData(&mouse_changed_tmp, sizeof(int));
		if (mouse_changed_tmp) {
			int old_mouse_size = mouse_width * mouse_height * 4;
			getData(&mouse_width, sizeof(int));
			getData(&mouse_height, sizeof(int));
			getData(&mouse_center_x, sizeof(int));
			getData(&mouse_center_y, sizeof(int));
			int mouse_size = 0;
			getData(&mouse_size, sizeof(int));
			if (old_mouse_size != mouse_size)
				mouse_data = (unsigned char*)realloc(mouse_data, mouse_size);
			getData(mouse_data, mouse_size);
			mouse_changed = 1;
		}
		int screen_size;
		getData(&screen_size, sizeof(int));
		getData(screen_data, screen_size);
		screen_changed = 1;
		int dummy;
		asio::async_read(screen_socket, asio::buffer(&dummy, 0),
			std::bind(&ScreenClient::handleRead, this, std::placeholders::_1 /*error*/));
	}

}

void ScreenClient::connect(const char *address) {
	try {
		std::string raw_ip_address(address);
		tcp::endpoint ep(asio::ip::address::from_string(raw_ip_address), SOCKET_SCREEN_PORT);
		screen_socket = tcp::socket(io_context, ep.protocol());

		screen_socket.connect(ep);
		connected = 1;
	}
	catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
		connected = 0;
	}
}

bool ScreenClient::isConnected() {
	return connected;
}

void ScreenClient::init() {
	getData(&screen_info, sizeof(ScreenInfo));

	screen_data = (unsigned char*)malloc(getWidth() * getHeight() * 4);
	mouse_data = (unsigned char*)malloc(32 * 32 * 4);

	int dummy;
	asio::async_read(screen_socket, asio::buffer(&dummy, 0),
		std::bind(&ScreenClient::handleRead, this, std::placeholders::_1 /*error*/));
}

int ScreenClient::getWidth() {
	return screen_info.width;
}

int ScreenClient::getHeight() {
	return screen_info.height;
}

bool ScreenClient::isFrameChanged() {
	bool changed = screen_changed;
	screen_changed = 0;
	return changed;
}

bool ScreenClient::isMouseImgChanged() {
	bool changed = mouse_changed;
	mouse_changed = 0;
	return changed;
}

unsigned char* ScreenClient::getScreenData() {
	return screen_data;
}

void ScreenClient::getMouseInfo(int *x, int *y) {
	*x = mouse_x - mouse_center_x;
	*y = mouse_y - mouse_center_y;
}

unsigned char* ScreenClient::getMouseData(int *width, int *height) {
	*width = mouse_width;
	*height = mouse_height;
	return mouse_data;
}

ScreenClient::~ScreenClient() {
	free(screen_data);
	free(mouse_data);
	//screen_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
	//screen_socket.close();
}

//---------------------------------------------------------------

ControlClient::ControlClient() {

}

void ControlClient::getData(void *data, int size) {
	asio::error_code error;
	asio::read(control_socket, asio::buffer(data, size), error);
}

std::string ControlClient::getString() {
	asio::error_code error;
	std::string res;
	int size;
	asio::read(control_socket, asio::buffer(&size, sizeof(int)), error);
	res.resize(size);
	asio::read(control_socket, asio::buffer(res, size), error);
	return res;
}

void ControlClient::connect(const char *address) {
	try {
		std::string raw_ip_address(address);
		tcp::endpoint ep(asio::ip::address::from_string(raw_ip_address), SOCKET_CONTROL_PORT);
		control_socket = tcp::socket(io_context, ep.protocol());

		control_socket.connect(ep);
		connected = 1;
	}
	catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
		connected = 0;
	}
}

bool ControlClient::isConnected() {
	return connected;
}

void ControlSocketClose() {
	control_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
	control_socket.close();
}

void ControlClient::sendControl(OperationCode opcode, int data, void *raw_data) {
	ControlBuffer buf = {opcode, data};
	asio::error_code error;
	asio::write(control_socket, asio::buffer(&buf, sizeof(buf)), error);
	if (raw_data) {
		asio::write(control_socket, asio::buffer(raw_data, data), error);
	}
}

std::vector<ProcessInfo> ControlClient::getProcesses() {
	sendControl(PROCESS_LIST);
	ControlBuffer buf;
	getData(&buf, sizeof(buf));
	if (buf.opcode != PROCESS_LIST) return {};
	std::vector<ProcessInfo> processes(buf.data);
	for (auto &process: processes) {
		getData(&process.pid, sizeof(int));
		process.name = getString();
		getData(&process.type, sizeof(char));
	}
	return processes;
}

ControlClient::~ControlClient() {
	//control_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
	//control_socket.close();
}

// template <typename T>
// void ControlClient::sendType(T obj) {
// 	asio::error_code error;
// 	asio::write(control_socket, asio::buffer(&obj, sizeof(obj)), error);
// }

// template void ControlSocketSendType<int>(int);
// template void ControlSocketSendType<float>(float);


void IoContextRun() {
	io_context.run();
}

void IoContextStop() {
	io_context.stop();
}