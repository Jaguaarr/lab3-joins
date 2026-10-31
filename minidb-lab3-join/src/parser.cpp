#include "minidb/parser.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
using namespace std;
std::unique_ptr<Query> Parser::parse(const std::string& sql) {
	// Convert to uppercase for case-insensitive matching
	std::string upper_sql = sql;
	std::transform(upper_sql.begin(), upper_sql.end(), upper_sql.begin(), ::toupper);

	// Simple parsing based on first keyword
	if (upper_sql.find("SELECT") == 0) {
		return parse_select(sql);
	} else if (upper_sql.find("INSERT") == 0) {
		return parse_insert(sql);
	} else if (upper_sql.find("CREATE TABLE") == 0) {
		return parse_create_table(sql);
	} else if (upper_sql.find("CREATE INDEX") == 0) {
		return parse_create_index(sql);
	} else {
		throw std::runtime_error("Unsupported SQL statement: " + sql);
	}
}

std::unique_ptr<SelectQuery> Parser::parse_select(const std::string& sql) {
	// This is a simplified parser for educational purposes
	// A real SQL parser would be much more complex

	// Simple parsing for "SELECT column1, column2 FROM table"
	size_t select_pos = sql.find("SELECT");
	size_t from_pos	  = sql.find("FROM");

	if (select_pos == std::string::npos || from_pos == std::string::npos) {
		throw std::runtime_error("Invalid SELECT statement: " + sql);
	}

	// Extract column list
	std::string				 columns_str = sql.substr(select_pos + 6, from_pos - select_pos - 6);
	std::vector<std::string> columns;

	// Parse comma-separated columns
	std::stringstream ss(columns_str);
	std::string		  column;
	while (std::getline(ss, column, ',')) {
		// Trim whitespace
		column.erase(0, column.find_first_not_of(" \t"));
		column.erase(column.find_last_not_of(" \t") + 1);

		if (!column.empty()) {
			columns.push_back(column);
		}
	}

	// Extract table name
	std::string table_str = sql.substr(from_pos + 4);
	size_t		where_pos = table_str.find("WHERE");

	std::string table_name;
	if (where_pos != std::string::npos) {
		table_name = table_str.substr(0, where_pos);
	} else {
		table_name = table_str;
	}

	// Split off an optional "[INNER] JOIN <table> ON <cond>" clause
	std::string upper_table = table_name;
	std::transform(upper_table.begin(), upper_table.end(), upper_table.begin(), ::toupper);
	size_t		join_pos = upper_table.find(" JOIN ");
	std::string join_clause;
	if (join_pos != std::string::npos) {
		join_clause = table_name.substr(join_pos + 1);
		table_name	= table_name.substr(0, join_pos);

		upper_table = upper_table.substr(0, join_pos);
		upper_table.erase(upper_table.find_last_not_of(" \t") + 1);
		if (upper_table.size() >= 6 && upper_table.compare(upper_table.size() - 6, 6, " INNER") == 0) {
			table_name = table_name.substr(0, upper_table.size() - 6);
		}
	}

	// Trim whitespace
	table_name.erase(0, table_name.find_first_not_of(" \t"));
	table_name.erase(table_name.find_last_not_of(" \t;") + 1);

	// Create the query object
	auto query = std::make_unique<SelectQuery>(columns, table_name);

	if (!join_clause.empty()) {
		query->add_join(parse_join(join_clause));
	}

	// TODO: Parse WHERE, ORDER BY, etc.

	return query;
}

Join Parser::parse_join(const std::string& clause) {
	// TODO (Lab 3): parse "JOIN <table> ON <table>.<col> =	 <table>.<col>[;]"
	//  - tokenize(clause) splits on spaces and '='; strip a trailing ';'

    vector<string> tokens = tokenize(clause);

    string table;
    string left_column;
    string right_column;

    int join_index = find(tokens.begin(), tokens.end(), "JOIN") - tokens.begin();

    if (join_index == tokens.size()) {
        throw std::runtime_error("JOIN keyword not found");}



    if (tokens.size() != join_index + 6) {
        throw std::runtime_error("Invalid JOIN syntax");}

    if (tokens[join_index + 2] != "ON" && tokens[join_index + 2] != "on") {
        throw std::runtime_error("ON keyword expected");
    }

    if (tokens[join_index + 4] != "=") {
        throw std::runtime_error("= expected");
    }

    table = tokens[join_index + 1];
    left_column = tokens[join_index + 3];
    right_column = tokens[join_index + 5];

    vector<string> table_column;
    string token = "";

    for (char c: left_column) {
        if (c == '.') {
            if (token != ""){
                table_column.push_back(token);
                token="";
            }
        }
        else {
            token += c;
        }
    }
    table_column.push_back(token);
    token = "";

    for (char c: right_column) {
        if (c == '.') {
            if (token != ""){
                table_column.push_back(token);
                token="";
            }
        }
        else {
            token += c;
        }
    }

    if (!token.empty() && token.back() == ';') {
        token.pop_back();
    }

    table_column.push_back(token);

    if (table_column.size() != 4 ||
        table_column[0].empty() || table_column[1].empty() ||
        table_column[2].empty() || table_column[3].empty()) {
        throw std::runtime_error("Invalid JOIN columns");
    }

    if (table_column[0] != table && table_column[2] != table) {
        throw std::runtime_error("table names don't match" + table);
    }

    return Join(table, JoinType::INNER,
                JoinCondition(table_column[0], table_column[2],
                              table_column[1], table_column[3]));
}





	//  - both sides of '=' must be qualified "table.column"
	//  - keep the condition in the order it was written
	//  - throw std::runtime_error for anything malformed
	// return Join(table, JoinType::INNER, JoinCondition(lt, lc, rt, rc));



std::unique_ptr<InsertQuery> Parser::parse_insert(const std::string& sql) {
	// Simple parsing for "INSERT INTO table (col1, col2) VALUES (val1, val2)"
	size_t into_pos		   = sql.find("INTO");
	size_t open_paren_pos  = sql.find("(");
	size_t close_paren_pos = sql.find(")");
	size_t values_pos	   = sql.find("VALUES");

	if (into_pos == std::string::npos || values_pos == std::string::npos) {
		throw std::runtime_error("Invalid INSERT statement: " + sql);
	}

	// Extract table name
	std::string table_name = sql.substr(into_pos + 4, open_paren_pos - into_pos - 4);
	table_name.erase(0, table_name.find_first_not_of(" \t"));
	table_name.erase(table_name.find_last_not_of(" \t") + 1);

	// Extract column list
	std::vector<std::string> columns;
	if (open_paren_pos != std::string::npos && close_paren_pos != std::string::npos) {
		std::string columns_str =
			sql.substr(open_paren_pos + 1, close_paren_pos - open_paren_pos - 1);

		// Parse comma-separated columns
		std::stringstream ss(columns_str);
		std::string		  column;
		while (std::getline(ss, column, ',')) {
			// Trim whitespace
			column.erase(0, column.find_first_not_of(" \t"));
			column.erase(column.find_last_not_of(" \t") + 1);

			if (!column.empty()) {
				columns.push_back(column);
			}
		}
	}

	// Extract values
	std::vector<Value> values_list;
	size_t			   values_open_pos	= sql.find("(", values_pos);
	size_t			   values_close_pos = sql.find(")", values_pos);

	if (values_open_pos != std::string::npos && values_close_pos != std::string::npos) {
		std::string values_str =
			sql.substr(values_open_pos + 1, values_close_pos - values_open_pos - 1);

		// Parse comma-separated values
		std::stringstream ss(values_str);
		std::string		  value_str;
		while (std::getline(ss, value_str, ',')) {
			// Trim whitespace
			value_str.erase(0, value_str.find_first_not_of(" \t"));
			value_str.erase(value_str.find_last_not_of(" \t") + 1);

			if (!value_str.empty()) {
				// For simplicity, we'll just store everything as strings for now
				values_list.push_back(value_str);
			}
		}
	}

	return std::make_unique<InsertQuery>(table_name, columns, values_list);
}

std::unique_ptr<CreateTableQuery> Parser::parse_create_table(const std::string& sql) {
	// Simple parsing for "CREATE TABLE table (col1 TYPE, col2 TYPE)"
	size_t table_pos	   = sql.find("TABLE");
	size_t open_paren_pos  = sql.find("(");
	size_t close_paren_pos = sql.find_last_of(")");

	if (table_pos == std::string::npos || open_paren_pos == std::string::npos ||
		close_paren_pos == std::string::npos) {
		throw std::runtime_error("Invalid CREATE TABLE statement: " + sql);
	}

	// Extract table name
	std::string table_name = sql.substr(table_pos + 5, open_paren_pos - table_pos - 5);
	table_name.erase(0, table_name.find_first_not_of(" \t"));
	table_name.erase(table_name.find_last_not_of(" \t") + 1);

	if (table_name.size() == 0) {
		throw std::runtime_error("Invalid CREATE TABLE statement: " + sql);
	}

	// Extract column definitions
	std::string columns_str = sql.substr(open_paren_pos + 1, close_paren_pos - open_paren_pos - 1);
	std::vector<ColumnDefinition> columns;

	// Simple parsing of column definitions (col_name TYPE [constraints])
	size_t pos = 0;
	while (pos < columns_str.length()) {
		// Find the end of this column definition (next comma or end)
		size_t end_pos = columns_str.find(",", pos);
		if (end_pos == std::string::npos) {
			end_pos = columns_str.length();
		}

		// Extract the column definition
		std::string col_def = columns_str.substr(pos, end_pos - pos);

		// Parse column name and type
		std::stringstream ss(col_def);
		std::string		  col_name, type_str;
		ss >> col_name >> type_str;

		// Determine column constraints
		bool not_null	 = col_def.find("NOT NULL") != std::string::npos;
		bool primary_key = col_def.find("PRIMARY KEY") != std::string::npos;

		// Map the type string to our DataType enum
		DataType type = parse_data_type(type_str);

		// Add the column definition
		columns.emplace_back(col_name, type, not_null, primary_key);

		// Move to next column definition
		pos = end_pos + 1;
	}

	return std::make_unique<CreateTableQuery>(table_name, columns);
}

std::unique_ptr<CreateIndexQuery> Parser::parse_create_index(const std::string& sql) {
	// Tokenize the SQL
	auto tokens = tokenize(sql);

	// Expected: CREATE INDEX ON 'table_name' 'column_name'
	if (tokens.size() < 5) {
		throw std::runtime_error("Invalid CREATE INDEX statement: " + sql);
	}

	size_t pos = 0;
	if (tokens[pos] != "CREATE" && tokens[pos] != "create")
		throw std::runtime_error("Expected CREATE in CREATE INDEX");
	pos++;

	if (tokens[pos] != "INDEX" && tokens[pos] != "index")
		throw std::runtime_error("Expected INDEX in CREATE INDEX");
	pos++;

	if (tokens[pos] != "ON" && tokens[pos] != "on")
		throw std::runtime_error("Expected ON in CREATE INDEX");
	pos++;

	// Table name
	if (pos >= tokens.size())
		throw std::runtime_error("Missing table name in CREATE INDEX");
	std::string table_name = tokens[pos++];
	if ((table_name.front() == '\'' && table_name.back() == '\'') ||
		(table_name.front() == '"' && table_name.back() == '"')) {
		table_name = table_name.substr(1, table_name.size() - 2);
	}

	// Column name
	if (pos >= tokens.size())
		throw std::runtime_error("Missing column name in CREATE INDEX");
	std::string column_name = tokens[pos++];
	if ((column_name.front() == '\'' && column_name.back() == '\'') ||
		(column_name.front() == '"' && column_name.back() == '"')) {
		column_name = column_name.substr(1, column_name.size() - 2);
	}

	// Auto-generate index name: table_column_idx
	std::string index_name = table_name + "_" + column_name + "_idx";

	return std::make_unique<CreateIndexQuery>(table_name, column_name, index_name);
}

std::vector<std::string> Parser::tokenize(const std::string& sql) {
	// Simple tokenizer for SQL
	std::vector<std::string> tokens;
	std::string				 current_token;
	bool					 in_quote	= false;
	char					 quote_char = '\0';

	for (char c : sql) {
		if (c == '\'' || c == '"') {
			if (!in_quote) {
				in_quote   = true;
				quote_char = c;
				current_token += c;
			} else if (c == quote_char) {
				in_quote   = false;
				quote_char = '\0';
				current_token += c;
			} else {
				current_token += c;
			}
		} else if (in_quote) {
			current_token += c;
		} else if (std::isspace(c)) {
			if (!current_token.empty()) {
				tokens.push_back(current_token);
				current_token.clear();
			}
		} else if (c == ',' || c == '(' || c == ')' || c == '=' || c == '<' || c == '>') {
			if (!current_token.empty()) {
				tokens.push_back(current_token);
				current_token.clear();
			}
			tokens.push_back(std::string(1, c));
		} else {
			current_token += c;
		}
	}

	if (!current_token.empty()) {
		tokens.push_back(current_token);
	}

	return tokens;
}

std::unique_ptr<Condition> Parser::parse_condition(const std::vector<std::string>& tokens,
												   size_t&						   pos) {
	// Placeholder for condition parsing
	// This would be a more complex recursive-descent parser in a real implementation
	return nullptr;
}

std::unique_ptr<Expression> Parser::parse_expression(const std::vector<std::string>& tokens,
													 size_t&						 pos) {
	// Placeholder for expression parsing
	// This would be a more complex recursive-descent parser in a real implementation
	return nullptr;
}

Value Parser::parse_value(const std::string& token) {
	// Try to parse as integer
	try {
		return std::stoi(token);
	} catch (...) {
		// Not an integer
	}

	// Try to parse as double
	try {
		return std::stod(token);
	} catch (...) {
		// Not a double
	}

	// Handle quoted strings
	if ((token.front() == '\'' && token.back() == '\'') ||
		(token.front() == '"' && token.back() == '"')) {
		return token.substr(1, token.length() - 2);
	}

	// Handle boolean values
	if (token == "TRUE" || token == "true") {
		return true;
	} else if (token == "FALSE" || token == "false") {
		return false;
	}

	// Default to string
	return token;
}

DataType Parser::parse_data_type(const std::string& type_str) {
	std::string upper_type = type_str;
	std::transform(upper_type.begin(), upper_type.end(), upper_type.begin(), ::toupper);

	if (upper_type == "INT" || upper_type == "INTEGER") {
		return DataType::INT;
	} else if (upper_type == "FLOAT" || upper_type == "DOUBLE") {
		return DataType::FLOAT;
	} else if (upper_type == "VARCHAR" || upper_type == "CHAR" || upper_type == "TEXT" ||
			   upper_type == "STRING") {
		return DataType::STRING;
	} else if (upper_type == "BOOL" || upper_type == "BOOLEAN") {
		return DataType::BOOL;
	} else {
		// Default to string for unknown types
		return DataType::STRING;
	}
}
