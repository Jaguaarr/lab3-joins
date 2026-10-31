#include <iostream>
#include <memory>
#include <thread>

#include "minidb/catalog.hpp"
#include "minidb/config.hpp"
#include "minidb/database.hpp"
#include "minidb/network/connection.hpp"
#include "minidb/network/connqueue.hpp"
#include "minidb/network/server.hpp"
#include "minidb/network/threadpool.hpp"
#include "minidb/parser.hpp"
#include "minidb/shutdown.hpp"

#include "minidb/buffer_manager/buffer_manager.hpp"
#include "minidb/executor.hpp"
#include "minidb/index/b_plus_tree.hpp"
#include "minidb/table_manager/table_manager.hpp"

#include <unistd.h>

DataBase::DataBase() {

#ifdef ENABLE_THREADPOOL
	q  = std::make_unique<ConnectionQueue>();
	tp = std::make_unique<ThreadPool>(*this, Config::get_instance()->nthreads);
#endif
	catalog = std::make_unique<CatalogManager>(Config::get_instance()->catalog_path);
	bufferm = std::make_unique<BufferManager>(Config::get_instance()->buffer_manager_pool_size);
	tablem	= std::make_unique<TableManager>(*bufferm);
	indexm	= std::make_unique<IndexManager>(*bufferm, *catalog, Config::get_instance()->index_dir);
}

DataBase::~DataBase() {
}

void DataBase::start() {
	// check if db is starting in backup mode
	// otherwise bootstrap the data directory
	bootstrapp();

#ifdef ENABLE_THREADPOOL
	if (tp->started()) {
		start_connection_server(*this);
	}
#else
	std::string		 query;
	auto			 parser = std::make_unique<Parser>();
	ExecutionContext ctx;
	Executor		 executor(*this, ctx);

	std::cerr << ">> ";
	while (std::getline(std::cin, query)) {
		try {
			auto		parsed_query = parser->parse(query);
			std::string res			 = executor.execute(*parsed_query);
			std::cout << res << std::endl;
		} catch (const std::exception& e) {
			std::cout << e.what();
		}
		std::cerr << ">> ";
	}
#endif
}

void DataBase::stop() {
	this->catalog->saveToDisk();
	this->bufferm->flush_all();
}

void DataBase::bootstrapp() {
	// creating table that are in the catalog
	auto tables = this->catalog->tables_names();

	for (const auto& table : tables) {
		auto path = Config::get_instance()->datadir + "/" + table + ".tbl";
		this->tablem->create_table(table, path);
		auto idxlist = this->catalog->listIndexes(table);
		this->indexm->load_indices(idxlist);
	}
}

ConnectionQueue& DataBase::get_connection_queue() {
	return (*q);
}

void backend_server(DataBase& db) {
	std::thread::id	 id_	= std::this_thread::get_id();
	ConnectionQueue& q		= db.get_connection_queue();
	auto			 parser = std::make_unique<Parser>();
	std::string		 query;

	for (;;) {
		Connection		 c = q.pop();
		ExecutionContext ctx;
		Executor		 executor(db, ctx);

		for (;;) {
			if (!c.recieve(query))
				break;
			try {
				auto parsed_query = parser->parse(query);
				parsed_query->print();
				std::string res = executor.execute(*parsed_query);

				if (!c.transmit(res)) {
					break;
				}
			} catch (std::exception& e) {
				if (!c.transmit(e.what())) {
					break;
				}
			}
		}
	}
}
