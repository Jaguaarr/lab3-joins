#include "minidb/IdGenerator.hpp"

// int id genrator
std::atomic<int> IdGenerator::next_id{1};
