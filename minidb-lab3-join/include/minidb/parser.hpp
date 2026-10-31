#ifndef __PARSER_HPP__
#define __PARSER_HPP__

#include <memory>
#include <string>
#include <vector>

#include "query.hpp"

/**
 * @class Parser
 * @brief Responsible for parsing SQL strings into internal query representations.
 *
 * The Parser class provides methods to interpret and convert SQL query strings into
 * structured Query objects such as SELECT, INSERT, CREATE TABLE, and CREATE INDEX.
 * It includes internal helpers for tokenizing, parsing expressions and conditions, and
 * interpreting SQL components like values and data types.
 */
class Parser {
  public:
	Parser() = default;

	// Parse a SQL string and return a Query object
	std::unique_ptr<Query> parse(const std::string& sql);

  private:
	// Helper methods for parsing different query types
	std::unique_ptr<SelectQuery>	  parse_select(const std::string& sql);
	std::unique_ptr<InsertQuery>	  parse_insert(const std::string& sql);
	std::unique_ptr<CreateTableQuery> parse_create_table(const std::string& sql);
	std::unique_ptr<CreateIndexQuery> parse_create_index(const std::string& sql);

	// Parse "JOIN <table> ON <table>.<col> = <table>.<col>"
	Join parse_join(const std::string& clause);

	// Tokenize SQL string
	std::vector<std::string> tokenize(const std::string& sql);

	// Parse conditions (WHERE clauses)
	std::unique_ptr<Condition> parse_condition(const std::vector<std::string>& tokens, size_t& pos);

	// Parse expressions
	std::unique_ptr<Expression> parse_expression(const std::vector<std::string>& tokens,
												 size_t&						 pos);

	// Convert string to value
	Value parse_value(const std::string& token);

	// Parse data type from string
	DataType parse_data_type(const std::string& type_str);
};

#endif
