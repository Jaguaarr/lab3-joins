#include "minidb/table_manager/table_manager.hpp"

#include "minidb/buffer_manager/buffer_manager.hpp"

TableManager::TableManager(BufferManager& bufferM) : buffer_manager(bufferM) {
}

int TableManager::create_table(const std::string& name, const std::string& filename) {
	if (name_to_id.count(name))
		return -1;

	int table_id	 = id_gen();
	name_to_id[name] = table_id;

	tables.emplace(table_id, HeapFile(filename));

	buffer_manager.register_table(table_id, &tables[table_id]);

	return (table_id);
}

int TableManager::get_table_id(const std::string& name) {
	return name_to_id[name];
}

RecordId TableManager::insert(int table_id, const std::vector<uint8_t>& data) {

	HeapFile& heap		  = tables[table_id];
	int		  total_pages = heap.get_num_pages();

	for (int page_no = 0; page_no < total_pages; ++page_no) {
		PageId pid{table_id, page_no};

		Page&		page = buffer_manager.fetch_page(pid);
		SlottedPage sp(page);

		if (sp.get_free_space() >= (int)data.size() + SLOT_SIZE) {
			int slot = sp.insert_record(data);
			buffer_manager.unpin_page(pid, true);

			return RecordId{page_no, slot};
		}

		buffer_manager.unpin_page(pid, false);
	}

	int	   new_page = heap.allocate_page();
	PageId pid{table_id, new_page};

	Page&		page = buffer_manager.fetch_page(pid);
	SlottedPage sp(page);

	int slot = sp.insert_record(data);
	buffer_manager.unpin_page(pid, true);

	return (RecordId{new_page, slot});
}

std::vector<uint8_t> TableManager::get(int table_id, const RecordId& rid) {
	PageId pid{table_id, rid.page_number};
	Page&  page = buffer_manager.fetch_page(pid);

	SlottedPage sp(page);

	auto data = sp.get_record(rid.slot_number);
	buffer_manager.unpin_page(pid, false);

	return (data);
}

void TableManager::remove(int table_id, const RecordId& rid) {
	PageId pid{table_id, rid.page_number};
	Page&  page = buffer_manager.fetch_page(pid);

	SlottedPage sp(page);

	sp.delete_record(rid.slot_number);
	buffer_manager.unpin_page(pid, true);
}

std::vector<RecordId> TableManager::scan(int table_id) {
	std::vector<RecordId> results;

	HeapFile& heap = tables[table_id];

	int total_pages = heap.get_num_pages();

	for (int page_no = 0; page_no < total_pages; ++page_no) {
		PageId pid{table_id, page_no};

		Page&		page = buffer_manager.fetch_page(pid);
		SlottedPage sp{page};

		int slots = sp.get_num_slots();
		for (int slot_no = 0; slot_no < slots; ++slot_no) {
			if (sp.is_slot_valid(slot_no)) {
				results.push_back(RecordId{page_no, slot_no});
			}
		}

		buffer_manager.unpin_page(pid, false);
	}

	return (results);
}
