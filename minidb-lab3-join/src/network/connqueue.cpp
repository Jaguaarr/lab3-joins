#include "minidb/network/connqueue.hpp"

#include "minidb/network/connection.hpp"

void ConnectionQueue::push(Connection conn) {
	std::lock_guard<std::mutex> lock(mtx);

	queue.push(conn);
	cv.notify_all();
}

Connection ConnectionQueue::pop() {
	std::unique_lock<std::mutex> lock(mtx);
	cv.wait(lock, [this] { return !queue.empty(); });
	Connection con = queue.front();
	queue.pop();
	return con;
}

bool ConnectionQueue::is_empty() {
	std::lock_guard<std::mutex> lock(mtx);
	return queue.empty();
}
