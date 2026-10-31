#include "minidb/network/threadpool.hpp"

void backend_server(DataBase&);

ThreadPool::ThreadPool(DataBase& db_, size_t nthreads) : db(db_), ready(false) {
	workers.reserve(nthreads);

	for (auto i = 0; i < nthreads; i++) {
		try {
			workers.push_back(std::thread(&backend_server, std::ref(db)));
		} catch (std::exception) {
			this->stop();
		}
	}
	ready = true;
}

ThreadPool::~ThreadPool() {
	this->stop();
};

void ThreadPool::stop() {
	ready = false;
	for (auto& worker : workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
}

bool ThreadPool::started() const {
	return (ready);
}
