#ifndef __TABLE_HPP__
#define __TABLE_HPP__

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <iostream>

class BufferManager;

enum DataType {
	INT = 1,
	FLOAT,
	STRING,
	BOOL,
};

std::string DataTypeToString(DataType dt);

DataType StringToDataType(const std::string& t);

// Value can hold different types
using Value = std::variant<int, double, std::string, bool>;

std::optional<Value> convert_string_to_type(const std::string& str, DataType type);

// a row is a vector of values to support null
using Row = std::vector<std::optional<Value>>;

struct ColumnDefinition {
	std::string name;
	DataType	type_;
	bool		not_null;
	bool		primary_key;

	ColumnDefinition(const std::string& n, DataType t, bool nn = false, bool pk = false)
		: name(n), type_(t), not_null(nn), primary_key(pk) {}

	void print() const { std::cout << name << " " << type_ << std::endl; }
};

struct IndexInfo {
	std::string name;	   // table name
	std::string column;	   // column name
	std::string file_path; // file where the B+ tree lives
	int			root_page; // root of B+ tree
	int			index_id;  // optional, only used in run_time

	IndexInfo() : name(""), column(""), file_path(""), root_page(-1), index_id(-1) {}
	IndexInfo(const std::string& n, const std::string& col, const std::string& fpath, int root = -1,
			  int index_id_ = -1)
		: name(n), column(col), file_path(fpath), root_page(root), index_id(index_id_) {}

	IndexInfo(const IndexInfo& other) {
		name	  = other.name;
		column	  = other.column;
		file_path = other.file_path;
		root_page = other.root_page;
		index_id  = other.index_id;
	}
};

struct TableInfo {
	std::string					  name;
	std::vector<ColumnDefinition> columns;
	std::vector<IndexInfo>		  indexes;
};

#endif
