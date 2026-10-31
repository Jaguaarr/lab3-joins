#include "minidb/catalog.hpp"
#include "minidb/utils.hpp"
#include "nlohmann/json.hpp"

#include "minidb/table.hpp"

#include <fstream>
#include <iostream>
#include <mutex>

using json = nlohmann::json;

CatalogManager::CatalogManager(const std::string& file_path) : path(file_path) {
	dirty_ = false;
	if (file_exists(path)) {
		this->loadFromDisk();
		return;
	}
	std::ofstream catalog(path);
}

CatalogManager::~CatalogManager() {
	if (dirty_)
		this->saveToDisk();
}

bool CatalogManager::loadFromDisk() {
	std::ifstream in(path);
	json		  j;

	// check the size of the file first
	in.seekg(0, std::ios::end);
	auto size = in.tellg();
	in.seekg(0, std::ios::beg);
	if (size == 0) {
		// empty
		return (true);
	}

	try {
		in >> j;
	} catch (...) {
		std::cout << "Error parsing Catalog file" << std::endl;
		exit(EXIT_FAILURE);
	}

	for (const auto& table : j["tables"]) {
		TableInfo tinfo;

		tinfo.name = table["name"];
		for (const auto& col : table["columns"]) {
			tinfo.columns.push_back({col["name"], StringToDataType(col["type"])});
		}

		// load indexes
		if (table.contains("indexes")) {
			for (const auto& idx : table["indexes"]) {
				IndexInfo ii(idx["name"], idx["column"], idx["file_path"],
							 idx.value("root_page", 0));
				tinfo.indexes.push_back(ii);
			}
		}
		auto inserted = this->tables_.insert(std::make_pair(tinfo.name, tinfo));
		if (!inserted.second)
			return (false);
	}
	return (true);
}

bool CatalogManager::saveToDisk() {
	json j;

	if (!dirty_)
		return (true);
	for (const auto& [name, tinfo] : tables_) {
		json table;

		table["name"] = tinfo.name;
		for (const auto& col : tinfo.columns) {
			table["columns"].push_back({{"name", col.name}, {"type", DataTypeToString(col.type_)}});
		}

		for (const auto& idx : tinfo.indexes) {
			table["indexes"].push_back({{"name", idx.name},
										{"column", idx.column},
										{"file_path", idx.file_path},
										{"root_page", idx.root_page}});
		}
		j["tables"].push_back(table);
	}

	std::ofstream out(path.c_str());
	if (!out)
		return (false);

	out << j.dump(4);
	dirty_ = false;
	return (true);
}

bool CatalogManager::createTable(const TableInfo& table) {
	std::unique_lock lock(mutex_);

	auto inserted = this->tables_.insert(std::make_pair(table.name, table));
	if (inserted.second) {
		dirty_ = true;
		return (true);
	}
	return (false);
}

bool CatalogManager::dropTable(const std::string& name) {
	std::unique_lock lock(mutex_);

	auto found = this->tables_.find(name);
	if (found != this->tables_.end()) {
		tables_.erase(found);
		dirty_ = true;
		return (true);
	}
	return (false);
}

std::optional<TableInfo> CatalogManager::getTable(const std::string& name) const {
	std::shared_lock lock(mutex_);

	auto found = this->tables_.find(name);
	if (found != this->tables_.end()) {
		return (found->second);
	}
	return (std::nullopt);
}

std::vector<std::string> CatalogManager::tables_names() const {
	std::vector<std::string> names;
	for (const auto& n : tables_) {
		names.push_back(n.first);
	}
	return names;
}

bool CatalogManager::createIndex(const std::string& table_name, const IndexInfo& idx) {
	std::unique_lock lock(mutex_);

	auto it = tables_.find(table_name);
	if (it == tables_.end())
		return (false);

	// check for duplicate index
	for (const auto& existing : it->second.indexes) {
		if (existing.name == idx.name) {
			return (false);
		}
	}

	it->second.indexes.push_back(idx);
	dirty_ = true;

	return (true);
}

bool CatalogManager::dropIndex(const std::string& table_name, const std::string& index_name) {
	std::unique_lock lock(mutex_);
	auto			 it = tables_.find(table_name);
	if (it == tables_.end())
		return false;

	auto& idxs	   = it->second.indexes;
	auto  old_size = idxs.size();
	idxs.erase(std::remove_if(idxs.begin(), idxs.end(),
							  [&](const IndexInfo& info) { return info.name == index_name; }),
			   idxs.end());

	if (idxs.size() < old_size) {
		dirty_ = true;
		return true;
	}

	// do we need to erase the index file ???????????????????????????
	return false;
}

// Lookup index metadata
std::optional<IndexInfo> CatalogManager::get_index(const std::string& table_name,
												   std::string&		  index_name) const {
	std::shared_lock lock(mutex_);
	auto			 it = tables_.find(table_name);
	if (it == tables_.end())
		return std::nullopt;

	for (const auto& idx : it->second.indexes) {
		if (idx.name == index_name) {
			return idx;
		}
	}
	return std::nullopt;
}

// List all indexes on a table
std::vector<IndexInfo> CatalogManager::listIndexes(const std::string& table_name) const {
	std::shared_lock lock(mutex_);
	auto			 it = tables_.find(table_name);
	if (it == tables_.end())
		return {};

	return it->second.indexes;
}

// Update root_page in catalog for an index
bool CatalogManager::updateIndexRoot(const std::string& table, const std::string& index_name,
									 int new_root) {
	std::unique_lock lock(mutex_);
	auto			 it = tables_.find(table);
	if (it == tables_.end())
		return (false);

	for (auto& idx : it->second.indexes) {
		if (idx.name == index_name) {
			idx.root_page = new_root;
			dirty_		  = true;
			return (true);
		}
	}
	return (false);
}
