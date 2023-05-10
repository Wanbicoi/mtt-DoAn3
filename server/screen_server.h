#include <iostream>
#include <string>
#include <memory>
#include <functional>
#include <asio.hpp>
#include "define.h"
using asio::ip::tcp;

struct ScreenInfo {
	int width;
	int height;
};

struct ScreenBuffer {
	void *screen;
	int screen_size;
	int mouse_x;
	int mouse_y;
	int mouse_changed;
	void *mouse_img;
	int mouse_width;
	int mouse_height;
	int mouse_size;
};

class ScreenConnection : public std::enable_shared_from_this<ScreenConnection> {
public:
	typedef std::shared_ptr<ScreenConnection> pointer;

	static pointer create(asio::io_context& io_context, const ScreenInfo info, const std::function<ScreenBuffer()> &callback) {
		return pointer(new ScreenConnection(io_context, info, callback));
	}

	tcp::socket& socket() {
		return socket_;
	}

	void start() {
		ScreenBuffer buffer = callback_(); 
		asio::error_code ignored_error;
		asio::write(socket_, asio::buffer(&info_.width, sizeof(int)), ignored_error);
		asio::write(socket_, asio::buffer(&info_.height, sizeof(int)), ignored_error);
		asio::async_write(socket_, asio::buffer(buffer.screen, buffer.screen_size),
				std::bind(&ScreenConnection::handle_write, shared_from_this(), std::placeholders::_1 /*error*/));
	}

private:
	ScreenConnection(asio::io_context& io_context, const ScreenInfo info, const std::function<ScreenBuffer()> &callback)
	: socket_(io_context)
	, info_(info)
	, callback_(callback) {}

	void handle_write(asio::error_code error) {
		if (!error) {
			ScreenBuffer buffer = callback_(); 
			asio::async_write(socket_, asio::buffer(buffer.screen, buffer.screen_size),
				std::bind(&ScreenConnection::handle_write, shared_from_this(), std::placeholders::_1 /*error*/));
		}
	}

	tcp::socket socket_;
	const ScreenInfo info_;
	const std::function<ScreenBuffer()> callback_;
	void *data_;
	size_t data_size_;
};


class ScreenServer {
public:
	ScreenServer(asio::io_context& io_context, const ScreenInfo info, const std::function<ScreenBuffer()> &callback)
		: io_context_(io_context)
		, acceptor_(io_context, tcp::endpoint(tcp::v4(), SOCKET_SCREEN_PORT))
		, info_(info)
		, callback_(callback) {
		start_accept();
	}

private:
	void start_accept() {
		ScreenConnection::pointer new_connection = ScreenConnection::create(io_context_, info_, callback_);

		acceptor_.async_accept(new_connection->socket(),
				std::bind(&ScreenServer::handle_accept, this, new_connection, std::placeholders::_1 /*error*/));
	}

	void handle_accept(ScreenConnection::pointer new_connection, const asio::error_code& error) {
		if (!error) {
			new_connection->start();
		}
		start_accept();
	}

	asio::io_context& io_context_;
	tcp::acceptor acceptor_;
	ScreenInfo info_;
	const std::function<ScreenBuffer()> callback_;
};