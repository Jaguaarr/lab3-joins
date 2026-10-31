#include "minidb/buffer_manager/heapfile.hpp"

#include "minidb/buffer_manager/page.hpp"

#include <filesystem>
#include <fstream>

HeapFile::HeapFile(const std::string& filename_) : filename(filename_) {
	if (!std::filesystem::exists(filename)) {
		std::ofstream file(filename, std::ios::binary);
	}
}

Page HeapFile::read_page(int page_number) {
	++reads;
	std::ifstream file(filename, std::ios::binary);
	Page		  page;

	std::vector<uint8_t>& data = page.get_data();

	file.seekg(page_number * PAGE_SIZE, std::ios::beg);
	file.read(reinterpret_cast<char*>(data.data()), PAGE_SIZE);

	return page;
}

void HeapFile::write_page(int page_number, const Page& page) {
	std::fstream file(filename, std::ios::in | std::ios::out | std::ios::binary);

	const std::vector<uint8_t>& data = page.get_data();

	file.seekp(page_number * PAGE_SIZE, std::ios::beg);

	file.write(reinterpret_cast<const char*>(data.data()), PAGE_SIZE);
	file.flush();
}

int HeapFile::allocate_page() {
	std::fstream file(filename, std::ios::in | std::ios::out | std::ios::binary);

	file.seekp(0, std::ios::end);
	std::streampos end		   = file.tellp();
	int			   page_number = static_cast<int>(end / PAGE_SIZE);

	Page empty_page;

	const std::vector<uint8_t>& data = empty_page.get_data();

	file.write(reinterpret_cast<const char*>(data.data()), PAGE_SIZE);
	file.flush();

	return (page_number);
}

int HeapFile::get_num_pages() const {
	namespace fs = std::filesystem;

	uintmax_t file_size = fs::file_size(filename);
	return (static_cast<int>(file_size / PAGE_SIZE));
}
