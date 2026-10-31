#include "minidb/buffer_manager/page.hpp"

#include <cstring>
#include <stdexcept>

Page::Page() : data(PAGE_SIZE, 0) {
}

std::vector<uint8_t>& Page::get_data() {
	return (data);
}

const std::vector<uint8_t>& Page::get_data() const {
	return (data);
}

SlottedPage::SlottedPage(Page& page_) : page(page_), data(page.get_data()) {
	// intialize the slotted page if this is new page
	if (free_ptr() == 0) {
		num_slots() = 0;
		free_ptr()	= PAGE_SIZE;
	}
}

uint16_t& SlottedPage::num_slots() {
	return (*reinterpret_cast<uint16_t*>(&data[0]));
}

uint16_t& SlottedPage::free_ptr() {
	return (*reinterpret_cast<uint16_t*>(&data[2]));
}

const uint16_t& SlottedPage::free_ptr() const {
	return (*reinterpret_cast<uint16_t*>(&data[2]));
}

int SlottedPage::get_num_slots() const {
	return (*reinterpret_cast<const uint16_t*>(&data[0]));
}

int SlottedPage::get_free_space() const {
	uint16_t slot_bytes = get_num_slots() * SLOT_SIZE;

	return (free_ptr() - (HEADER_SIZE + slot_bytes));
}

SlottedPage::Slot SlottedPage::get_slot(int slot_id) {
	size_t offset = HEADER_SIZE + slot_id * SLOT_SIZE;

	Slot s;
	std::memcpy(&s, &data[offset], SLOT_SIZE);
	return (s);
}

void SlottedPage::set_slot(int slot_id, Slot s) {
	size_t offset = HEADER_SIZE + slot_id * SLOT_SIZE;

	std::memcpy(&data[offset], &s, SLOT_SIZE);
}

bool SlottedPage::ensure_space(int length) {
	return (get_free_space() >= length + SLOT_SIZE);
}

int SlottedPage::insert_record(const std::vector<uint8_t>& record) {
	int length = record.size();

	if (ensure_space(length)) {
		free_ptr() -= length;
		uint16_t record_offset = free_ptr();
		std::memcpy(&data[record_offset], record.data(), length);

		Slot s{record_offset, static_cast<uint16_t>(length)};
		set_slot(num_slots(), s);
		return (num_slots()++);
	}
	return (-1);
}

std::vector<uint8_t> SlottedPage::get_record(int slot_id) const {
	size_t offset = HEADER_SIZE + slot_id * SLOT_SIZE;

	SlottedPage::Slot s;

	std::memcpy(&s, &data[offset], SLOT_SIZE);

	if (s.offset == 0xFFFF)
		throw std::runtime_error("Slot is deleted");
	std::vector<uint8_t> record(s.length);
	std::memcpy(record.data(), &data[s.offset], s.length);
	return (record);
}

void SlottedPage::delete_record(int slot_id) {
	Slot s	 = get_slot(slot_id);
	s.offset = 0xFFFF;
	set_slot(slot_id, s);
}

bool SlottedPage::is_slot_valid(int slot_id) const {
	if (slot_id >= get_num_slots())
		return (false);

	size_t offset = HEADER_SIZE + slot_id * SLOT_SIZE;
	Slot   s;
	std::memcpy(&s, &data[offset], SLOT_SIZE);

	return (s.offset != 0xFFFF);
}
