#include "minidb/network/server.hpp"
#include "minidb/database.hpp"
#include "minidb/network/connection.hpp"
#include "minidb/network/connqueue.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "minidb/config.hpp"
#include "minidb/shutdown.hpp"

void start_connection_server(DataBase& db) {
	struct sockaddr_in sa;
	Config*			   conf;
	int				   socketfd, clientfd, reuse;
	unsigned short	   port;

	conf = Config::get_instance();
	port = conf->port;

	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketfd == 0) {
		std::cerr << "cann't create socket" << std::endl;
		exit(EXIT_FAILURE);
	}
	// set SO_REUSEADDR option
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (void*)&reuse, sizeof(reuse)) < 0) {
		std::cerr << "reuseaddr err" << std::endl;
		close(socketfd);
		exit(EXIT_FAILURE);
	}

	// set to non blocking
	int flags = fcntl(socketfd, F_GETFL, 0);
	if (flags == -1) {
		std::cerr << "fcntl get error" << std::endl;
		close(socketfd);
		exit(EXIT_FAILURE);
	}
	if (fcntl(socketfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		std::cerr << "fcntl set error" << std::endl;
		close(socketfd);
		exit(EXIT_FAILURE);
	}

	memset(&sa, 0, sizeof(sa));
	sa.sin_family	   = AF_INET;
	sa.sin_port		   = htons(port);
	sa.sin_addr.s_addr = htons(INADDR_ANY);

	if (bind(socketfd, (const struct sockaddr*)&sa, sizeof sa) == -1) {
		std::cerr << "cannot bind to the specified port and address" << std::endl;
		close(socketfd);
		exit(EXIT_FAILURE);
	}

	if (listen(socketfd, 20) == -1) {
		std::cout << "socket listen error" << std::endl;
		close(socketfd);
		exit(EXIT_FAILURE);
	}

	while (!shutdown_requested) {
		clientfd = accept(socketfd, nullptr, nullptr);
		if (clientfd == -1) {							   // error case
			if (errno == EAGAIN || errno == EWOULDBLOCK) { // no connection yet
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}
		}
		if (clientfd != -1) {
			// add to the connection queue
			ConnectionQueue& q = db.get_connection_queue();
			q.push(Connection(clientfd));
		}
	}
}
