#include "minidb/network/connection.hpp"

#include <cstdint>
#include <netinet/in.h>
#include <unistd.h>

Connection::Connection(int _fd) : fd(_fd), closed(false) {
}

Connection::~Connection() {
}

void Connection::_close() {
	closed = true;
	close(fd);
}

bool Connection::is_closed() const {
	return (closed);
}

bool Connection::transmit(const std::string& msg) {
	uint32_t mlength = htonl(msg.size());

	if (this->is_closed())
		return (false);
	if (send(fd, &mlength, sizeof(mlength), 0) != sizeof(mlength)) {
		return (false);
	}

	if (send(fd, msg.data(), msg.size(), 0) != static_cast<ssize_t>(msg.size())) {
		return (false);
	}
	return (true);
}

bool Connection::recieve(std::string& buff) {
	// length prefixing protocol
	uint32_t mlength;

	if (this->is_closed())
		return (false);

	// read the length of the message
	if (recv(fd, &mlength, sizeof(mlength), 0) != sizeof(mlength)) {
		this->_close();
		return (false);
	}
	mlength = ntohl(mlength);

	buff.resize(mlength);

	if (recv(fd, &buff[0], mlength, 0) != static_cast<ssize_t>(mlength)) {
		this->_close();
		return (false);
	}

	return (true);
}
