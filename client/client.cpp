#include <iostream>
#include <asio.hpp>
#include <functional>
#include <thread>
#include <chrono>
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
		FrameBundle frBd;
		getData(&frBd, sizeof(FrameBundle));
		mouse_x = frBd.mouse_x;
		mouse_y = frBd.mouse_y;
		if (frBd.mouse_changed) {
			int old_mouse_size = mouse_width * mouse_height * 4;
			mouse_width = frBd.mouse_width;
			mouse_height = frBd.mouse_height;
			mouse_center_x = frBd.mouse_center_x;
			mouse_center_y = frBd.mouse_center_y;
			if (old_mouse_size != frBd.mouse_size)
				mouse_data = (unsigned char*)realloc(mouse_data, frBd.mouse_size);
			getData(mouse_data, frBd.mouse_size);
			mouse_changed = 1;
		}
		if (frBd.screen_changed) {
			keys_pressed.reserve(frBd.num_keys_pressed);
			unsigned char keys[frBd.num_keys_pressed];
			getData(keys, frBd.num_keys_pressed);
			for (int i = 0; i < frBd.num_keys_pressed; i++)
				keys_pressed.push_back(keys[i]);
			getData(screen_data, frBd.screen_size);
			screen_changed = 1;
		}
		asio::async_read(screen_socket, asio::buffer(&opcode, sizeof(OperationCode)),
			std::bind(&ScreenClient::handleRead, this, std::placeholders::_1 /*error*/));
	}

}

bool ScreenClient::connect(const char *address) {
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
	return connected;
}

void ScreenClient::disconnect() {
	connected = 0;
	screen_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
	screen_socket.close();
}

bool ScreenClient::isConnected() {
	return connected;
}

void ScreenClient::init() {
	getData(&screen_info, sizeof(ScreenInfo));

	screen_data = (unsigned char*)realloc(screen_data, getWidth() * getHeight() * 4);
	mouse_data = (unsigned char*)realloc(mouse_data, 32 * 32 * 4);

	asio::async_read(screen_socket, asio::buffer(&opcode, 0),
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

std::vector<unsigned char> ScreenClient::getKeys() {
	std::vector<unsigned char> keys = keys_pressed;
	keys_pressed.clear();
	return keys;
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
}

//---------------------------------------------------------------

ControlClient::ControlClient() {

}

void ControlClient::getData(void *data, int size) {
	asio::error_code error;
	asio::read(control_socket, asio::buffer(data, size), error);
}

void ControlClient::getString(std::string *str) {
	asio::error_code error;
	int size;
	asio::read(control_socket, asio::buffer(&size, sizeof(int)), error);
	str->resize(size);
	asio::read(control_socket, asio::buffer(*str, size), error);
}

void ControlClient::sendData(void *data, int size) {
	asio::error_code error;
	asio::write(control_socket, asio::buffer(data, size), error);
}

void ControlClient::sendString(std::string str) {
	asio::error_code error;
	int size = str.size();
	asio::write(control_socket, asio::buffer(&size, sizeof(int)), error);
	asio::write(control_socket, asio::buffer(str, size), error);
}

void ControlClient::sendControl(OperationCode opcode) {
	sendData(&opcode, sizeof(OperationCode));
}

bool ControlClient::connect(const char *address) {
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
	return connected;
}

void ControlClient::disconnect() {
	connected = 0;
	control_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
	control_socket.close();
}

bool ControlClient::isConnected() {
	return connected;
}

void ControlClient::suspendProcess(int pid) {
	sendControl(PROCESS_SUSPEND);
	sendData(&pid, sizeof(int));
}

void ControlClient::resumeProcess(int pid) {
	sendControl(PROCESS_RESUME);
	sendData(&pid, sizeof(int));
}

void ControlClient::terminateProcess(int pid) {
	sendControl(PROCESS_KILL);
	sendData(&pid, sizeof(int));
}

std::vector<ProcessInfo> ControlClient::getProcesses() {
	sendControl(PROCESS_LIST);
	int size;
	getData(&size, sizeof(int));
	std::vector<ProcessInfo> processes(size);
	for (auto &process: processes) {
		getData(&process.pid, sizeof(int));
		getString(&process.name);
		getData(&process.type, sizeof(char));
	}
	return processes;
}

std::string ControlClient::getDefaultLocation() {
	sendControl(FS_INIT);
	std::string res;
	getString(&res);
	return res;
}

std::vector<FileInfo> ControlClient::listDir(std::string path) {
	sendControl(FS_LIST);
	sendString(path);
	int size;
	getData(&size, sizeof(int));
	std::vector<FileInfo> files_list(size);
	for (auto &entry: files_list) {
		getString(&entry.name);
		getData(&entry.type, sizeof(char));
	}
	return files_list;
}

bool ControlClient::checkExist(std::string from, std::string to) {
	sendControl(FS_CHECK_EXIST);
	sendString(from);
	sendString(to);
	bool res = 1;
	getData(&res, sizeof(bool));
	return res;
}

void ControlClient::requestCopy(std::string from, std::string to, bool overwrite) {
	sendControl(FS_COPY);
	sendString(from);
	sendString(to);
	sendData(&overwrite, sizeof(bool));
}

void ControlClient::requestMove(std::string from, std::string to, bool overwrite) {
	sendControl(FS_MOVE);
	sendString(from);
	sendString(to);
	sendData(&overwrite, sizeof(bool));
}

void ControlClient::requestWrite(std::string path, bool overwrite, int size, void *data) {
	sendControl(FS_WRITE);
	sendString(path);
	sendData(&overwrite, sizeof(bool));
	sendData(&size, sizeof(int));
	sendData(data, size);
}

void ControlClient::requestDelete(std::string path) {
	sendControl(FS_DELETE);
	sendString(path);
}

// void ControlClient::sendDisconnect() {
// 	sendControl(CONTROL_DISCONNECT);
// }

ControlClient::~ControlClient() {

}

bool io_stop = 1;

void IoContextPoll() {
	while (1) {
		if (!io_stop)
			io_context.poll();
		else
			std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void IoContextRun() {
	io_stop = 0;
}

void IoContextStop() {
	io_stop = 1;
}