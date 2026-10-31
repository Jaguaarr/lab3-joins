// - builds leaf nodes, insert keys, seriliaze()s, reloads via deserialize() ctor
// verifies order and fields
// - splits leaf nodes and checks next_leaf, counts, and returns split key
// - builds internal nodes, verifies find_insert_position() semantics (upper_bpund)
// internal_child_for(), serialize()/deserialize()

// splits internal nodes and checks the promoted key, left/right partitions, and
// first_child on the sibling
// runs all of the above for int and std::string

#include "minidb/index/b_plus_tree.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static const std::string TEST_IDX_FILE = "test_bptree_node.idx";
static const int		 TID		   = 777; // arbitrary table id for tests

static void cleanup() {
	if (fs::exists(TEST_IDX_FILE)) {
		std::remove(TEST_IDX_FILE.c_str());
	}
}

static RecordId rid(int p, int s) {
	RecordId r{};
	r.page_number = p;
	r.slot_number = s;
	return (r);
}

// ---- LEAF : serialize/deserialize roundtrip -------
template <typename K>
static void test_leaf_roundtrip(BufferManager& bm, HeapFile& hf,
								const std::vector<std::pair<K, RecordId>>& items) {
	int				 page_no = hf.allocate_page();
	BPlusTreeNode<K> leaf	 = BPlusTreeNode<K>::create_new_leaf(bm, TID, page_no);

	// Insert in arbitrary  order, leaf_insert must key sorted by key
	for (auto& kv : items) {
		bool of = leaf.leaf_insert(kv.first, kv.second);
		(void)of;
	}
	leaf.serialize(true);
	leaf.release(true);

	// REload from disk (ctor calls deserialize)
	BPlusTreeNode<K> loaded(bm, TID, page_no);

	assert(loaded.is_leaf_node());
	assert(loaded.num_keys == items.size());

	// verify ordering and data
	K	 prev{};
	bool first = true;
	for (size_t i = 0; i < items.size(); ++i) {
		const auto& e = loaded.leaf_entries[i];
		if (!first) {
			assert(!(e.key < prev)); // increasing
		}
		first = false;
		prev  = e.key;

		// The inserted set equals the loaded set, but since we inserted arbitrary
		// order, compare by sorted-by-key behavior:
		// Find that key among original items
		bool found = false;
		for (auto& kv : items) {
			if (!(kv.first < e.key) && !(e.key < kv.first)) {
				//key equal
				found = true;
				break;
			}
		}
		assert(found);
	}
	loaded.release(false);

	std::cout << "[PASS] Leaf<" << typeid(K).name() << "> serialize/deserialize roundtrip\n";
}

template <typename K> static void test_leaf_split(BufferManager& bm, HeapFile& hf) {
	int p_left	= hf.allocate_page();
	int p_right = hf.allocate_page();

	BPlusTreeNode<K> left =
		BPlusTreeNode<K>::create_new_leaf(bm, TID, p_left, /*parent*/ 123 /*arbitrary parent*/);
	BPlusTreeNode<K> right = BPlusTreeNode<K>::create_new_leaf(bm, TID, p_right);

	// Insert a small, sorted set of keys (we will split "manually")
	// Using 6 items so left gets 3 and right gets 3 after mid split

	std::vector<std::pair<K, RecordId>> items;

	if constexpr (std::is_same_v<K, int>) {
		items = {{1, rid(0, 1)}, {2, rid(0, 2)}, {3, rid(0, 3)},
				 {4, rid(0, 4)}, {5, rid(0, 5)}, {6, rid(0, 6)}};
	} else if constexpr (std::is_same_v<K, std::string>) {
		items = {{"a", rid(0, 1)}, {"b", rid(0, 2)}, {"c", rid(0, 3)},
				 {"d", rid(0, 4)}, {"e", rid(0, 5)}, {"f", rid(0, 6)}};
	} else {
		static_assert(!sizeof(K*), "Add a concrete key set for this K.");
	}

	for (auto& kv : items)
		left.leaf_insert(kv.first, kv.second);

	// perform split
	K split_key = left.split_leaf_into(right);

	left.serialize(true);
	right.serialize(true);

	// right inehrits parent
	assert(right.parent_page == left.parent_page);

	// Leaf linking
	assert(left.next_leaf == p_right);
	// right.next_leaf remains whatever left had (-1 by default)
	// We didn't set next_leaf on left before, so right should see -1
	assert(right.next_leaf == -1);

	// Sizes
	size_t left_sz	= left.leaf_entries.size();
	size_t right_sz = right.leaf_entries.size();

	assert(left_sz + right_sz == items.size());
	assert(left_sz == items.size() / 2);
	assert(right_sz == items.size() - left_sz);

	assert(!(split_key < right.leaf_entries.front().key) &&
		   !(right.leaf_entries.front().key < split_key));

	left.release(true);
	right.release(true);
	std::cout << "[PASS] LEAF<" << typeid(K).name() << "> split\n";
};

// -------- INTERNAL: serialize/deserialize + child routing ---------------
template <typename K>
static void test_internal_roundtrip_and_routing(BufferManager& bm, HeapFile& hf) {
	int p = hf.allocate_page();

	BPlusTreeNode<K> node = BPlusTreeNode<K>::create_new_internal(bm, TID, p);
	node.parent_page	  = 222;
	node.first_child	  = 10;

	// insert seperators (they must remain sorted by key, and routing uses upper_bound)
	if constexpr (std::is_same_v<K, int>) {
		// children: [10] --1--> 11 --3--> 12 --5-->13
		node.internal_insert_seperator(1, 11, node.find_insert_position(1));
		node.internal_insert_seperator(3, 12, node.find_insert_position(3));
		node.internal_insert_seperator(5, 13, node.find_insert_position(5));

		node.serialize(true);
		node.release(true);

		// Reload
		BPlusTreeNode<K> re(bm, TID, p);
		assert(!re.is_leaf_node());
		assert(re.num_keys == 3);
		assert(re.first_child == 10);

		// Routing: upper_bound logic
		// k < 1 		---> first_child(10)
		// 1 <= k < 3 	---> 11
		// 3 <= k < 5	---> 12
		// k >= 5 		---> 13

		assert(re.internal_child_for(0) == 10);
		assert(re.internal_child_for(1) == 11);
		assert(re.internal_child_for(2) == 11);
		assert(re.internal_child_for(3) == 12);
		assert(re.internal_child_for(4) == 12);
		assert(re.internal_child_for(5) == 13);
		assert(re.internal_child_for(100) == 13);

		re.release(true);
	} else if constexpr (std::is_same_v<K, std::string>) {
		// children: [10] --"b"--> 11 --"d"--> 12 --"f"-->13
		node.internal_insert_seperator(std::string("b"), 11, node.find_insert_position("b"));
		node.internal_insert_seperator(std::string("d"), 12, node.find_insert_position("d"));
		node.internal_insert_seperator(std::string("f"), 13, node.find_insert_position("f"));

		node.serialize(true);
		node.release(true);

		BPlusTreeNode<K> re(bm, TID, p);
		assert(!re.is_leaf_node());
		assert(re.num_keys == 3);
		assert(re.first_child == 10);

		assert(re.internal_child_for("a") == 10);
		assert(re.internal_child_for("b") == 11);
		assert(re.internal_child_for("c") == 11);
		assert(re.internal_child_for("d") == 12);
		assert(re.internal_child_for("e") == 12);
		assert(re.internal_child_for("f") == 13);
		assert(re.internal_child_for("zzz") == 13);

		re.release(true);
	} else {
		static_assert(!sizeof(K*), "Add a concrete key set for this K.");
	}

	std::cout << "[PASS] Internal<" << typeid(K).name() << "> serialize/deserialize + routing\n";
};

// ---------- Internal:: split behavior -------------
template <typename K> static void test_internal_split(BufferManager& bm, HeapFile& hf) {
	int p_parent = hf.allocate_page();
	int p_sib	 = hf.allocate_page();

	BPlusTreeNode<K> parent =
		BPlusTreeNode<K>::create_new_internal(bm, TID, p_parent, /*parent*/ -1);
	BPlusTreeNode<K> sib = BPlusTreeNode<K>::create_new_internal(bm, TID, p_sib, /*parent*/ -1);

	// build parent with 5 seperators (and 6 children) sp we can test median split
	// left will keep 2, promote 3rd, right keeps last 2

	if constexpr (std::is_same_v<K, int>) {
		parent.first_child = 100; // child0
		parent.internal_insert_seperator(10, 101, parent.find_insert_position(10));
		parent.internal_insert_seperator(20, 102, parent.find_insert_position(20));
		parent.internal_insert_seperator(30, 103, parent.find_insert_position(30));
		parent.internal_insert_seperator(40, 104, parent.find_insert_position(40));
		parent.internal_insert_seperator(50, 105, parent.find_insert_position(50));

		K promote = parent.split_interal_into(sib);

		// promote must be the true median
		assert(promote == 30);

		// Left: keys { 10, 20}, Right: keys { 40, 50} and sib.first_child is child
		// to the right of promote

		assert(parent.internal_entries.size() == 2);
		assert(parent.internal_entries[0].key == 10);
		assert(parent.internal_entries[1].key == 20);

		assert(sib.internal_entries.size() == 2);
		assert(sib.internal_entries[0].key == 40);
		assert(sib.internal_entries[1].key == 50);

		// sib.first_child should be the child immediately right
		// of the promoted key (103)
		assert(sib.first_child == 103);
	} else if constexpr (std::is_same_v<K, std::string>) {
		parent.first_child = 100;
		parent.internal_insert_seperator(std::string("b"), 101, parent.find_insert_position("b"));
		parent.internal_insert_seperator(std::string("d"), 102, parent.find_insert_position("d"));
		parent.internal_insert_seperator(std::string("f"), 103, parent.find_insert_position("f"));
		parent.internal_insert_seperator(std::string("h"), 104, parent.find_insert_position("h"));
		parent.internal_insert_seperator(std::string("j"), 105, parent.find_insert_position("j"));

		K promote = parent.split_interal_into(sib);
		assert(promote == std::string("f"));

		assert(parent.internal_entries.size() == 2);
		assert(parent.internal_entries[0].key == "b");
		assert(parent.internal_entries[1].key == "d");

		assert(sib.internal_entries.size() == 2);
		assert(sib.internal_entries[0].key == "h");
		assert(sib.internal_entries[1].key == "j");

		assert(sib.first_child == 103);
	} else {
		static_assert(!sizeof(K*), "Add a concrete key set for this K.");
	}

	parent.serialize(true);
	sib.serialize(true);

	parent.release(true);
	sib.release(true);

	std::cout << "[PASS] Internal<" << typeid(K).name() << "> split\n";
};

int main() {
	cleanup();

	// minimal storage and BM
	{
		HeapFile hf(TEST_IDX_FILE);

		BufferManager bm(3);

		bm.register_table(TID, &hf);

		// Leaf rountrip: int and string
		{
			std::vector<std::pair<int, RecordId>> ints = {
				{5, rid(1, 1)},
				{1, rid(1, 2)},
				{10, rid(1, 3)},
				{7, rid(1, 4)},
			};
			test_leaf_roundtrip<int>(bm, hf, ints);
		}
		{
			std::vector<std::pair<std::string, RecordId>> strs = {{"delta", rid(2, 1)},
																  {"alpha", rid(2, 2)},
																  {"echo", rid(2, 3)},
																  {"bravo", rid(2, 4)}};
			test_leaf_roundtrip<std::string>(bm, hf, strs);
		}

		// Leaf_split
		test_leaf_split<int>(bm, hf);
		test_leaf_split<std::string>(bm, hf);

		// Internal Roundtrip + routing
		test_internal_roundtrip_and_routing<int>(bm, hf);
		test_internal_roundtrip_and_routing<std::string>(bm, hf);

		// internal Soplit
		test_internal_split<int>(bm, hf);
		test_internal_split<std::string>(bm, hf);
	}

	cleanup();
	std::cout << "[ALL BPlusTreeNode TESTS PASSED]\n";
	return (0);
}
