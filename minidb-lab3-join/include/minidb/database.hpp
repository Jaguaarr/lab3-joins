#ifndef __DATABASE_HPP__
#define __DATABASE_HPP__

#include <memory>

class ConnectionQueue;
class ThreadPool;
class CatalogManager;
class BufferManager;
class TableManager;
class IndexManager;

/**
 * @class DataBase
 * @brief Core database class managing all main components and lifecycle.
 *
 * Encapsulates connection queue, thread pool, catalog manager,
 * buffer manager, and table manager.
 *
 * Provides methods to start and stop the database server.
 * Accessors allow retrieving the key subsystem instances.
 *
 * Copy and assignment are disabled to enforce unique ownership.
 */
class DataBase {
  public:
	DataBase();
	~DataBase();

	DataBase(const DataBase&)			 = delete;
	DataBase& operator=(const DataBase&) = delete;

	void start();
	void stop();

	ConnectionQueue& get_connection_queue();
	CatalogManager&	 getCatalog() { return (*catalog); }
	TableManager&	 get_table_manager() { return (*tablem); }
	BufferManager&	 get_buffer_manager() { return (*bufferm); }
	IndexManager&	 get_index_manager() { return (*indexm); }

  private:
	void bootstrapp();

	std::unique_ptr<ConnectionQueue> q;
	std::unique_ptr<ThreadPool>		 tp;
	std::unique_ptr<CatalogManager>	 catalog;
	std::unique_ptr<BufferManager>	 bufferm;
	std::unique_ptr<TableManager>	 tablem;
	std::unique_ptr<IndexManager>	 indexm;
};

#endif
