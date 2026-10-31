#include "minidb/IdGenerator.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

static void test_sequential() {
	IdGenerator gen;

	int a = gen();
	int b = gen();
	int c = gen();

	assert(b == a + 1);
	assert(c == b + 1);

	std::cout << "[PASS] sequential ID generation\n";
}

static void test_concurrent() {
	IdGenerator gen;

	constexpr int	 num_threads = 8;
	constexpr int	 per_thread	 = 1000;
	std::vector<int> results;
	results.reserve(num_threads * per_thread);

	std::vector<std::thread> threads;

	std::mutex mtx;

	for (int t = 0; t < num_threads; t++) {
		threads.emplace_back([&]() {
			for (int i = 0; i < per_thread; i++) {
				int							id = gen();
				std::lock_guard<std::mutex> lock(mtx);
				results.push_back(id);
			}
		});
	}

	for (auto& th : threads)
		th.join();

	// Verify uniqueness
	std::sort(results.begin(), results.end());
	auto it = std::unique(results.begin(), results.end());
	assert(it == results.end());

	std::cout << "[PASS] concurrent ID generation\n";
}

int main() {
	test_sequential();
	test_concurrent();
}
