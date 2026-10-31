#include "minidb/buffer_manager/buffer_manager.hpp"
#include "minidb/buffer_manager/heapfile.hpp"
#include "minidb/buffer_manager/page.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

const std::string TEST_FILE = "test_buffer_manager.tbl";

void cleanup() {
	if (fs::exists(TEST_FILE)) {
		std::remove(TEST_FILE.c_str());
	}
}

void test_buffer_manager_basic_fetch_unpin_flush() {
	cleanup();

	HeapFile hf(TEST_FILE);
	int		 page_num = hf.allocate_page(); // page 0
	{
		Page  p;
		auto& data = p.get_data();
		data[0]	   = 42;
		hf.write_page(page_num, p);
	}

	// small pool to test eviction
	BufferManager bm(2);
	int			  table_id = 1;
	bm.register_table(table_id, &hf);

	PageId pid{table_id, page_num};
	Page&  fetched = bm.fetch_page(pid);
	assert(fetched.get_data()[0] == 42);

	// Mutate and mark dirty
	fetched.get_data()[0] = 99;
	bm.unpin_page(pid, true); // mark as dirty

	// Flush to disk
	bm.flush_page(pid);

	Page verify = hf.read_page(page_num);
	assert(verify.get_data()[0] == 99);

	std::cout << "[PASS] Fetch → Modify → Unpin → Flush cycle.\n";
}

void test_eviction_writes_dirty_page() {
	cleanup();

	HeapFile hf(TEST_FILE);
	int		 pg0 = hf.allocate_page(); // 0
	int		 pg1 = hf.allocate_page(); // 1
	int		 pg2 = hf.allocate_page(); // 2

	BufferManager bm(2);
	bm.register_table(1, &hf);

	PageId pid0{1, pg0}, pid1{1, pg1}, pid2{1, pg2};

	Page& p0		 = bm.fetch_page(pid0);
	p0.get_data()[0] = 100;
	bm.unpin_page(pid0, true); // mark dirty

	Page& p1 = bm.fetch_page(pid1);
	bm.unpin_page(pid1, false); // clean

	// Now fetch page 2, which should cause eviction
	Page& p2 = bm.fetch_page(pid2);
	bm.unpin_page(pid2, false);

	// p0 or p1 should have been evicted (whichever was LRU), but p0 was dirty
	// Let’s see if p0 was written back
	Page reloaded = hf.read_page(pg0);

	assert(reloaded.get_data()[0] == 100);

	std::cout << "[PASS] Dirty page is flushed before eviction.\n";
}

int main() {
	test_buffer_manager_basic_fetch_unpin_flush();
	test_eviction_writes_dirty_page();
	cleanup();

	std::cout << "[ALL TESTS PASSED]\n";
	return 0;
}
