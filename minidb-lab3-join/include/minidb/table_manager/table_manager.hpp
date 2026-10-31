#ifndef __TABLE_MANAGER_HPP__
#define __TABLE_MANAGER_HPP__

#include "minidb/IdGenerator.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class BufferManager;
class HeapFile;

/**
 * @struct RecordId
 * @brief Identifies a record within a page using page number and slot number.
 *
 * Used as a handle to locate specific records in table data.
 */
struct RecordId {
	int page_number;
	int slot_number;

	bool operator==(const RecordId& other) const {
		return (page_number == other.page_number && slot_number == other.slot_number);
	}
};

namespace std {
	template <> struct hash<RecordId> {
		std::size_t operator()(const RecordId& rid) const {
			return std::hash<int>()(rid.page_number) ^ std::hash<int>()(rid.slot_number << 1);
		}
	};
} // namespace std

/**
 * @class TableManager
 * @brief Provides high-level management of tables and their records.
 *
 * Manages multiple tables identified by name and ID.
 * Supports table creation, record insertion, retrieval, deletion, and scanning.
 * Coordinates with BufferManager and HeapFile for page and record storage.
 */
class TableManager {
  private:
	std::unordered_map<std::string, int> name_to_id;
	std::unordered_map<int, HeapFile>	 tables;
	BufferManager&						 buffer_manager;
	IdGenerator							 id_gen;

  public:
	TableManager(BufferManager& buffer_manager);

	int create_table(const std::string& name, const std::string& filename);
	int get_table_id(const std::string& name);

	RecordId			  insert(int table_id, const std::vector<uint8_t>& data);
	std::vector<uint8_t>  get(int table_id, const RecordId& rid);
	void				  remove(int table_id, const RecordId& id);
	std::vector<RecordId> scan(int table_id);
};

#endif
