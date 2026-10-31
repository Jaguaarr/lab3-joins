#ifndef __HEAP_FILE_HPP__
#define __HEAP_FILE_HPP__

#include <string>

class Page;

/**
 * @class HeapFile
 * @brief Manages disk-backed storage of pages for a single table.
 *
 * Provides reading and writing pages to a file.
 * Supports allocating new pages and tracking number of pages.
 */
class HeapFile {
  private:
	std::string filename;

  public:
	// Pages read from disk by every heap file since the last reset (tests/bench_join.cpp)
	inline static unsigned long long reads = 0;

	HeapFile() = default;
	HeapFile(const std::string& filename);

	Page			   read_page(int page_number);
	void			   write_page(int page_number, const Page& page);
	int				   allocate_page();
	int				   get_num_pages() const;
	const std::string& get_file_name() const { return (filename); }
};

#endif
