#ifndef __B_PLUS_TREE_HPP__
#define __B_PLUS_TREE_HPP__

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <filesystem>
#include <fstream>

#include "minidb/IdGenerator.hpp"
#include "minidb/buffer_manager/buffer_manager.hpp"
#include "minidb/catalog.hpp"
#include "minidb/table.hpp"
#include "minidb/table_manager/table_manager.hpp"

template <typename K> size_t serialize_key(uint8_t* buffer, const K& key);

template <typename K> K deserialize_key(const uint8_t* buffer);

// BOOL
template <> inline size_t serialize_key<bool>(uint8_t* buffer, const bool& key) {
	buffer[0] = key ? 1 : 0;
	return (sizeof(bool));
}

template <> inline bool deserialize_key<bool>(const uint8_t* buffer) {
	return buffer[0] != 0;
}

//INT
template <> inline size_t serialize_key<int>(uint8_t* buffer, const int& key) {
	*reinterpret_cast<int*>(buffer) = key;
	return (sizeof(int));
}
template <> inline int deserialize_key<int>(const uint8_t* buffer) {
	return (*reinterpret_cast<const int*>(buffer));
}

/// float
template <> inline size_t serialize_key<float>(uint8_t* buffer, const float& key) {
	*reinterpret_cast<float*>(buffer) = key;
	return (sizeof(float));
}

template <> inline float deserialize_key<float>(const uint8_t* buffer) {
	return (*reinterpret_cast<const float*>(buffer));
}

// std::string
template <> inline size_t serialize_key<std::string>(uint8_t* buffer, const std::string& key) {
	uint16_t len = key.size();

	*reinterpret_cast<uint16_t*>(buffer) = len;
	std::memcpy(buffer + sizeof(uint16_t), key.data(), len);
	return (len + sizeof(uint16_t));
}

template <> inline std::string deserialize_key<std::string>(const uint8_t* buffer) {
	uint16_t len = *reinterpret_cast<const uint16_t*>(buffer);
	return std::string(reinterpret_cast<const char*>(buffer + sizeof(uint16_t)), len);
}

static constexpr size_t BPTREE_NODE_HEADER_BYTES = 11;

template <typename K> struct LeafEntry {
	K key;
	// (page_number, slot_number)
	RecordId rid;
};

template <typename K> struct InternalEntry {
	K key;
	// right child for this seperator key
	int child_page;
};

// ---------- Node on-disk layout ----------
//
// Header (fixed, little endian):
//   [0]     : uint8_t  is_leaf  (1 = leaf, 0 = internal)
//   [1..2]  : uint16_t num_keys
//   [3..6]  : int32_t  parent_page  (-1 if none)
//   [7..10] : int32_t  next_leaf    (valid only if leaf; -1 otherwise)
//   => HEADER_BYTES = 11
//
// Body:
//   Leaf:
//     Repeated num_keys times: [key][int32 page_number][int32 slot_number]
//   Internal:
//     int32 child0
//     Repeated num_keys times: [key][int32 child_i]
//
// Notes:
// - We pack from the beginning (no slotted free space) for simplicity.
// - Variable-length STRING keys are supported.
// - RecordId is 8 bytes (two int32).
// - Child page ids are int32.
// - Capacity check is done by re-serializing in-memory vectors and comparing to PAGE_SIZE.
//
template <typename K> class BPlusTreeNode {
  private:
	BufferManager& bm_;
	PageId		   pid_;
	Page&		   page_;

	// private constr variant (skip deserialize) used by create_new_* helpers
	BPlusTreeNode(BufferManager& bm, int table_id, int page_no, bool /*skipDeseriliaze*/)
		: bm_(bm), pid_{table_id, page_no}, page_(bm_.fetch_page(pid_)) {
		// do not deserialize; caller will populate fields and seriliaze
	}

  public:
	// public state (read after deserialize, update in memory)
	bool	 is_leaf	 = true;
	uint16_t num_keys	 = 0;
	int		 parent_page = -1;
	int		 next_leaf	 = -1; // only meaninfull if leaf

	// Leaf entries OR internal entries + firstChild
	std::vector<LeafEntry<K>>	  leaf_entries;
	int							  first_child = -1; // internal only;
	std::vector<InternalEntry<K>> internal_entries;

	// --- Lifecycle / I/O ---
	BPlusTreeNode(BufferManager& bm, int table_id, int page_no)
		: bm_(bm), pid_{table_id, page_no}, page_(bm_.fetch_page(pid_)) {
		deserialize(); // load from page
	}

	// create a brand new node (empty) on an existing, newly-allocated page
	static BPlusTreeNode<K> create_new_leaf(BufferManager& bm, int table_id, int page_no,
											int parent = -1, int next_leaf = -1) {
		BPlusTreeNode<K> n(bm, table_id, page_no, /*skip Deserialize*/ true);
		n.is_leaf	  = true;
		n.num_keys	  = 0;
		n.parent_page = parent;
		n.next_leaf	  = next_leaf;
		n.leaf_entries.clear();
		n.serialize(/*markDirt*/ true);
		return (n);
	}

	static BPlusTreeNode<K> create_new_internal(BufferManager& bm, int table_id, int page_no,
												int parent = -1, int next_leaf = -1) {
		BPlusTreeNode<K> n(bm, table_id, page_no, /*skipDeserialize*/ true);
		n.is_leaf	  = false;
		n.num_keys	  = 0;
		n.parent_page = parent;
		n.next_leaf	  = -1;
		n.first_child = -1;
		n.internal_entries.clear();
		n.serialize(/*markDirty*/ true);
		return (n);
	}

	int find_insert_position(const K& key) const {
		if (is_leaf) {
			return static_cast<int>(
				std::lower_bound(leaf_entries.begin(), leaf_entries.end(), key,
								 [](const LeafEntry<K>& e, const K& k) { return (e.key < k); }) -
				leaf_entries.begin());
		} else {
			return static_cast<int>(std::upper_bound(internal_entries.begin(),
													 internal_entries.end(), key,
													 [](const K& k, const InternalEntry<K>& e) {
														 return (k < e.key);
													 }) -
									internal_entries.begin());
		}
	}

	void serialize(bool markDirty = true) {
		auto& data = page_.get_data();

		// Header
		data[0] = static_cast<uint8_t>(is_leaf ? 1 : 0);
		std::memcpy(&data[1], &num_keys, sizeof(uint16_t));
		std::memcpy(&data[3], &parent_page, sizeof(int32_t));
		std::memcpy(&data[7], &next_leaf, sizeof(int32_t));

		// Body
		size_t off = BPTREE_NODE_HEADER_BYTES;

		if (is_leaf) {
			for (const auto& e : leaf_entries) {
				off += serialize_key<K>(data.data() + off, e.key);
				std::memcpy(&data[off], &e.rid.page_number, sizeof(int32_t));
				off += sizeof(int32_t);
				std::memcpy(&data[off], &e.rid.slot_number, sizeof(int32_t));
				off += sizeof(int32_t);
			}
		} else {
			std::memcpy(&data[off], &first_child, sizeof(int32_t));
			off += sizeof(int32_t);
			for (const auto& e : internal_entries) {
				off += serialize_key<K>(data.data() + off, e.key);
				std::memcpy(&data[off], &e.child_page, sizeof(int32_t));
				off += sizeof(int32_t);
			}
		}
		// Zero the remainder for cleanliness (optional)
		if (off < PAGE_SIZE)
			std::memset(data.data() + off, 0, PAGE_SIZE - off);
		// if (markDirty)
		// 	bm_.unpin_page(pid_, /*isDirty*/ true);
		// // repin immediatly for future ops
		// ///////////////////////////////////WHY?????///////////////////////
		// page_ = bm_.fetch_page(pid_);
	}

	void release(bool dirty) { bm_.unpin_page(pid_, dirty); }

	// ---- leaf operations -----
	// Returns true if overflow after insertion (caller may then split)
	bool leaf_insert(const K& key, const RecordId& rid) {
		int pos = find_insert_position(key);

		leaf_entries.insert(leaf_entries.begin() + pos, LeafEntry<K>{key, rid});
		num_keys = static_cast<uint16_t>(leaf_entries.size());

		// check capacity by a dry-run serialize into a temp buffer
		return (encoded_size() > PAGE_SIZE);
	}

	// Split leaf into 'sibling' (caller must provide a new empty leaf node
	// on a fresh page
	// we move the upper half into sibling, update next_leaf links, and return
	// the split key (first key in sibling)
	K split_leaf_into(BPlusTreeNode<K>& sibling) {
		size_t mid = leaf_entries.size() / 2;

		sibling.leaf_entries.assign(leaf_entries.begin() + mid, leaf_entries.end());
		leaf_entries.erase(leaf_entries.begin() + mid, leaf_entries.end());

		// FIX headers
		this->num_keys		= static_cast<uint16_t>(leaf_entries.size());
		sibling.num_keys	= static_cast<uint16_t>(sibling.leaf_entries.size());
		sibling.parent_page = this->parent_page;

		// Link leaves
		sibling.next_leaf = this->next_leaf;
		this->next_leaf	  = sibling.pid_.page_number;

		// Split key (first key of sibling)
		return (sibling.leaf_entries.front().key);
	}

	// ---------- Internal operations -----------
	// Given a search key, return the child page to descend to
	int internal_child_for(const K& k) const {
		int idx = find_insert_position(k);
		if (idx == 0)
			return (first_child);
		return (internal_entries[idx - 1].child_page);
	}

	// Leftmost child that may contain k. Duplicates of a separator key can sit
	// in the left sibling after a split, so range scans must start there.
	int internal_leftmost_child_for(const K& k) const {
		int idx = static_cast<int>(std::lower_bound(internal_entries.begin(),
													internal_entries.end(), k,
													[](const InternalEntry<K>& e, const K& key) {
														return (e.key < key);
													}) -
								   internal_entries.begin());
		if (idx == 0)
			return (first_child);
		return (internal_entries[idx - 1].child_page);
	}

	K split_interal_into(BPlusTreeNode<K>& sibling) {
		// Median by count
		size_t midIdx  = internal_entries.size() / 2;
		K	   promote = internal_entries[midIdx].key;

		// determine sibling's first_child = child to the right of promote
		int rightOfPromote = internal_entries[midIdx].child_page;

		// left keeps [0...midIdx -1]
		std::vector<InternalEntry<K>> leftEntries(internal_entries.begin(),
												  internal_entries.begin() +
													  midIdx); // right keeps [midIdx + 1 .. end]
		std::vector<InternalEntry<K>> rightEntries(internal_entries.begin() + midIdx + 1,
												   internal_entries.end());

		// Rebuild current (left)
		this->internal_entries = std::move(leftEntries);
		this->num_keys		   = static_cast<uint16_t>(internal_entries.size());

		// build sibling (right)
		sibling.is_leaf			 = false;
		sibling.parent_page		 = this->parent_page;
		sibling.internal_entries = std::move(rightEntries);
		sibling.first_child		 = rightOfPromote;

		return promote;
	}

	// Insert seperator (key, rightChild) at position pos;
	// firstChild may need to be set for very first insert
	// we assume caller already inserted/handled left side in parent
	// context
	// return true if overflow
	bool internal_insert_seperator(const K& key, int right_child, int pos) {
		internal_entries.insert(internal_entries.begin() + pos, InternalEntry<K>{key, right_child});
		num_keys = static_cast<uint16_t>(internal_entries.size());
		return encoded_size() > PAGE_SIZE;
	}

	size_t encoded_size() const {
		size_t off = BPTREE_NODE_HEADER_BYTES;
		if (is_leaf) {
			for (const auto& e : leaf_entries) {
				off += key_encoded_size(e.key);
				off += sizeof(int32_t) * 2;
			}
		} else {
			off += sizeof(int32_t);
			for (const auto& e : internal_entries) {
				off += key_encoded_size(e.key);
				off += sizeof(int32_t);
			}
		}
		return (off);
	}

	static size_t key_encoded_size(const K& k) {
		if constexpr (std::is_same_v<K, std::string>) {
			return (sizeof(uint16_t) + k.size());
		}
		return (sizeof(K));
	}
	// Re-read from page(useful after splits by someone else)
	void deserialize() {
		auto& data = page_.get_data();
		// data.size() should alwasy be == PAGE_SIZE

		is_leaf = (data[0] != 0);
		std::memcpy(&num_keys, &data[1], sizeof(uint16_t));
		std::memcpy(&parent_page, &data[3], sizeof(int32_t));
		std::memcpy(&next_leaf, &data[7], sizeof(int32_t));

		leaf_entries.clear();
		internal_entries.clear();

		first_child = -1;

		size_t off = BPTREE_NODE_HEADER_BYTES;

		if (is_leaf) {
			leaf_entries.reserve(num_keys);
			for (uint16_t i = 0; i < num_keys; ++i) {
				K key = deserialize_key<K>(data.data() + off);
				off += key_size_at(data.data(), off);

				RecordId rid{};
				std::memcpy(&rid.page_number, &data[off], sizeof(int32_t));
				off += sizeof(int32_t);
				std::memcpy(&rid.slot_number, &data[off], sizeof(int32_t));
				off += sizeof(int32_t);

				leaf_entries.push_back(LeafEntry<K>{std::move(key), rid});
			}
		} else {
			// read first child
			std::memcpy(&first_child, &data[off], sizeof(int32_t));
			off += sizeof(int32_t);
			internal_entries.reserve(num_keys);
			for (uint16_t i = 0; i < num_keys; ++i) {
				K key = deserialize_key<K>(data.data() + off);
				off += key_size_at(data.data(), off);
				int child;
				std::memcpy(&child, &data[off], sizeof(int32_t));
				off += sizeof(int32_t);
				internal_entries.push_back(InternalEntry<K>{std::move(key), child});
			}
		}
	}

	static size_t key_size_peek(const uint8_t* src) {
		if constexpr (std::is_same_v<K, std::string>) {
			uint16_t len;
			std::memcpy(&len, src, sizeof(uint16_t));
			return sizeof(uint16_t) + len;
		} else {
			return sizeof(K);
		}
	}

	size_t key_size_at(const uint8_t* base, size_t off) const { return key_size_peek(base + off); }

	int page_number() const { return pid_.page_number; }

	bool is_leaf_node() const { return is_leaf; }

	bool is_deficient(size_t min_keys) const { return num_keys < min_keys; }

	bool can_lend(size_t minKeys) const { return num_keys > minKeys; }

	K first_key() const {
		if (is_leaf_node()) {
			return leaf_entries.front().key;
		} else {
			return internal_entries.front().key;
		}
	}
};

template <typename K> class BPlusTree {
  private:
	BufferManager&			 bm_;
	int						 table_id_;
	int						 root_page_;
	std::function<void(int)> on_root_changed_; // callback

	// helpers
	BPlusTreeNode<K> load_node(int page_no) { return BPlusTreeNode<K>(bm_, table_id_, page_no); }
	int				 find_leaf_page(const K& key) {
		 if (root_page_ == -1)
			 return (-1);

		 int current = root_page_;
		 while (true) {
			 BPlusTreeNode<K> node = load_node(current);
			 if (node.is_leaf_node()) {
				 node.release(false);
				 return (current);
			 } else {
				 current = node.internal_child_for(key);
				 node.release(false); // unpin when moving down
			 }
		 }
	}

	BPlusTreeNode<K> find_leaf(const K& k) {
		int page_id = find_leaf_page(k);
		return load_node(page_id);
	}

	void insert_in_parent(BPlusTreeNode<K>& left, const K& key, BPlusTreeNode<K>& right) {
		if (left.parent_page == -1) {
			// ------case 1: create new root --------------
			int	 newRootPid	 = bm_.allocate_page(table_id_);
			auto root		 = BPlusTreeNode<K>::create_new_internal(bm_, table_id_, newRootPid);
			root.first_child = left.page_number();
			root.internal_entries.push_back({key, right.page_number()});
			root.num_keys = 1;

			root.serialize(true);

			left.parent_page  = newRootPid;
			right.parent_page = newRootPid;

			left.serialize(true);
			right.serialize(true);

			root_page_ = newRootPid;
			if (on_root_changed_) {
				on_root_changed_(root_page_);
			}

			root.release(true);
			return;
		}

		// ---- Caee 2; insert into existing prent ----
		BPlusTreeNode<K> parent(bm_, table_id_, left.parent_page);

		int	 pos	  = parent.find_insert_position(key);
		bool overflow = parent.internal_insert_seperator(key, right.page_number(), pos);

		if (!overflow) {
			parent.serialize(true);
			parent.release(true);
			return;
		}

		// ---- Case 3: parent overflows --> split ------
		int	 sibling_pid = bm_.allocate_page(table_id_);
		auto sibling =
			BPlusTreeNode<K>::create_new_internal(bm_, table_id_, sibling_pid, parent.parent_page);

		K promote = parent.split_interal_into(sibling);
		parent.serialize(true);
		sibling.serialize(true);

		// update children's parent pointers for sibling
		BPlusTreeNode<K> child(bm_, table_id_, sibling.first_child);
		child.parent_page = sibling_pid;
		child.serialize(true);
		child.release(true);

		for (auto& entry : sibling.internal_entries) {
			BPlusTreeNode<K> c(bm_, table_id_, entry.child_page);
			c.parent_page = sibling_pid;
			c.serialize(true);
			c.release(true);
		}

		insert_in_parent(parent, promote, sibling);
		parent.release(true);
		sibling.release(true);
	}

  public:
	BPlusTree(BufferManager& bm, int table_id, int root_page = -1,
			  std::function<void(int)> root_cb = {})
		: bm_(bm),
		  table_id_(table_id),
		  root_page_(root_page),
		  on_root_changed_(std::move(root_cb)) {}

	void insert(const K& key, const RecordId& rid) {
		// Case 1: empty tree
		if (root_page_ == -1) {
			HeapFile* file	   = this->bm_.get_heap_file(table_id_);
			int		  root_pid = file->allocate_page();
			auto	  root	   = BPlusTreeNode<K>::create_new_leaf(bm_, table_id_, root_pid);
			root.leaf_insert(key, rid);
			root.serialize(true);
			root.release(true);
			root_page_ = root_pid;
			if (on_root_changed_) {
				on_root_changed_(root_page_);
			}
			return;
		}

		// case 2: non-empty tree
		int current_pid = root_page_;

		// --- traverse until leaf ----
		while (true) {
			BPlusTreeNode<K> node(bm_, table_id_, current_pid);
			bool			 is_leaf = node.is_leaf;
			if (!is_leaf)
				current_pid = node.internal_child_for(key);
			node.release(false);
			if (is_leaf)
				break;
		}
		// ----insert into leaf ----
		BPlusTreeNode<K> leaf(bm_, table_id_, current_pid);
		bool			 overflow = leaf.leaf_insert(key, rid);

		// an overflowing node does not fit in its page: serialize only after the split
		if (!overflow) {
			leaf.serialize(true);
			leaf.release(true);
			return;
		}

		// --- handle leaf split -----
		HeapFile* file		  = bm_.get_heap_file(table_id_);
		int		  sibling_pid = file->allocate_page();
		auto	  sibling =
			BPlusTreeNode<K>::create_new_leaf(bm_, table_id_, sibling_pid, leaf.parent_page);

		K promote = leaf.split_leaf_into(sibling);

		leaf.serialize(true);
		sibling.serialize(true);

		// push promoted key upward
		insert_in_parent(leaf, promote, sibling);
		leaf.release(true);
		sibling.release(true);
	}
	std::optional<RecordId> search(const K& key) {
		if (root_page_ == -1) {
			return (std::nullopt);
		}
		int current = root_page_;

		while (true) {
			BPlusTreeNode<K> node(bm_, table_id_, current);
			if (node.is_leaf) {
				auto it =
					std::lower_bound(node.leaf_entries.begin(), node.leaf_entries.end(), key,
									 [](const LeafEntry<K>& e, const K& k) { return e.key < k; });
				std::optional<RecordId> found;
				if (it != node.leaf_entries.end() && !(key < it->key)) {
					// found exact match
					found = it->rid;
				}
				node.release(false);
				return (found);
			} else {
				current = node.internal_child_for(key);
			}
			node.release(false);
		}
		return (std::nullopt);
	}
	std::vector<RecordId> range_search(const K& low, const K& high) {
		std::vector<RecordId> results;

		if (root_page_ == -1) {
			return (results);
		}

		int current = root_page_;

		// 1. Find leaf containing low
		while (true) {
			BPlusTreeNode<K>* node = new BPlusTreeNode<K>(bm_, table_id_, current);

			if (node->is_leaf) {
				// find first key >= low
				auto it =
					std::lower_bound(node->leaf_entries.begin(), node->leaf_entries.end(), low,
									 [](const LeafEntry<K>& e, const K& k) { return e.key < k; });

				// 2. Scan accross leaves
				while (true) {
					for (; it != node->leaf_entries.end(); ++it) {
						if (it->key > high) {
							node->release(false);
							delete node;
							return (results); // stop at upper bound
						}
						if (!(it->key < low)) { // key >= low
							results.push_back(it->rid);
						}
					}

					if (node->next_leaf == -1) {
						node->release(false);
						delete node;
						return results; // no more leaves
					}
					// move to the mext leaf

					current = node->next_leaf;
					node->release(false); // unpin current before fetching next
					delete node;
					node = new BPlusTreeNode<K>(bm_, table_id_, current);
					it	 = node->leaf_entries.begin();
				}
			} else {
				current = node->internal_leftmost_child_for(low);
				node->release(false);
				delete node;
			}
		}
		return (results);
	}

	int get_root_page() const { return (root_page_); }
};

struct IndexKey {
	std::string table;
	std::string column;

	bool operator==(const IndexKey& other) const {
		return table == other.table && column == other.column;
	}
};

namespace std {
	template <> struct hash<IndexKey> {
		size_t operator()(const IndexKey& key) const noexcept {
			size_t h1 = std::hash<std::string>()(key.table);
			size_t h2 = std::hash<std::string>()(key.column);

			return h1 ^ (h2 << 1);
		}
	};
}; // namespace std

class IndexManager {
  private:
	BufferManager&	bm_;
	CatalogManager& catalog_;
	std::string		index_dir_;
	IdGenerator		id_gen;

	std::unordered_map<IndexKey, IndexInfo> indices;

	// private helpers
	template <typename K>
	BPlusTree<K> openIndex(const std::string& table, const std::string& column) {
		// assume the index is in indices
		// we just find it and return a B+ tree from it

		IndexKey key{table, column};
		auto	 it = indices.find(key);
		if (it == indices.end()) {
			throw std::runtime_error("index not found: " + table + "." + column);
		}

		// Capture key/table by value: the local iterator must not outlive this call.
		return BPlusTree<K>(bm_, it->second.index_id, it->second.root_page,
							[this, key, table](int new_root) {
								auto found = indices.find(key);
								if (found == indices.end())
									return;
								found->second.root_page = new_root;
								catalog_.updateIndexRoot(table, found->second.name, new_root);
							});
	}

  public:
	IndexManager(BufferManager& bm, CatalogManager& catalog, const std::string& index_dir)
		: bm_(bm), catalog_(catalog), index_dir_(index_dir) {}

	// p

	void load_indices(const std::vector<IndexInfo>& idxlist) {
		// we assume some indices are already created
		// we just save them in indices

		for (auto& idxinfo : idxlist) {
			std::string idx_file = idxinfo.name + "_" + idxinfo.column + ".idx";
			std::string path	 = index_dir_ + "/" + idx_file;
			int			idx_id	 = id_gen();

			bm_.register_table(idx_id, new HeapFile(path));

			IndexKey key{idxinfo.name, idxinfo.column};

			IndexInfo tmp(idxinfo);
			tmp.index_id = idx_id;

			indices[key] = tmp;
		}
	}
	void create_index(const std::string& table, const std::string& column) {
		// creating a new empty index
		std::string idx_file = table + "_" + column + ".idx";
		std::string path	 = index_dir_ + "/" + idx_file;

		// Create empty file
		{ std::ofstream out(path, std::ios::binary | std::ios::trunc); }

		int index_id = id_gen();

		bm_.register_table(index_id, new HeapFile(path));

		IndexKey  key{table, column};
		IndexInfo idx(table,	// table name
					  column,	// colum name
					  idx_file, // file name
					  -1,		// root page (-1 not nood yet)
					  index_id	// index id
		);
		indices[key] = idx;

		// in catalog now
		catalog_.createIndex(table, idx);
	}
	void drop_index(const std::string& table, const std::string& column) {
		// removing from indices
		// droping from catalog
		// unregister in buffer manager
		catalog_.dropIndex(table, column);

		IndexKey key{table, column};
		std::filesystem::remove(index_dir_ + "/" + indices[key].file_path);
		indices.erase(key);
	}

	template <typename K>
	void insert(const std::string& table, const std::string& column, const K& key,
				const RecordId& rid) {
		auto tree = openIndex<K>(table, column);
		tree.insert(key, rid);
	}

	template <typename K>
	std::optional<RecordId> search(const std::string& table, const std::string& column,
								   const K& key) {
		auto tree = openIndex<K>(table, column);
		return (tree.search(key));
	}

	template <typename K>
	std::vector<RecordId> range_search(const std::string& table, const std::string& column,
									   const K& low, const K& high) {
		auto tree = openIndex<K>(table, column);

		return (tree.range_search(low, high));
	}
};

#endif
