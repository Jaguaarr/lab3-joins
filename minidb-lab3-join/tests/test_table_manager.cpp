#include "minidb/buffer_manager/buffer_manager.hpp"
#include "minidb/buffer_manager/page.hpp"
#include "minidb/table_manager/table_manager.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs				= std::filesystem;
const std::string TEST_FILE = "test_table_manager.tbl";

void clean() {
	if (fs::exists(TEST_FILE)) {
		fs::remove(TEST_FILE);
	}
}

void test_create_and_get_table() {
	clean();

	BufferManager bm(4);
	TableManager  tm(bm);

	int id = tm.create_table("my_table", TEST_FILE);
	assert(id > 0);

	int id2 = tm.get_table_id("my_table");
	assert(id2 == id);

	std::cout << "[PASS] Table creation and get_table_id.\n";
}

void test_insert_and_get() {
	clean();

	BufferManager bm(4);
	TableManager  tm(bm);

	int					 id	  = tm.create_table("tbl", TEST_FILE);
	std::vector<uint8_t> data = {10, 20, 30};

	RecordId			 rid	= tm.insert(id, data);
	std::vector<uint8_t> result = tm.get(id, rid);

	assert(result == data);

	std::cout << "[PASS] Insert and get record.\n";
}

void test_insert_and_scan() {
	clean();

	BufferManager bm(4);
	TableManager  tm(bm);

	int id = tm.create_table("tbl", TEST_FILE);

	for (int i = 0; i < 5; ++i) {
		std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
		tm.insert(id, data);
	}

	std::vector<RecordId> scanned = tm.scan(id);
	assert(scanned.size() == 5);

	std::cout << "[PASS] Insert multiple and scan.\n";
}

void test_insert_remove_scan() {
	clean();

	BufferManager bm(4);
	TableManager  tm(bm);

	int id = tm.create_table("tbl", TEST_FILE);

	std::vector<uint8_t> data1 = {1};
	std::vector<uint8_t> data2 = {2};

	RecordId r1 = tm.insert(id, data1);
	RecordId r2 = tm.insert(id, data2);

	tm.remove(id, r1);

	auto results = tm.scan(id);
	assert(results.size() == 1);
	assert(results[0].slot_number == r2.slot_number);

	std::cout << "[PASS] Insert, remove, and scan valid record.\n";
}

int main() {
	test_create_and_get_table();
	test_insert_and_get();
	test_insert_and_scan();
	test_insert_remove_scan();

	std::cout << "[PASS] All TableManager tests.\n";
	return 0;
}
