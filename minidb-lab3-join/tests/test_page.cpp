#include "minidb/buffer_manager/page.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void test_page_initialization() {
	Page  p;
	auto& data = p.get_data();
	assert(data.size() == PAGE_SIZE);
	for (auto byte : data) {
		assert(byte == 0);
	}
	std::cout << "[PASS] Page initialized with zeroed data.\n";
}

void test_insert_and_get_record() {
	Page		p;
	SlottedPage sp(p);

	std::vector<uint8_t> record1 = {1, 2, 3};
	std::vector<uint8_t> record2 = {4, 5, 6, 7};

	int slot1 = sp.insert_record(record1);
	int slot2 = sp.insert_record(record2);

	assert(slot1 == 0);
	assert(slot2 == 1);

	auto result1 = sp.get_record(slot1);
	auto result2 = sp.get_record(slot2);

	assert(result1 == record1);
	assert(result2 == record2);

	std::cout << "[PASS] Insert and get_record working correctly.\n";
}

void test_delete_and_validate_slot() {
	Page		p;
	SlottedPage sp(p);

	std::vector<uint8_t> record = {42, 43};
	int					 slot	= sp.insert_record(record);
	assert(sp.is_slot_valid(slot));

	sp.delete_record(slot);
	assert(!sp.is_slot_valid(slot));

	try {
		sp.get_record(slot);
		assert(false); // Should not reach here
	} catch (const std::runtime_error& e) {
		std::cout << "[PASS] Deleted record throws error as expected.\n";
	}
}

int main() {
	test_page_initialization();
	test_insert_and_get_record();
	test_delete_and_validate_slot();

	std::cout << "[ALL TESTS PASSED]\n";
	return 0;
}
