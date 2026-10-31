#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

bool send_length_prefixed(int sockfd, const std::string& data) {
	uint32_t len = htonl(data.size());
	if (send(sockfd, &len, sizeof(len), 0) != sizeof(len)) {
		perror("Failed to send length");
		return false;
	}
	if (send(sockfd, data.data(), data.size(), 0) != static_cast<ssize_t>(data.size())) {
		perror("Failed to send data");
		return false;
	}
	return true;
}

bool recv_length_prefixed(int sockfd, std::string& out_data) {
	uint32_t len_net;
	if (recv(sockfd, &len_net, sizeof(len_net), MSG_WAITALL) != sizeof(len_net)) {
		perror("Failed to receive length");
		return false;
	}

	uint32_t len = ntohl(len_net);

	out_data.resize(len);
	if (recv(sockfd, &out_data[0], len, MSG_WAITALL) != static_cast<ssize_t>(len)) {
		perror("Failed to receive data");
		return false;
	}

	return true;
}

int main() {
	const char* hostname = "127.0.0.1";
	const int	port	 = 5000;

	// Create socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("Socket creation failed");
		return 1;
	}

	// Resolve hostname
	struct hostent* server = gethostbyname(hostname);
	if (!server) {
		std::cerr << "No such host\n";
		return 1;
	}

	// Setup server address
	struct sockaddr_in serv_addr;
	std::memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	std::memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	serv_addr.sin_port = htons(port);

	// Connect
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("Connect failed");
		return 1;
	}

	std::string query;

	for (;;) {
		std::cout << ">> ";
		std::getline(std::cin, query);
		if (!send_length_prefixed(sockfd, query)) {
			std::cerr << "Failed to send query\n";
			close(sockfd);
			return 1;
		}

		std::string response;
		if (!recv_length_prefixed(sockfd, response)) {
			std::cerr << "Failed to receive response\n";
			close(sockfd);
			return 1;
		}

		std::cout << response << "\n";
	}
	close(sockfd);
	return (0);
}
