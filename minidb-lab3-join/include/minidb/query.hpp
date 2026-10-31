#ifndef __QUERY__HPP__
#define __QUERY__HPP__

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <iostream>

#include "table.hpp"

// Comparison operators for WHERE clauses
enum class ComparisonOp {
	EQ,	 // =
	NE,	 // !=
	LT,	 // <
	LE,	 // <=
	GT,	 // >
	GE,	 // >=
	LIKE // LIKE
};

// Logical operators for combining conditions
enum class LogicalOp { AND, OR, NOT };

// Expression types
enum class ExprType { COLUMN, LITERAL, BINARY_OP, FUNCTION };

// Base expression class
class Expression {
  public:
	virtual ~Expression()		  = default;
	virtual ExprType type() const = 0;
};

// Column reference expression
class ColumnExpression : public Expression {
  public:
	ColumnExpression(const std::string& table_name, const std::string& column_name)
		: table_name_(table_name), column_name_(column_name) {}

	ExprType type() const override { return ExprType::COLUMN; }

	const std::string& table_name() const { return table_name_; }
	const std::string& column_name() const { return column_name_; }

  private:
	std::string table_name_;
	std::string column_name_;
};

// Literal value expression
class LiteralExpression : public Expression {
  public:
	explicit LiteralExpression(const Value& value) : value_(value) {}

	ExprType type() const override { return ExprType::LITERAL; }

	const Value& value() const { return value_; }

  private:
	Value value_;
};

// Binary operation expression
class BinaryOpExpression : public Expression {
  public:
	BinaryOpExpression(std::unique_ptr<Expression> left, ComparisonOp op,
					   std::unique_ptr<Expression> right)
		: left_(std::move(left)), op_(op), right_(std::move(right)) {}

	ExprType type() const override { return ExprType::BINARY_OP; }

	const Expression* left() const { return left_.get(); }
	ComparisonOp	  op() const { return op_; }
	const Expression* right() const { return right_.get(); }

  private:
	std::unique_ptr<Expression> left_;
	ComparisonOp				op_;
	std::unique_ptr<Expression> right_;
};

// WHERE condition
class Condition {
  public:
	// Primitive condition with binary operation
	Condition(std::unique_ptr<Expression> expr) : expr_(std::move(expr)), is_compound_(false) {}

	// Compound condition with logical operator
	Condition(std::unique_ptr<Condition> left, LogicalOp op, std::unique_ptr<Condition> right)
		: left_(std::move(left)), op_(op), right_(std::move(right)), is_compound_(true) {}

	// NOT condition
	explicit Condition(std::unique_ptr<Condition> child, bool is_not = false)
		: left_(std::move(child)), op_(LogicalOp::NOT), is_compound_(true), is_not_(is_not) {}

	bool	  is_compound() const { return is_compound_; }
	bool	  is_not() const { return is_not_; }
	LogicalOp op() const { return op_; }

	const Expression* expr() const { return expr_.get(); }
	const Condition*  left() const { return left_.get(); }
	const Condition*  right() const { return right_.get(); }

  private:
	std::unique_ptr<Expression> expr_;
	std::unique_ptr<Condition>	left_;
	LogicalOp					op_;
	std::unique_ptr<Condition>	right_;
	bool						is_compound_;
	bool						is_not_ = false;
};

// Join types
enum class JoinType { INNER, LEFT, RIGHT, FULL };

// Join condition
struct JoinCondition {
	std::string left_table;
	std::string left_column;
	std::string right_table;
	std::string right_column;

	JoinCondition(const std::string& lt, const std::string& lc, const std::string& rt,
				  const std::string& rc)
		: left_table(lt), left_column(lc), right_table(rt), right_column(rc) {}
};

// Join specification
struct Join {
	std::string	  table_name;
	JoinType	  type;
	JoinCondition condition;

	Join(const std::string& tn, JoinType jt, const JoinCondition& jc)
		: table_name(tn), type(jt), condition(jc) {}
};

// Order direction
enum class OrderDirection { ASC, DESC };

// Order by specification
struct OrderBy {
	std::string	   column_name;
	OrderDirection direction;

	OrderBy(const std::string& col, OrderDirection dir = OrderDirection::ASC)
		: column_name(col), direction(dir) {}
};

// Base query class
class Query {
  public:
	virtual ~Query() = default;
	virtual void print() const {};
};

// SELECT query
class SelectQuery : public Query {
  public:
	SelectQuery(const std::vector<std::string>& columns, const std::string& table_name)
		: columns_(columns), from_(table_name) {}

	// Add a WHERE condition
	void add_condition(std::unique_ptr<Condition> condition) { where_ = std::move(condition); }

	// Add a JOIN clause
	void add_join(const Join& join) { joins_.push_back(join); }

	// Add an ORDER BY clause
	void add_order_by(const OrderBy& order_by) { order_by_.push_back(order_by); }

	// Set limit
	void set_limit(int limit) { limit_ = limit; }

	void print() const override {
		std::cout << "SELECT (";
		for (const auto& c : columns_) {
			std::cout << c << ", ";
		}
		std::cout << ") FROM " << from_ << std::endl;
	}

	// Getters
	const std::vector<std::string>& columns() const { return columns_; }
	const std::string&				from() const { return from_; }
	const Condition*				where() const { return where_.get(); }
	const std::vector<Join>&		joins() const { return joins_; }
	const std::vector<OrderBy>&		order_by() const { return order_by_; }
	int								limit() const { return limit_; }

  private:
	std::vector<std::string>   columns_;
	std::string				   from_;
	std::unique_ptr<Condition> where_;
	std::vector<Join>		   joins_;
	std::vector<OrderBy>	   order_by_;
	int						   limit_ = -1; // -1 means no limit
};

// INSERT query
class InsertQuery : public Query {
  public:
	InsertQuery(const std::string& table_name, const std::vector<std::string>& columns,
				const std::vector<Value>& values)
		: table_name_(table_name), columns_(columns), values_(values) {}

	const std::string&				table_name() const { return table_name_; }
	const std::vector<std::string>& columns() const { return columns_; }
	const std::vector<Value>&		values() const { return values_; }

	void print() const override {
		std::cout << "INSERT INTO " << table_name();
		std::cout << " ( ";
		for (auto c : columns()) {
			std::cout << c << ", ";
		}
		std::cout << ") VALUES (";
		for (auto v : values()) {
			std::cout << " " << std::get<std::string>(v) << ",";
		}
		std::cout << ")" << std::endl;
	}

	std::unordered_map<std::string, std::string> columns_to_values() const {

		std::unordered_map<std::string, std::string> value_map;
		for (size_t i = 0; i < this->columns_.size(); ++i) {
			value_map[columns_[i]] = std::get<std::string>(values_[i]);
		}
		return (value_map);
	}

  private:
	std::string				 table_name_;
	std::vector<std::string> columns_;
	std::vector<Value>		 values_;
};

// CREATE TABLE query
class CreateTableQuery : public Query {
  public:
	CreateTableQuery(const std::string& table_name, const std::vector<ColumnDefinition>& columns)
		: table_name_(table_name), columns_(columns) {}

	const std::string&					 table_name() const { return table_name_; }
	const std::vector<ColumnDefinition>& columns() const { return columns_; }

	void print() const override {
		std::cout << "CREATE TABLE " << table_name_;
		std::cout << " (";
		for (auto cdef : columns_) {
			std::cout << cdef.name << " " << DataTypeToString(cdef.type_) << ", ";
		}
		std::cout << ")" << std::endl;
	}

  private:
	std::string					  table_name_;
	std::vector<ColumnDefinition> columns_;
};

// CREATE INDEX query
class CreateIndexQuery : public Query {
  public:
	CreateIndexQuery(const std::string& table_name, const std::string& column_name,
					 const std::string& index_name)
		: table_name_(table_name), column_name_(column_name), index_name_(index_name) {}

	const std::string& table_name() const { return table_name_; }
	const std::string& column_name() const { return column_name_; }
	const std::string& index_name() const { return index_name_; }

	void print() const override {
		std::cout << "CREATE INDEX ON " << table_name_ << " " << column_name_ << std::endl;
	}

  private:
	std::string table_name_;
	std::string column_name_;
	std::string index_name_;
};

#endif
