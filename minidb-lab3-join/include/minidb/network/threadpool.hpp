#ifndef __THREAD_POOL_HPP__
#define __THREAD_POOL_HPP__

#include <thread>
#include <vector>

class DataBase;

/**
 * @class ThreadPool
 * @brief A pool of threads to process client connections.
 *
 * Initializes a fixed number of worker threads that handle connections using the provided database instance.
 */
class ThreadPool {
  private:
	std::vector<std::thread> workers;
	DataBase&				 db;
	bool					 ready;

  public:
	ThreadPool(DataBase& db_, size_t num_workers);
	~ThreadPool();

	bool started() const;

  private:
	void stop();
};

#endif
