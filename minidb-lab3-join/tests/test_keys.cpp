#include "minidb/index/b_plus_tree.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

void test_serialize_deserialize_int() {
	uint8_t buffer[16];

	int original = 42;
	serialize_key<int>(buffer, original);
	int recovered = deserialize_key<int>(buffer);
	assert(original == recovered);
	std::cout << "[PASS] int serialize/deserialize\n";
}

void test_serialize_deserialize_float() {
	uint8_t buffer[16];

	float original = 3.14f;
	serialize_key<float>(buffer, original);
	float recovered = deserialize_key<float>(buffer);
	assert(original == recovered);
	std::cout << "[PASS] float serialize/deserialize\n";
}

void test_serialize_deserialize_bool() {
	uint8_t buffer[4];
	bool	original = true;
	serialize_key<bool>(buffer, original);
	bool recovered = deserialize_key<bool>(buffer);
	assert(original == recovered);

	original = false;
	serialize_key<bool>(buffer, original);
	recovered = deserialize_key<bool>(buffer);
	assert(original == recovered);
	std::cout << "[PASS] bool serialize/deserialize\n";
}

void test_serialize_deserialize_string() {
	uint8_t buffer[256];

	std::string original = "hello world";
	serialize_key<std::string>(buffer, original);
	std::string recovered = deserialize_key<std::string>(buffer);
	assert(original == recovered);
	std::cout << "[PASS] string serialize/deserialize\n";
}

int main() {
	test_serialize_deserialize_int();
	test_serialize_deserialize_float();
	test_serialize_deserialize_bool();
	test_serialize_deserialize_string();

	std::cout << "[ALL KEY TESTS PASSED]\n";
	return (0);
}
