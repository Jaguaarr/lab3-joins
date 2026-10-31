#ifndef __ID_GENERATOR_HPP__
#define __ID_GENERATOR_HPP__

#include <atomic>

class IdGenerator {
  private:
	static std::atomic<int> next_id;

  public:
	IdGenerator() = default;
	int operator()() { return next_id.fetch_add(1, std::memory_order_relaxed); }
};

#endif
