#include "minidb/semantic_verifier.hpp"

#include "minidb/catalog.hpp"
#include "minidb/query.hpp"
#include "minidb/table.hpp"

#include <algorithm>
#include <stdexcept>

SemanticVerifier::SemanticVerifier(const CatalogManager& cat) : catalog(cat) {
}

void SemanticVerifier::verify_insert_query(const InsertQuery& q) const {
	// 1- TABLE existance check
	auto tinfo_opt = catalog.getTable(q.table_name());
	if (!tinfo_opt)
		throw std::runtime_error("UNKNOWN TABLE");

	const auto&							 tinfo		   = tinfo_opt.value();
	const std::vector<ColumnDefinition>& table_columns = tinfo.columns;

	// 2- Columns and values count match
	if (q.columns().size() != q.values().size()) {
		throw std::runtime_error("COLUMNS AND VALUES UNMATCH");
	}

	// 3- Columns count unmatch schema columns count
	if (q.columns().size() != table_columns.size()) {
		throw std::runtime_error("SCHEMA UNMATCH");
	}

	// 4- Column existance check
	// 5. Type Check
	for (size_t i = 0; i < q.columns().size(); i++) {
		const std::string& col_name = q.columns()[i];
		const std::string& raw_val	= std::get<std::string>(q.values()[i]);

		auto it = std::find_if(table_columns.begin(), table_columns.end(),
							   [&](const ColumnDefinition& col) { return col.name == col_name; });
		if (it == table_columns.end()) {
			throw std::runtime_error("UNKNOWN COLUMN: " + col_name);
		}

		const ColumnDefinition& col_def = *it;

		auto converted = convert_string_to_type(raw_val, col_def.type_);
		if (!converted.has_value()) {
			throw std::runtime_error("TYPE MISMATCH on COLUMN: " + col_name);
		}
	}
}

void SemanticVerifier::verify_select_query(const SelectQuery& q) const {
	// 1- TABLE existance check
	auto table_info_opt = catalog.getTable(q.from());
	if (!table_info_opt)
		throw std::runtime_error("UNKNOWN TABLE");

	const auto&							 tinfo		   = table_info_opt.value();
	const std::vector<ColumnDefinition>& table_columns = tinfo.columns;

	if (q.columns().size() > table_columns.size()) {
		throw std::runtime_error("UNKNOWN COLUMNS IN SELECT");
	}

	// columns check
	for (size_t i = 0; i < q.columns().size(); i++) {
		const std::string& col_name = q.columns()[i];

		auto it = std::find_if(table_columns.begin(), table_columns.end(),
							   [&](const ColumnDefinition& col) { return col.name == col_name; });
		if (it == table_columns.end()) {
			throw std::runtime_error("UNKNOWN COLUMN: " + col_name);
		}
	}
}

static const ColumnDefinition* find_column(const TableInfo& tinfo, const std::string& name) {
	for (const auto& col : tinfo.columns) {
		if (col.name == name)
			return &col;
	}
	return nullptr;
}

void SemanticVerifier::verify_join_query(const SelectQuery& q) const {
	// TODO (Lab 3): validate the JOIN against the catalog. Throw std::runtime_error when:
	//  1. there is not exactly one join, or it is not JoinType::INNER
	//  2. q.from() or the joined table is unknown, or both are the same table
	//  3. the ON condition does not compare one column of each table
	//  4. an ON column does not exist in its table
	//  5. the two join columns have different types
	//  6. a selected column is unknown, refers to another table, or a bare
	//     column name exists in both tables (ambiguous)
	// find_column() above may help.


	if (q.joins().size()!=1) throw std::runtime_error("Multiple JOINs detected");
	if (q.joins()[0].type != JoinType::INNER) throw std::runtime_error("Only INNER JOINS are supported");

	if (q.from() == q.joins()[0].table_name) throw std::runtime_error("Doing a self join!");
	if (!catalog.getTable(q.from()).has_value() || !catalog.getTable(q.joins()[0].table_name).has_value()) throw std::runtime_error("Existence of an Unknown table!");


	const JoinCondition& condition = q.joins()[0].condition;

	std::string left_table = condition.left_table;
	std::string right_table = condition.right_table;
	std::string left_column = condition.left_column;

	std::string right_column = condition.right_column;

	if (!((left_table ==q.from() && right_table == q.joins()[0].table_name) ||
		  (left_table == q.joins()[0].table_name && right_table == q.from()))) {
		throw std::runtime_error("Invalid JOIN condition");}




	auto table1 = catalog.getTable(left_table).value();
	auto table2 = catalog.getTable(right_table).value();

	auto col1 = find_column(table1, left_column);
	auto col2 = find_column(table2, right_column);

	if (!col1 || !col2) {
		throw std::runtime_error("Unknown JOIN column");
	}

	if (col1->type_ != col2->type_) throw std::runtime_error("Differents column types");

	//checking the structure of the columns

	for (const auto& column : q.columns()) {

		if (column == "*") continue;
		int pos = column.find('.');
		if (pos != std::string::npos) {

			std::string table_name = column.substr(0, pos);
			std::string column_name = column.substr(pos + 1);

			if (table_name != q.from() && table_name != q.joins()[0].table_name) {
				throw std::runtime_error("Unknown SELECT table");
			}

			auto table = catalog.getTable(table_name).value();
			if (!find_column(table, column_name)) {
				throw std::runtime_error("Unknown SELECT column");
			}

		} else {
			auto c1 = find_column(table1, column);
			auto c2 = find_column(table2, column);

			if (!c1 && !c2) throw std::runtime_error("Unknown SELECT column");


			if (c1 && c2) {
				throw std::runtime_error("Ambiguous SELECT column");
			}
		}
	}


}

void SemanticVerifier::verify_create_index(const CreateIndexQuery& q) const {
	auto table_info_opt = catalog.getTable(q.table_name());
	if (!table_info_opt) {
		throw std::runtime_error("UNKNOWN TABLE: " + q.table_name());
	}
	const auto& tinfo = table_info_opt.value();

	// 1. Check if column exists
	bool column_found = false;
	for (const auto& col : tinfo.columns) {
		if (col.name == q.column_name()) {
			column_found = true;
			break;
		}
	}
	if (!column_found) {
		throw std::runtime_error("UNKNOWN COLUMN: " + q.column_name() + " in table " +
								 q.table_name());
	}

	// 2. Check if index already exists on this column
	for (const auto& idx : tinfo.indexes) {
		if (idx.column == q.column_name()) {
			throw std::runtime_error("INDEX ALREADY EXISTS on column " + q.column_name() +
									 " of table " + q.table_name());
		}
	}
}
