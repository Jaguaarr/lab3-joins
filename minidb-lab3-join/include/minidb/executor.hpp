#ifndef __EXECUTOR_HPP__
#define __EXECUTOR_HPP__

#include <string>
#include <thread>
#include <vector>

class DataBase;
class Query;
class CreateTableQuery;
class InsertQuery;
class SelectQuery;
class CreateIndexQuery;

/**
 * @struct ExecutionContext
 * @brief Represents the execution context for a session or transaction.
 *
 * Tracks whether a transaction is active, the transaction ID,
 * and the thread/session ID executing the queries.
 */
struct ExecutionContext {
	bool in_transaction = false;
	int	 transaction_id = -1;

	std::thread::id session_id;
	ExecutionContext() : session_id(std::this_thread::get_id()) {}
};

/**
 * @brief Algorithm used by the last JOIN executed (exposed for tests and EXPLAIN-like output).
 */
enum class JoinAlgorithm { NONE, NESTED_LOOP, INDEX_NESTED_LOOP };

/**
 * @class Executor
 * @brief Executes database queries within a given execution context.
 *
 * Interfaces with the DataBase instance to run queries such as
 * create table, insert, and select.
 *
 * Provides a single execute() method that dispatches query types
 * to the appropriate private execution methods.
 */
class Executor {
  private:
	DataBase&		  db;
	ExecutionContext& ctx;

  public:
	Executor(DataBase& db, ExecutionContext& ctx);
	std::string execute(const Query& q);

	JoinAlgorithm last_join_algorithm() const { return last_join_algorithm_; }

  private:
	std::string exec_create(const CreateTableQuery& q);
	std::string exec_insert(const InsertQuery& q);
	std::string exec_select(const SelectQuery& q);
	std::string exec_create_index(const CreateIndexQuery& q);
	std::string exec_join(const SelectQuery& q);

	JoinAlgorithm last_join_algorithm_ = JoinAlgorithm::NONE;
};

// Lab 3 benchmark hook, used by tests/bench_join.cpp (provided, do not change)
enum class NljFlavor { TUPLE, PAGE, CHUNK };

struct NljRun {
	std::vector<std::string> rows;		  // sorted "outer row | inner row", columns by name
	unsigned long long		 page_reads;  // pages read from disk during the join
	size_t					 peak_frames; // most buffer frames held at the same time
	double					 ms;		  // wall-clock time of the join
};

// Runs one nested loop join flavor on a cold buffer pool of `pool_frames` frames.
// CHUNK uses chunks of pool_frames - 2 outer pages.
NljRun run_nlj_flavor(DataBase& db, const std::string& outer_table, const std::string& outer_key,
					  const std::string& inner_table, const std::string& inner_key,
					  NljFlavor flavor, size_t pool_frames);

#endif
