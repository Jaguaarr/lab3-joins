#include "minidb/catalog.hpp"
#include "minidb/table.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

namespace fs				   = std::filesystem;
const std::string CATALOG_FILE = "test_catalog.json";

void clean() {
	if (fs::exists(CATALOG_FILE)) {
		fs::remove(CATALOG_FILE);
	}
}

TableInfo sample_table_info() {
	TableInfo info;
	info.name	 = "users";
	info.columns = {{"id", DataType::INT}, {"name", DataType::STRING}};
	return info;
}

void test_create_and_get_table() {
	clean();

	CatalogManager catalog(CATALOG_FILE);
	TableInfo	   tinfo = sample_table_info();

	bool created = catalog.createTable(tinfo);
	assert(created);

	auto result = catalog.getTable("users");
	assert(result.has_value());
	assert(result->name == "users");
	assert(result->columns.size() == 2);
	assert(result->columns[0].name == "id");

	std::cout << "[PASS] Catalog create and getTable.\n";
}

void test_drop_table() {
	clean();

	CatalogManager catalog(CATALOG_FILE);
	catalog.createTable(sample_table_info());

	bool dropped = catalog.dropTable("users");
	assert(dropped);

	auto result = catalog.getTable("users");
	assert(!result.has_value());

	std::cout << "[PASS] Catalog dropTable and validate not found.\n";
}

void test_save_and_load() {
	clean();

	{
		CatalogManager catalog(CATALOG_FILE);
		catalog.createTable(sample_table_info());
		catalog.saveToDisk();
	}

	{
		CatalogManager catalog(CATALOG_FILE);
		auto		   result = catalog.getTable("users");
		assert(result.has_value());
		assert(result->columns.size() == 2);
		assert(result->columns[1].name == "name");
		assert(result->columns[1].type_ == DataType::STRING);
	}

	std::cout << "[PASS] Catalog saveToDisk and loadFromDisk.\n";
}

void test_tables_names() {
	clean();

	CatalogManager catalog(CATALOG_FILE);

	TableInfo t1 = sample_table_info();
	TableInfo t2 = t1;
	t2.name		 = "orders";

	catalog.createTable(t1);
	catalog.createTable(t2);

	auto names = catalog.tables_names();
	assert(names.size() == 2);
	assert((names[0] == "users" || names[1] == "users"));

	std::cout << "[PASS] Catalog tables_names listing.\n";
}

int main() {
	test_create_and_get_table();
	test_drop_table();
	test_save_and_load();
	test_tables_names();

	std::cout << "[PASS] All CatalogManager tests.\n";
	return 0;
}
