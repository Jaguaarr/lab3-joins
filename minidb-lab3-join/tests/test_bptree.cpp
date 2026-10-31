#include "minidb/buffer_manager/buffer_manager.hpp"
#include "minidb/index/b_plus_tree.hpp"
#include "minidb/table.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

static const std::string TEST_IDX_FILE_INT = "test_bplustree_int.idx";
static const std::string TEST_IDX_FILE_STR = "test_bplustree_str.idx";
static const int		 TABLE_ID_INT	   = 1001;
static const int		 TABLE_ID_STR	   = 1002;

static void cleanup() {
	if (fs::exists(TEST_IDX_FILE_INT))
		std::remove(TEST_IDX_FILE_INT.c_str());
	if (fs::exists(TEST_IDX_FILE_STR))
		std::remove(TEST_IDX_FILE_STR.c_str());
}

static RecordId rid(int p, int s) {
	RecordId r;
	r.page_number = p;
	r.slot_number = s;
	return (r);
}

static constexpr int compute_leaf_capacity() {
	constexpr int PAGE_SIZE	   = 4096;
	constexpr int HEADER_BYTES = 11;
	int			  entry_size   = sizeof(int) + sizeof(RecordId);
	return (PAGE_SIZE - HEADER_BYTES) / entry_size;
}

// -------------------- Test 1: single insert + search (int) ----
static void test_single_insert_search_int() {
	cleanup();

	HeapFile	  hf(TEST_IDX_FILE_INT);
	BufferManager bm(16);
	bm.register_table(TABLE_ID_INT, &hf);

	// Start with empty tree (root page = -1)
	BPlusTree<int> tree(bm, TABLE_ID_INT, -1);

	RecordId r = rid(1, 2);
	tree.insert(42, r);

	auto found = tree.search(42);
	assert(found.has_value());
	assert(found->page_number = r.page_number && found->slot_number == r.slot_number);

	// not found case
	auto notfound = tree.search(1000);
	assert(!notfound.has_value());

	std::cout << "[PASS] single insert + search (int)\n";
}

// -------- Test 2: many inserts cause splits + exact search (int)
static void test_many_inserts_and_search_int() {
	cleanup();

	HeapFile	  hf(TEST_IDX_FILE_INT);
	BufferManager bm(64);
	bm.register_table(TABLE_ID_INT, &hf);

	// Track root updates
	std::vector<int> root_changes;
	auto			 on_root_changed = [&](int new_root) { root_changes.push_back(new_root); };

	BPlusTree<int> tree(bm, TABLE_ID_INT, -1, on_root_changed);

	int leaf_capacity = compute_leaf_capacity();
	int num_inserts	  = leaf_capacity + 20; // guarantees split

	for (int i = 0; i < num_inserts; i++) {
		tree.insert(i, rid(0, i));
	}

	for (int i = 0; i < num_inserts; i++) {
		auto rid = tree.search(i);
		assert(rid.has_value());
		assert(rid->slot_number == i);
	}

	assert(tree.get_root_page() != -1);
	assert(!root_changes.empty());

	for (const auto& r : root_changes) {
		std::cout << r << " ";
	}
	std::cout << "\n";

	std::cout << "[PASS] many inserts + exact searchs (int)\n";
}

static void test_range_queries() {
	cleanup();

	HeapFile	  hf(TEST_IDX_FILE_INT);
	BufferManager bm(64);
	bm.register_table(TABLE_ID_INT, &hf);

	BPlusTree<int> tree(bm, TABLE_ID_INT, -1);

	int leaf_capacity = compute_leaf_capacity();
	int num_inserts	  = leaf_capacity + 20; // guarantees split

	for (int i = 0; i < num_inserts; i++) {
		tree.insert(i, rid(0, i));
	}

	// single leaf
	auto results = tree.range_search(20, 30);
	assert(results.size() == 11);

	for (int i = 0; i < results.size(); i++) {
		assert(results[i].slot_number == 20 + i);
	}

	std::cout << "[PASS] single leaf span range search\n";

	// Split boundary is at mid = floor((cap+1)/2) = 170
	const int split_key = (leaf_capacity + 1) / 2; // 170
	const int low		= split_key - 5;		   // 165
	const int high		= split_key + 5;		   // 175

	results = tree.range_search(low, high);

	// Expect inclusive count
	const int expected = high - low + 1; // 11
	assert(static_cast<int>(results.size()) == expected);

	for (int i = 0; i < expected; ++i) {
		assert(results[i].slot_number == low + i); // we inserted rid(0, key)
	}

	std::cout << "[PASS] range search across leaf boundary via next_leaf\n";
}

int main() {
	cleanup();
	test_single_insert_search_int();
	test_many_inserts_and_search_int();
	test_range_queries();

	cleanup();
	std::cout << "[ALL BPlusTree TESTS PASSED\n]";
	return (0);
}
