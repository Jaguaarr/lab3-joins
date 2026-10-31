#include "minidb/query_result.hpp"

#include <iomanip>
#include <sstream>

// Constructor for DML operations
QueryResult::QueryResult(int affected_rows)
	: affected_rows_(affected_rows), success_(true), message_(""), is_query_result_(false) {
}

// Constructor for DDL operations
QueryResult::QueryResult(bool success, const std::string& message)
	: affected_rows_(0), success_(success), message_(message), is_query_result_(false) {
}

// Constructor for query operations
QueryResult::QueryResult(const std::vector<std::string>& columns)
	: columns_(columns), affected_rows_(0), success_(true), message_(""), is_query_result_(true) {
}

// Add a row to the result set
void QueryResult::add_row(const std::vector<std::string>& row) {
	rows_.push_back(row);
}

// Print the result to the given output stream
std::string QueryResult::operator()() const {
	std::stringstream oss;

	if (is_query_result_) {
		// Print query results
		if (rows_.empty()) {
			oss << "Empty set (0 rows)" << std::endl;
			return (oss.str());
		}

		// Calculate column widths
		std::vector<size_t> col_widths(columns_.size());
		for (size_t i = 0; i < columns_.size(); ++i) {
			col_widths[i] = columns_[i].length();
		}

		// Update column widths based on data
		for (const auto& row : rows_) {
			for (size_t i = 0; i < row.size() && i < col_widths.size(); ++i) {
				size_t value_width = row[i].length();
				if (value_width > col_widths[i]) {
					col_widths[i] = value_width;
				}
			}
		}

		// Print header
		for (size_t i = 0; i < columns_.size(); ++i) {
			oss << "| " << std::left << std::setw(col_widths[i]) << columns_[i] << " ";
		}
		oss << "|" << std::endl;

		// Print separator
		for (size_t i = 0; i < columns_.size(); ++i) {
			oss << "+-" << std::string(col_widths[i], '-') << "-";
		}
		oss << "+" << std::endl;

		// Print rows
		for (const auto& row : rows_) {
			for (size_t i = 0; i < row.size() && i < columns_.size(); ++i) {
				oss << "| " << std::left << std::setw(col_widths[i]) << row[i] << " ";
			}
			oss << "|" << std::endl;
		}

		// Print row count
		oss << rows_.size() << " rows in set" << std::endl;
	} else if (affected_rows_ > 0) {
		// Print DML results
		oss << "Query OK, " << affected_rows_ << " rows affected" << std::endl;
	} else {
		// Print DDL results
		if (success_) {
			oss << "Query OK";
			if (!message_.empty()) {
				oss << ", " << message_;
			}
			oss << std::endl;
		} else {
			oss << "Query failed";
			if (!message_.empty()) {
				oss << ": " << message_;
			}
			oss << std::endl;
		}
	}
	return (oss.str());
}
