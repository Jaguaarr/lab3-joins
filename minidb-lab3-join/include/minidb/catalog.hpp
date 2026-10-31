#ifndef __CATALOG_HPP__
#define __CATALOG_HPP__

#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "table.hpp"

/**
 * @class CatalogManager
 * @brief Manages the catalog metadata of database tables.
 *
 * Responsible for storing, retrieving, and persisting metadata information
 * about tables (such as schemas and table properties).
 * Supports thread-safe access using a shared mutex.
 *
 * Provides functionality to:
 * - Load and save catalog data to disk
 * - Create, retrieve, list, and drop tables in the catalog
 *
 * The catalog tracks a collection of TableInfo objects identified by table name.
 */
class CatalogManager {
  private:
	std::string								   path;
	mutable std::shared_mutex				   mutex_;
	std::unordered_map<std::string, TableInfo> tables_;
	bool									   dirty_;

  public:
	CatalogManager(const std::string& filepath);
	~CatalogManager();
	bool					 saveToDisk(void);
	bool					 loadFromDisk(void);
	bool					 createTable(const TableInfo& table);
	std::optional<TableInfo> getTable(const std::string& name) const;
	std::vector<std::string> tables_names() const;
	bool					 dropTable(const std::string& name);

	// index methods
	bool createIndex(const std::string& table_name, const IndexInfo& idx);
	bool dropIndex(const std::string& table_name, const std::string& index_name);
	std::optional<IndexInfo> get_index(const std::string& table_name,
									   std::string&		  index_name) const;
	std::vector<IndexInfo>	 listIndexes(const std::string& table_name) const;
	bool updateIndexRoot(const std::string& table, const std::string& column, int new_root);
};

#endif
