#ifndef __CONNECTION_QUEUE_HPP__
#define __CONNECTION_QUEUE_HPP__

#include <condition_variable>
#include <mutex>
#include <queue>

#include "minidb/network/connection.hpp"

/**
 * @class ConnectionQueue
 * @brief Thread-safe queue for managing incoming connections.
 *
 * Wraps a standard queue with mutex and condition variable
 * to support thread-safe access and blocking behavior.
 */
class ConnectionQueue {
  private:
	std::queue<Connection>	queue;
	std::mutex				mtx;
	std::condition_variable cv;

  public:
	ConnectionQueue()  = default;
	~ConnectionQueue() = default;

	ConnectionQueue(const ConnectionQueue&)			   = delete;
	ConnectionQueue& operator=(const ConnectionQueue&) = delete;
	void			 push(Connection conn);
	Connection		 pop();
	bool			 is_empty();
};

#endif
