// Lab 3 benchmark: tuple, page and chunk nested loop join.
//
// Fills a few tables, runs your nested_loop_join, page_nested_loop_join and
// chunk_nested_loop_join (src/executor.cpp) on a cold buffer pool of B frames, and prints
// for every run: correct rows, page reads, the textbook estimate, peak frames and time.
// Keep the three function names and signatures. Not run by CI.
//
//   cmake -DENABLE_THREADPOOL=OFF .. && cmake --build . && ./bin/bench_join

#include "minidb/buffer_manager/buffer_manager.hpp"
#include "minidb/config.hpp"
#include "minidb/database.hpp"
#include "minidb/executor.hpp"
#include "minidb/parser.hpp"
#include "minidb/query.hpp"
#include "minidb/table_manager/table_manager.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Defined in main.cpp for the server binary; the benchmark provides its own.
Config*			  Config::instance = nullptr;
std::atomic<bool> shutdown_requested{false};

static const std::string BENCH_DIR = "bench_join_dir";
static const int		 PAD_LEN   = 76; // 90-byte tuples: 45 per page
static const int		 KEYS	   = 1000;

struct Table {
	std::string		 name;
	int				 pages = 0;
	std::vector<int> keys; // join key of tuple i (its id is i)
	std::string		 pad;
};

struct Bench {
	DataBase		 db;
	ExecutionContext ctx;
	Executor		 executor{db, ctx};
	Parser			 parser;
	int				 failures = 0;

	void run_sql(const std::string& sql) {
		std::string out = executor.execute(*parser.parse(sql));
		if (out.find("rror") != std::string::npos || out.find("failed") != std::string::npos)
			throw std::runtime_error(sql.substr(0, 60) + ": " + out);
	}

	// Creates `name` with exactly `pages` full pages of (id INT, k INT, pad STRING) tuples.
	Table make_table(const std::string& name, int pages, unsigned seed) {
		Table t{name, 0, {}, std::string(PAD_LEN, char('a' + seed % 26))};
		int	  per_page = (4096 - 4) / (2 * 4 + 2 + PAD_LEN + 4);
		run_sql("CREATE TABLE " + name + " (id INT, k INT, pad STRING);");
		std::mt19937 rng(seed);
		for (int i = 0; i < pages * per_page; i++) {
			int k = static_cast<int>(rng() % KEYS);
			t.keys.push_back(k);
			run_sql("INSERT INTO " + name + " (id, k, pad) VALUES (" + std::to_string(i) + ", " +
					std::to_string(k) + ", " + t.pad + ");");
		}
		db.get_buffer_manager().flush_all();
		t.pages =
			db.get_buffer_manager()
				.get_heap_file(db.get_table_manager().get_table_id(name))
				->get_num_pages();
		return t;
	}

	// The rows a correct join must return, in the format of NljRun::rows
	static std::vector<std::string> expected(const Table& outer, const Table& inner) {
		std::vector<std::string> rows;
		for (size_t o = 0; o < outer.keys.size(); o++) {
			for (size_t i = 0; i < inner.keys.size(); i++) {
				if (outer.keys[o] != inner.keys[i])
					continue;
				rows.push_back(std::to_string(o) + "," + std::to_string(outer.keys[o]) + "," +
							   outer.pad + " | " + std::to_string(i) + "," +
							   std::to_string(inner.keys[i]) + "," + inner.pad);
			}
		}
		std::sort(rows.begin(), rows.end());
		return rows;
	}

	// Textbook page reads: M + (tuples of outer) * N, M + M * N, M + ceil(M / (B - 2)) * N
	static long long estimate(NljFlavor f, const Table& outer, const Table& inner, int frames) {
		long long m = outer.pages, n = inner.pages;
		switch (f) {
		case NljFlavor::TUPLE:
			return m + static_cast<long long>(outer.keys.size()) * n;
		case NljFlavor::PAGE:
			return m + m * n;
		case NljFlavor::CHUNK:
			return m + (m + frames - 3) / (frames - 2) * n;
		}
		return 0;
	}

	void row(const char* label, NljFlavor f, const Table& outer, const Table& inner, int frames) {
		static const char* names[] = {"tuple", "page", "chunk"};
		std::cout << std::left << std::setw(20) << label << std::setw(7)
				  << names[static_cast<int>(f)] << std::right << std::setw(5)
				  << outer.name << std::setw(5) << outer.pages << std::setw(5)
				  << inner.pages << std::setw(5) << frames;
		try {
			NljRun run = run_nlj_flavor(db, outer.name, "k", inner.name, "k", f, frames);
			bool   ok  = run.rows == expected(outer, inner);
			bool   fit = run.peak_frames <= static_cast<size_t>(frames);
			if (!ok || !fit)
				failures++;
			std::cout << std::setw(8) << run.rows.size() << std::setw(7) << (ok ? "yes" : "NO")
					  << std::setw(12) << run.page_reads << std::setw(12)
					  << estimate(f, outer, inner, frames) << std::setw(6) << run.peak_frames
					  << (fit ? "  " : " !") << std::setw(10) << std::fixed
					  << std::setprecision(1) << run.ms << "\n";
		} catch (const std::exception& e) {
			failures++;
			std::cout << "   " << e.what() << "\n";
		}
	}
};

int main() {
#ifdef ENABLE_THREADPOOL
	std::cout << "[SKIP] bench_join needs a stdin build: cmake -DENABLE_THREADPOOL=OFF ..\n";
	return 77;
#endif

	fs::remove_all(BENCH_DIR);
	fs::create_directory(BENCH_DIR);
	std::string arg0 = "bench_join", arg1 = "--datadir", arg2 = BENCH_DIR;
	char*		argv[] = {arg0.data(), arg1.data(), arg2.data(), nullptr};
	Config::get_instance()->get(3, argv);
	Config::get_instance()->buffer_manager_pool_size = 512; // fast loading only

	int failures = 0;
	{
		Bench b;
		std::cout << "Loading tables...\n";
		Table r5   = b.make_table("r5", 5, 1);
		Table s10  = b.make_table("s10", 10, 2);
		Table a50  = b.make_table("a50", 50, 3);
		Table b50  = b.make_table("b50", 50, 4);
		Table c100 = b.make_table("c100", 100, 5);

		std::cout << "\nrows = rows returned, ok = exactly the expected rows,\n"
				  << "reads = pages read from disk, estimate = textbook formula,\n"
				  << "peak = most frames held at once (! = more than B: too many pages pinned)\n\n"
				  << std::left << std::setw(20) << "experiment" << std::setw(7) << "flavor"
				  << std::right << std::setw(5) << "outer" << std::setw(5) << "M"
				  << std::setw(5) << "N" << std::setw(5) << "B" << std::setw(8) << "rows"
				  << std::setw(7) << "ok" << std::setw(12) << "reads" << std::setw(12)
				  << "estimate" << std::setw(6) << "peak" << std::setw(12) << "ms" << "\n"
				  << std::string(100, '-') << "\n";

		// 1. Same tables, same pool: tuple vs page vs chunk
		for (NljFlavor f : {NljFlavor::TUPLE, NljFlavor::PAGE, NljFlavor::CHUNK})
			b.row("1 flavors", f, r5, s10, 4);

		// 2. Chunk NLJ as the pool grows (page NLJ for reference)
		b.row("2 pool size", NljFlavor::PAGE, a50, b50, 3);
		for (int frames : {3, 4, 6, 10, 18, 34, 52})
			b.row("2 pool size", NljFlavor::CHUNK, a50, b50, frames);

		// 3. Which table should be outer (fills the B - 2 chunk frames)?
		for (int frames : {4, 12}) {
			b.row("3 small outer", NljFlavor::CHUNK, s10, c100, frames);
			b.row("3 large outer", NljFlavor::CHUNK, c100, s10, frames);
		}
		failures = b.failures;
	}

	fs::remove_all(BENCH_DIR);
	std::cout << "\n" << (failures ? "Some runs failed (see ok / peak / errors above).\n"
								   : "All runs returned the expected rows.\n");
	return failures ? 1 : 0;
}
