#ifndef __BUFFER_MANAGER_HPP__
#define __BUFFER_MANAGER_HPP__

#include "heapfile.hpp"
#include "page.hpp"

#include <list>
#include <unordered_map>

/**
 * @struct BufferFrame
 * @brief Represents a frame in the buffer pool that caches a page.
 *
 * Stores the page data, page ID, dirty flag, and pin count for concurrency control.
 */
struct BufferFrame {
	PageId page_id;
	Page   page;
	bool   is_dirty	 = false;
	int	   pin_count = 0;
};

/**
 * @class BufferManager
 * @brief Manages an in-memory buffer pool for caching pages from multiple tables.
 *
 * different page replacement policy can be implemented
 * Supports page fetch, unpinning, flushing individual or all pages.
 * Coordinates with HeapFile to read/write pages from/to disk.
 */
class BufferManager {
  private:
	size_t									pool_size;
	std::unordered_map<int, HeapFile*>		table_files;
	std::unordered_map<PageId, BufferFrame> page_table;
	std::list<PageId>						lrulist;
	size_t									peak_frames_ = 0;

	bool evict_page(); // false if every frame is pinned
	void update_LRU(const PageId& page_id);

  public:
	BufferManager(size_t pool_size);
	void	  register_table(int tableId, HeapFile* heap_file);
	HeapFile* get_heap_file(int table_id);

	Page& fetch_page(const PageId& page_id);
	void  unpin_page(const PageId& page_id, bool is_dirty);
	void  flush_page(const PageId& page_id);
	void  flush_all();

	// Most frames held at the same time (above pool_size if too many pages were pinned)
	size_t peak_frames() const { return peak_frames_; }

	int allocate_page(int table_id) {
		HeapFile* file = this->get_heap_file(table_id);
		return (file->allocate_page());
	}
};

#endif
