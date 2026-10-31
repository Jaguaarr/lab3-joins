#include "minidb/buffer_manager/buffer_manager.hpp"

#include <algorithm>

BufferManager::BufferManager(size_t pool_size_) : pool_size(pool_size_) {
}

void BufferManager::register_table(int table_id, HeapFile* heap_file) {
	table_files[table_id] = heap_file;
}

HeapFile* BufferManager::get_heap_file(int table_id) {
	auto it = table_files.find(table_id);
	if (it == table_files.end()) {
		return (nullptr);
	}
	return it->second;
}

Page& BufferManager::fetch_page(const PageId& page_id) {
	// check if page already in mem
	auto it = page_table.find(page_id);
	if (it != page_table.end()) {
		it->second.pin_count++;
		update_LRU(page_id);
		return it->second.page;
	}

	// Eviction case. Pinned pages are never evicted: if every frame is pinned
	// the pool temporarily grows past pool_size and shrinks back on later fetches.
	while (page_table.size() >= pool_size && this->evict_page()) {
	}

	// Load from disk
	HeapFile* heap_file = table_files[page_id.table_id];
	Page	  page		= heap_file->read_page(page_id.page_number);

	// buffer frma creation

	BufferFrame frame;
	frame.page_id = page_id;
	frame.page	  = std::move(page);

	frame.pin_count = 1;
	frame.is_dirty	= false;

	page_table[page_id] = frame;
	this->update_LRU(page_id);
	peak_frames_ = std::max(peak_frames_, page_table.size());

	return (page_table[page_id].page);
}

void BufferManager::unpin_page(const PageId& page_id, bool is_dirty) {
	auto it = page_table.find(page_id);

	if (it == page_table.end())
		return;
	if (it->second.pin_count == 0)
		return;
	it->second.pin_count--;
	if (is_dirty) {
		it->second.is_dirty = true;
	}
}

void BufferManager::flush_page(const PageId& page_id) {
	auto it = page_table.find(page_id);

	if (it == page_table.end())
		return;

	BufferFrame& frame = it->second;
	if (frame.is_dirty) {
		HeapFile* heap_file = table_files[frame.page_id.table_id];

		heap_file->write_page(frame.page_id.page_number, frame.page);
		frame.is_dirty = false;
	}
}

void BufferManager::flush_all() {
	for (const auto& p : page_table) {
		this->flush_page(p.first);
	}
}

void BufferManager::update_LRU(const PageId& page_id) {
	lrulist.remove(page_id);
	lrulist.push_front(page_id);
}

bool BufferManager::evict_page() {
	for (auto it = lrulist.rbegin(); it != lrulist.rend(); ++it) {
		PageId		 condidate_id = *it;
		BufferFrame& frame		  = page_table[condidate_id];
		if (frame.pin_count > 0)
			continue;

		if (frame.is_dirty) {
			HeapFile* heap_file = table_files[condidate_id.table_id];
			heap_file->write_page(condidate_id.page_number, frame.page);
		}

		page_table.erase(condidate_id);
		lrulist.remove(condidate_id);
		return true;
	}
	return false;
}
