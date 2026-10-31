#include "minidb/buffer_manager/heapfile.hpp"
#include "minidb/buffer_manager/page.hpp"

#include <cassert>
#include <cstdio> // for std::remove
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

const std::string TEST_FILENAME = "test_heapfile.tdl";

void cleanup() {
	if (fs::exists(TEST_FILENAME)) {
		std::remove(TEST_FILENAME.c_str());
	}
}

void test_heapfile_creation_and_allocation() {
	cleanup();

	HeapFile hf(TEST_FILENAME);
	assert(fs::exists(TEST_FILENAME));
	assert(hf.get_file_name() == TEST_FILENAME);
	assert(hf.get_num_pages() == 0);

	int page_num = hf.allocate_page();
	assert(page_num == 0);
	assert(hf.get_num_pages() == 1);

	std::cout << "[PASS] HeapFile creation and allocation.\n";
}

void test_write_and_read_page() {
	HeapFile hf(TEST_FILENAME);

	Page  page;
	auto& data = page.get_data();
	for (int i = 0; i < 10; ++i) {
		data[i] = static_cast<uint8_t>(i + 1);
	}

	hf.write_page(0, page);
	Page  read		= hf.read_page(0);
	auto& read_data = read.get_data();

	for (int i = 0; i < 10; ++i) {
		assert(read_data[i] == static_cast<uint8_t>(i + 1));
	}

	std::cout << "[PASS] Page written and read correctly.\n";
}

int main() {
	test_heapfile_creation_and_allocation();
	test_write_and_read_page();
	cleanup();
	std::cout << "[ALL TESTS PASSED]\n";
	return 0;
}
