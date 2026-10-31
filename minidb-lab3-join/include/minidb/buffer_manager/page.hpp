#ifndef __PAGE_HPP__
#define __PAGE_HPP__

#include <cstdint>
#include <string>
#include <vector>

const size_t   PAGE_SIZE   = 4096;
const uint16_t HEADER_SIZE = 4;
const uint16_t SLOT_SIZE   = 4;

/**
 * @struct PageId
 * @brief Identifies a page uniquely within a table by table ID and page number.
 *
 * Used as a key in hash maps to track pages in buffer manager.
 */
struct PageId {
	int table_id;
	int page_number;

	bool operator==(const PageId& other) const {
		return (table_id == other.table_id && page_number == other.page_number);
	}
};

namespace std {
	/**
	 * @brief Hash specialization for PageId to be used in unordered containers.
	 */
	template <> struct hash<PageId> {
		std::size_t operator()(const PageId& pid) const {
			return std::hash<int>()(pid.table_id) ^ std::hash<int>()(pid.page_number << 1);
		}
	};
} // namespace std

/**
 * @struct Page
 * @brief Represents a fixed-size page of raw data.
 *
 * Contains a byte array of PAGE_SIZE length.
 * Provides accessors to manipulate the underlying data.
 */
struct Page {
  private:
	std::vector<uint8_t> data;

  public:
	Page();
	std::vector<uint8_t>&		get_data();
	const std::vector<uint8_t>& get_data() const;
};

/**
 * @class SlottedPage
 * @brief A slotted-page layout for managing variable-length records within a fixed-size page.
 *
 * This class wraps a raw Page and provides a layout for:
 * - Page header (number of slots, free pointer)
 * - Slot directory (array of offsets/lengths pointing to records)
 * - Record area (actual record data, growing from end of page)
 *
 * Slotted pages allow:
 * - Efficient insertion and deletion of variable-length records
 * - Fast access to records by slot number
 * - Compact page storage with low fragmentation
 *
 * Page layout:
 * -----------------------------------------------------
 * | num_slots (2B) | free_ptr (2B) | Slot 0 | Slot 1 | ...
 * -----------------------------------------------------
 * |               Free Space                        |
 * -----------------------------------------------------
 * |           Record N          |   Record N-1       |
 * -----------------------------------------------------
 *
 * Deleted records are marked by setting the slot offset to 0xFFFF.
 *
 * Usage:
 *   - insert_record(data) → returns slot number
 *   - get_record(slot) → returns data
 *   - delete_record(slot) → logically deletes the record
 */
class SlottedPage {
  private:
	Page&				  page;
	std::vector<uint8_t>& data;

	uint16_t&		num_slots();
	uint16_t&		free_ptr();
	const uint16_t& free_ptr() const;

	struct Slot {
		uint16_t offset;
		uint16_t length;
	};

	Slot get_slot(int slot_id);
	void set_slot(int slot_id, Slot s);
	bool ensure_space(int length);

  public:
	SlottedPage(Page& page);

	int					 insert_record(const std::vector<uint8_t>& record);
	std::vector<uint8_t> get_record(int slot_id) const;
	void				 delete_record(int slot_id);

	int get_num_slots() const;
	int get_free_space() const;

	bool is_slot_valid(int slot_id) const;
};

#endif
