#ifndef __CONNECTION_HPP__
#define __CONNECTION_HPP__

#include <cstddef>
#include <string>

/**
 * @class Connection
 * @brief Encapsulates a single client connection.
 *
 * Provides functionality for sending and receiving messages over a file descriptor,
 * and handles graceful connection closing.
 */
class Connection {
  private:
	const size_t RBUFSIZE = 1024;
	int			 fd;
	bool		 closed;

  public:
	Connection(int fd);
	~Connection();
	void _close();
	bool is_closed() const;
	bool transmit(const std::string& msg);
	bool recieve(std::string& buff);
};

#endif
