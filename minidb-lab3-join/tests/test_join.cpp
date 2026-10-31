#include "minidb/config.hpp"
#include "minidb/database.hpp"
#include "minidb/executor.hpp"
#include "minidb/parser.hpp"
#include "minidb/query.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Defined in main.cpp for the server binary; tests provide their own.
Config*			  Config::instance = nullptr;
std::atomic<bool> shutdown_requested{false};

static const std::string TEST_DIR = "test_join_dir";

using Rows = std::vector<std::vector<std::string>>;

static int failures = 0;

#define CHECK(cond)                                                                                \
	do {                                                                                           \
		if (!(cond)) {                                                                             \
			std::cerr << "  check failed: " #cond " (" << __FILE__ << ":" << __LINE__ << ")\n";     \
			++failures;                                                                            \
			return;                                                                                \
		}                                                                                          \
	} while (0)

// Small harness around one database instance shared by every test.
// Each test uses its own table names so tests stay independent.
struct Harness {
	DataBase		 db;
	ExecutionContext ctx;
	Executor		 executor{db, ctx};
	Parser			 parser;

	std::string run(const std::string& sql) {
		try {
			auto query = parser.parse(sql);
			return executor.execute(*query);
		} catch (const std::exception& e) {
			return std::string("Query failed: ") + e.what();
		}
	}
};

static std::string trim(const std::string& s) {
	size_t b = s.find_first_not_of(' ');
	if (b == std::string::npos)
		return "";
	return s.substr(b, s.find_last_not_of(' ') - b + 1);
}

// Parse the table printed by QueryResult into sorted rows (header excluded).
static Rows result_rows(const std::string& output) {
	Rows			   rows;
	std::istringstream in(output);
	std::string		   line;
	bool			   header_seen = false;
	while (std::getline(in, line)) {
		if (line.empty() || line[0] != '|')
			continue;
		if (!header_seen) {
			header_seen = true;
			continue;
		}
		std::vector<std::string> row;
		std::stringstream		 ss(line.substr(1));
		std::string				 cell;
		while (std::getline(ss, cell, '|')) {
			row.push_back(trim(cell));
		}
		rows.push_back(row);
	}
	std::sort(rows.begin(), rows.end());
	return rows;
}

static bool failed(const std::string& output) {
	return output.rfind("Query failed", 0) == 0;
}

static void create_products_and_sales(Harness& h, const std::string& products,
									  const std::string& sales) {
	h.run("CREATE TABLE " + products + " (product_id INT, product_name STRING, price INT);");
	h.run("CREATE TABLE " + sales + " (product_id INT, quantity INT);");
	h.run("INSERT INTO " + products +
		  " (product_id, product_name, price) VALUES (1, Product A, 100);");
	h.run("INSERT INTO " + products +
		  " (product_id, product_name, price) VALUES (2, Product B, 200);");
	h.run("INSERT INTO " + products +
		  " (product_id, product_name, price) VALUES (3, Product C, 300);");
	h.run("INSERT INTO " + sales + " (product_id, quantity) VALUES (1, 10);");
	h.run("INSERT INTO " + sales + " (product_id, quantity) VALUES (1, 20);");
	h.run("INSERT INTO " + sales + " (product_id, quantity) VALUES (2, 30);");
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

static const SelectQuery* as_select(const std::unique_ptr<Query>& q) {
	return dynamic_cast<const SelectQuery*>(q.get());
}

static void test_parse_join() {
	Parser p;
	auto   q = p.parse("SELECT products.product_id, sales.quantity FROM products JOIN sales ON "
					   "products.product_id = sales.product_id;");
	auto*  s = as_select(q);
	CHECK(s != nullptr);
	CHECK(s->from() == "products");
	CHECK(s->columns().size() == 2);
	CHECK(s->columns()[0] == "products.product_id");
	CHECK(s->columns()[1] == "sales.quantity");
	CHECK(s->joins().size() == 1);

	const Join& j = s->joins()[0];
	CHECK(j.table_name == "sales");
	CHECK(j.type == JoinType::INNER);
	CHECK(j.condition.left_table == "products");
	CHECK(j.condition.left_column == "product_id");
	CHECK(j.condition.right_table == "sales");
	CHECK(j.condition.right_column == "product_id");

	std::cout << "[PASS] parse JOIN ... ON a.x = b.y\n";
}

static void test_parse_inner_join_lowercase() {
	Parser p;
	auto   q = p.parse(
		  "SELECT a.x FROM a inner join b on b.y = a.x");
	auto*  s = as_select(q);
	CHECK(s != nullptr);
	CHECK(s->from() == "a");
	CHECK(s->joins().size() == 1);
	CHECK(s->joins()[0].table_name == "b");
	CHECK(s->joins()[0].condition.left_table == "b");
	CHECK(s->joins()[0].condition.left_column == "y");
	CHECK(s->joins()[0].condition.right_table == "a");
	CHECK(s->joins()[0].condition.right_column == "x");

	std::cout << "[PASS] parse lowercase INNER JOIN\n";
}

static void test_parse_select_without_join() {
	Parser p;
	auto   q = p.parse("SELECT id, name FROM students;");
	auto*  s = as_select(q);
	CHECK(s != nullptr);
	CHECK(s->from() == "students");
	CHECK(s->joins().empty());

	std::cout << "[PASS] parse SELECT without JOIN is unchanged\n";
}

static void test_parse_invalid_join() {
	{
		Parser p;
		p.parse("SELECT a.x FROM a JOIN b ON a.x = b.y"); // valid: must not throw
	}

	const std::vector<std::string> bad = {
		"SELECT a.x FROM a JOIN b a.x = b.y",	  // missing ON
		"SELECT a.x FROM a JOIN b ON a.x b.y",	  // missing =
		"SELECT a.x FROM a JOIN b ON x = b.y",	  // unqualified column
		"SELECT a.x FROM a JOIN ON a.x = b.y",	  // missing table
		"SELECT a.x FROM a JOIN b ON a.x = b.y z" // trailing garbage
	};
	for (const auto& sql : bad) {
		Parser p;
		bool   threw = false;
		try {
			p.parse(sql);
		} catch (const std::exception&) {
			threw = true;
		}
		if (!threw)
			std::cerr << "  expected parse error for: " << sql << "\n";
		CHECK(threw);
	}

	std::cout << "[PASS] malformed JOIN clauses are rejected\n";
}

// ---------------------------------------------------------------------------
// Semantic verification
// ---------------------------------------------------------------------------

static void test_verify_join(Harness& h) {
	create_products_and_sales(h, "v_products", "v_sales");

	// a valid join must be accepted
	CHECK(!failed(h.run("SELECT v_products.price FROM v_products JOIN v_sales ON "
						"v_products.product_id = v_sales.product_id;")));

	// unknown tables
	CHECK(failed(h.run("SELECT v_products.price FROM v_products JOIN nope ON v_products.product_id "
					   "= nope.product_id;")));
	CHECK(failed(h.run(
		"SELECT nope.x FROM nope JOIN v_sales ON nope.product_id = v_sales.product_id;")));

	// unknown column in ON
	CHECK(failed(h.run("SELECT v_products.price FROM v_products JOIN v_sales ON v_products.id = "
					   "v_sales.product_id;")));

	// ON must reference both tables
	CHECK(failed(h.run("SELECT v_products.price FROM v_products JOIN v_sales ON "
					   "v_products.product_id = v_products.price;")));
	CHECK(failed(h.run("SELECT v_products.price FROM v_products JOIN v_sales ON "
					   "other.product_id = v_sales.product_id;")));

	// key types must match (STRING vs INT)
	CHECK(failed(h.run("SELECT v_products.price FROM v_products JOIN v_sales ON "
					   "v_products.product_name = v_sales.product_id;")));

	// unknown / ambiguous selected columns
	CHECK(failed(h.run("SELECT v_products.nope FROM v_products JOIN v_sales ON "
					   "v_products.product_id = v_sales.product_id;")));
	CHECK(failed(h.run("SELECT other.price FROM v_products JOIN v_sales ON "
					   "v_products.product_id = v_sales.product_id;")));
	CHECK(failed(h.run("SELECT product_id FROM v_products JOIN v_sales ON "
					   "v_products.product_id = v_sales.product_id;")));

	std::cout << "[PASS] semantic checks reject invalid JOINs\n";
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------

static void test_nested_loop_join(Harness& h) {
	create_products_and_sales(h, "n_products", "n_sales");

	std::string out = h.run("SELECT n_products.product_id, n_products.product_name, "
							"n_sales.quantity FROM n_products JOIN n_sales ON "
							"n_products.product_id = n_sales.product_id;");
	CHECK(!failed(out));
	CHECK(h.executor.last_join_algorithm() == JoinAlgorithm::NESTED_LOOP);

	Rows expected = {{"1", "Product A", "10"}, {"1", "Product A", "20"}, {"2", "Product B", "30"}};
	CHECK(result_rows(out) == expected);

	std::cout << "[PASS] nested loop join (no index)\n";
}

static void test_reversed_condition_and_unqualified_columns(Harness& h) {
	create_products_and_sales(h, "r_products", "r_sales");

	std::string out = h.run("SELECT product_name, quantity FROM r_products JOIN r_sales ON "
							"r_sales.product_id = r_products.product_id;");
	CHECK(!failed(out));

	Rows expected = {{"Product A", "10"}, {"Product A", "20"}, {"Product B", "30"}};
	CHECK(result_rows(out) == expected);

	std::cout << "[PASS] ON written right-to-left + unambiguous bare columns\n";
}

static void test_empty_join(Harness& h) {
	h.run("CREATE TABLE e_left (id INT);");
	h.run("CREATE TABLE e_right (id INT);");
	h.run("INSERT INTO e_left (id) VALUES (1);");
	h.run("INSERT INTO e_right (id) VALUES (2);");

	std::string out = h.run("SELECT e_left.id FROM e_left JOIN e_right ON e_left.id = e_right.id;");
	CHECK(!failed(out));
	CHECK(out.find("Empty set") != std::string::npos);

	// joining with an empty table
	h.run("CREATE TABLE e_none (id INT);");
	out = h.run("SELECT e_left.id FROM e_left JOIN e_none ON e_left.id = e_none.id;");
	CHECK(!failed(out));
	CHECK(out.find("Empty set") != std::string::npos);

	std::cout << "[PASS] joins with no matching rows return an empty set\n";
}

static void test_string_key_join(Harness& h) {
	h.run("CREATE TABLE s_users (name STRING, age INT);");
	h.run("CREATE TABLE s_orders (customer STRING, total INT);");
	h.run("INSERT INTO s_users (name, age) VALUES (alice, 30);");
	h.run("INSERT INTO s_users (name, age) VALUES (bob, 25);");
	h.run("INSERT INTO s_orders (customer, total) VALUES (bob, 7);");
	h.run("INSERT INTO s_orders (customer, total) VALUES (carol, 9);");
	h.run("INSERT INTO s_orders (customer, total) VALUES (alice, 5);");

	const std::string sql = "SELECT s_users.name, s_users.age, s_orders.total FROM s_users JOIN "
							"s_orders ON s_users.name = s_orders.customer;";
	Rows expected = {{"alice", "30", "5"}, {"bob", "25", "7"}};

	std::string out = h.run(sql);
	CHECK(!failed(out));
	CHECK(h.executor.last_join_algorithm() == JoinAlgorithm::NESTED_LOOP);
	CHECK(result_rows(out) == expected);

	h.run("CREATE INDEX ON s_orders customer");
	out = h.run(sql);
	CHECK(!failed(out));
	CHECK(h.executor.last_join_algorithm() == JoinAlgorithm::INDEX_NESTED_LOOP);
	CHECK(result_rows(out) == expected);

	std::cout << "[PASS] STRING join key (nested loop and index nested loop)\n";
}

// Build customers(0..59) and two identical order tables with many duplicate
// keys: one without an index, one with an index on the join key.
static void test_index_nested_loop_join(Harness& h) {
	const int customers = 60;
	const int orders	= 1200;
	const int key_range = 80; // ids 60..79 have no customer

	h.run("CREATE TABLE i_customers (id INT, name STRING);");
	h.run("CREATE TABLE i_orders_plain (customer_id INT, amount INT);");
	h.run("CREATE TABLE i_orders_idx (customer_id INT, amount INT);");
	// index created before inserts: maintained by INSERT
	h.run("CREATE INDEX ON i_orders_idx customer_id");

	for (int c = 0; c < customers; c++) {
		h.run("INSERT INTO i_customers (id, name) VALUES (" + std::to_string(c) + ", c" +
			  std::to_string(c) + ");");
	}
	for (int o = 0; o < orders; o++) {
		std::string values =
			"(customer_id, amount) VALUES (" + std::to_string(o % key_range) + ", " +
			std::to_string(o) + ");";
		h.run("INSERT INTO i_orders_plain " + values);
		h.run("INSERT INTO i_orders_idx " + values);
	}

	std::string nlj = h.run("SELECT i_customers.name, i_orders_plain.amount FROM i_customers JOIN "
							"i_orders_plain ON i_customers.id = i_orders_plain.customer_id;");
	CHECK(!failed(nlj));
	CHECK(h.executor.last_join_algorithm() == JoinAlgorithm::NESTED_LOOP);

	std::string inlj = h.run("SELECT i_customers.name, i_orders_idx.amount FROM i_customers JOIN "
							 "i_orders_idx ON i_customers.id = i_orders_idx.customer_id;");
	CHECK(!failed(inlj));
	CHECK(h.executor.last_join_algorithm() == JoinAlgorithm::INDEX_NESTED_LOOP);

	Rows expected;
	for (int o = 0; o < orders; o++) {
		if (o % key_range < customers)
			expected.push_back({"c" + std::to_string(o % key_range), std::to_string(o)});
	}
	std::sort(expected.begin(), expected.end());

	Rows nlj_rows  = result_rows(nlj);
	Rows inlj_rows = result_rows(inlj);
	if (inlj_rows.size() != expected.size()) {
		std::cerr << "  index nested loop join returned " << inlj_rows.size() << " rows, expected "
				  << expected.size() << "\n";
	}
	CHECK(nlj_rows == expected);
	CHECK(inlj_rows == expected);

	std::cout << "[PASS] index nested loop join matches nested loop join (" << expected.size()
			  << " rows, duplicate keys)\n";
}

static void test_index_on_outer_only_uses_nested_loop(Harness& h) {
	create_products_and_sales(h, "o_products", "o_sales");
	h.run("CREATE INDEX ON o_products product_id");

	std::string out = h.run("SELECT o_products.product_id, o_sales.quantity FROM o_products JOIN "
							"o_sales ON o_products.product_id = o_sales.product_id;");
	CHECK(!failed(out));
	CHECK(h.executor.last_join_algorithm() == JoinAlgorithm::NESTED_LOOP);
	CHECK(result_rows(out).size() == 3);

	std::cout << "[PASS] index only on the outer table falls back to nested loop\n";
}

// Run one test; an exception counts as a failure instead of aborting the run.
template <typename Fn, typename... Args> static void run_test(Fn fn, Args&... args) {
	int before = failures;
	try {
		fn(args...);
	} catch (const std::exception& e) {
		std::cerr << "  uncaught exception: " << e.what() << "\n";
		++failures;
	}
	if (failures != before)
		std::cout << "[FAIL] see errors above\n";
}

int main() {
#ifdef ENABLE_THREADPOOL
	std::cout << "[SKIP] test_join needs a stdin build: cmake -DENABLE_THREADPOOL=OFF ..\n";
	return 77;
#endif

	fs::remove_all(TEST_DIR);
	fs::create_directory(TEST_DIR);

	std::string arg0 = "test_join", arg1 = "--datadir", arg2 = TEST_DIR;
	char*		argv[] = {arg0.data(), arg1.data(), arg2.data(), nullptr};
	Config::get_instance()->get(3, argv);

	run_test(test_parse_join);
	run_test(test_parse_inner_join_lowercase);
	run_test(test_parse_select_without_join);
	run_test(test_parse_invalid_join);

	{
		Harness h;
		run_test(test_verify_join, h);
		run_test(test_nested_loop_join, h);
		run_test(test_reversed_condition_and_unqualified_columns, h);
		run_test(test_empty_join, h);
		run_test(test_string_key_join, h);
		run_test(test_index_nested_loop_join, h);
		run_test(test_index_on_outer_only_uses_nested_loop, h);
	}

	fs::remove_all(TEST_DIR);

	if (failures > 0) {
		std::cerr << failures << " JOIN test(s) failed\n";
		return 1;
	}
	std::cout << "All JOIN tests passed\n";
	return 0;
}
