#include "minidb/buffer_manager/buffer_manager.hpp"
#include "minidb/index/b_plus_tree.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static const std::string TEST_DIR	  = "test_idx_dir";
static const std::string CATALOG_FILE = "catalog.json";
static const int		 TABLE_ID	  = 42;

static void cleanup() {
	if (fs::exists(TEST_DIR)) {
		fs::remove_all(TEST_DIR);
	}
	fs::create_directory(TEST_DIR);

	if (fs::exists(CATALOG_FILE)) {
		fs::remove(CATALOG_FILE);
	}
}

static TableInfo make_users_table() {
	TableInfo tinfo;
	tinfo.name = "users";
	tinfo.columns.push_back({"id", DataType::INT});
	tinfo.columns.push_back({"name", DataType::STRING});
	return tinfo;
}

static RecordId rid(int p, int s) {
	return (RecordId{p, s});
}

static void test_create_and_drop() {
	cleanup();

	BufferManager  bm(64);
	CatalogManager catalog(CATALOG_FILE);
	IndexManager   im(bm, catalog, TEST_DIR);

	// register table in catalog first
	catalog.createTable(make_users_table());

	// create index
	im.create_index("users", "id");
	assert(fs::exists(TEST_DIR + "/users_id.idx"));

	// drop index
	im.drop_index("users", "id");
	assert(!fs::exists(TEST_DIR + "/users_id.idx"));

	std::cout << "[PASS] create and drop index\n";
}

static void test_insert_and_search() {
	cleanup();

	BufferManager  bm(64);
	CatalogManager catalog(CATALOG_FILE);
	IndexManager   im(bm, catalog, TEST_DIR);

	catalog.createTable(make_users_table());

	im.create_index("users", "id");

	for (int i = 0; i < 50; i++) {
		im.insert("users", "id", i, rid(0, i));
	}

	for (int i = 0; i < 50; i++) {
		auto res = im.search("users", "id", i);
		assert(res.has_value());
		assert(res->slot_number == i);
	}

	// notfound keys
	auto notfound = im.search("users", "id", 1234);
	assert(!notfound.has_value());

	std::cout << "[PASS] insert and search\n";
}

// range search
static void test_range_search() {
	cleanup();

	BufferManager  bm(64);
	CatalogManager catalog(CATALOG_FILE);
	IndexManager   im(bm, catalog, TEST_DIR);

	catalog.createTable(make_users_table());
	im.create_index("users", "id");

	for (int i = 0; i < 50; i++) {
		im.insert("users", "id", i, rid(0, i));
	}

	auto results = im.range_search("users", "id", 20, 30);

	assert(results.size() == 11);
	for (int i = 0; i < results.size(); i++) {
		assert(results[i].slot_number == 20 + i);
	}

	std::cout << "[PASS] range search\n";
}

int main() {
	test_create_and_drop();
	test_insert_and_search();
	test_range_search();

	std::cout << "[ALL IndexManager TESTS PASSED]\n";
	return (0);
}

