#ifndef __QUERY_RESULT_HPP__
#define __QUERY_RESULT_HPP__

#include <iostream>
#include <string>
#include <vector>

/**
 * @struct ColumnInfo
 * @brief Represents metadata about a column in a database table.
 *
 * Stores the name of the column.
 */
struct ColumnInfo {
	std::string name;

	ColumnInfo(const std::string& n) : name(n) {}
};

/**
 * @class QueryResult
 * @brief Represents the result of executing a database query.
 *
 * Handles results from different types of queries:
 * - DML (INSERT, UPDATE, DELETE) with affected rows count
 * - DDL (CREATE TABLE, CREATE INDEX) with success status and message
 * - SELECT queries with columns and rows of data
 */
class QueryResult {
  public:
	// For DML operations (INSERT, UPDATE, DELETE)
	QueryResult(int affected_rows);

	// For DDL operations (CREATE TABLE, CREATE INDEX)
	QueryResult(bool success, const std::string& message);

	// For query operations (SELECT)
	QueryResult(const std::vector<std::string>& columns);

	// Add a row to the result set
	void add_row(const std::vector<std::string>& row);

	// Get the number of rows in the result
	size_t row_count() const { return rows_.size(); }

	// Get the columns in the result
	const std::vector<std::string>& columns() const { return columns_; }

	// Get the rows in the result
	const std::vector<std::vector<std::string>>& rows() const { return rows_; }

	// Get the number of affected rows (for DML operations)
	int affected_rows() const { return affected_rows_; }

	// Get the success flag (for DDL operations)
	bool success() const { return success_; }

	// Get the message (for DDL operations)
	const std::string& message() const { return message_; }

	std::string operator()() const;

  private:
	std::vector<std::string>			  columns_;
	std::vector<std::vector<std::string>> rows_;
	int									  affected_rows_;
	bool								  success_;
	std::string							  message_;
	bool								  is_query_result_;
};

#endif
