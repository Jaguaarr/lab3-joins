#include "minidb/executor.hpp"
#include "minidb/database.hpp"

#include "minidb/buffer_manager/buffer_manager.hpp"
#include "minidb/catalog.hpp"
#include "minidb/config.hpp"
#include "minidb/index/b_plus_tree.hpp"
#include "minidb/query.hpp"
#include "minidb/query_result.hpp"
#include "minidb/semantic_verifier.hpp"
#include "minidb/table_manager/table_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>
#include <stdexcept>

Executor::Executor(DataBase& db_, ExecutionContext& ctx_) : db(db_), ctx(ctx_) {
}

namespace {

	// Column name -> value of one decoded record
	using DecodedRow = std::unordered_map<std::string, Value>;

	// Records are serialized with columns ordered by name (see exec_insert)
	std::vector<ColumnDefinition> sorted_schema(const TableInfo& tinfo) {
		auto schema = tinfo.columns;
		std::sort(
			schema.begin(), schema.end(),
			[](const ColumnDefinition& a, const ColumnDefinition& b) { return a.name < b.name; });
		return schema;
	}

	DecodedRow decode_record(const std::vector<ColumnDefinition>& schema,
							 const std::vector<uint8_t>&		  record) {
		DecodedRow row;
		size_t	   offset = 0;
		for (const auto& col : schema) {
			if (offset >= record.size())
				break;

			switch (col.type_) {
			case DataType::INT: {
				int v;
				std::memcpy(&v, &record[offset], sizeof(int));
				row[col.name] = v;
				offset += sizeof(int);
				break;
			}
			case DataType::FLOAT: {
				float v;
				std::memcpy(&v, &record[offset], sizeof(float));
				row[col.name] = static_cast<double>(v);
				offset += sizeof(float);
				break;
			}
			case DataType::BOOL: {
				row[col.name] = (record[offset] != 0);
				offset += sizeof(uint8_t);
				break;
			}
			case DataType::STRING: {
				uint16_t len;
				std::memcpy(&len, &record[offset], sizeof(uint16_t));
				offset += sizeof(uint16_t);

				row[col.name] = std::string(reinterpret_cast<const char*>(&record[offset]), len);
				offset += len;
				break;
			}
			}
		}
		return row;
	}

	std::string value_to_string(const Value& v) {
		if (auto* i = std::get_if<int>(&v))
			return std::to_string(*i);
		if (auto* d = std::get_if<double>(&v))
			return std::to_string(*d);
		if (auto* b = std::get_if<bool>(&v))
			return *b ? "true" : "false";
		return std::get<std::string>(v);
	}

	std::string column_to_string(const DecodedRow& row, const std::string& column) {
		auto it = row.find(column);
		return it == row.end() ? "" : value_to_string(it->second);
	}

	// One input of a join: the table and the column it is joined on
	struct JoinSide {
		std::string					  table;
		int							  table_id;
		std::vector<ColumnDefinition> schema; // storage order
		std::string					  key_column;
		DataType					  key_type;
	};

	// (outer row, inner row) pairs that satisfy the join condition
	using JoinedRows = std::vector<std::pair<DecodedRow, DecodedRow>>;

	JoinSide make_join_side(TableManager& tm, const TableInfo& tinfo, const std::string& key) {
		JoinSide side{tinfo.name, tm.get_table_id(tinfo.name), sorted_schema(tinfo), key,
					  DataType::INT};
		for (const auto& col : tinfo.columns) {
			if (col.name == key)
				side.key_type = col.type_;
		}
		return side;
	}

	// All record ids of `inner` whose key equals `key`, using the B+ tree index
	std::vector<RecordId> index_lookup(IndexManager& im, const JoinSide& inner, const Value& key) {
		switch (inner.key_type) {
		case DataType::INT: {
			int k = std::get<int>(key);
			return im.range_search<int>(inner.table, inner.key_column, k, k);
		}
		case DataType::FLOAT: {
			float k = static_cast<float>(std::get<double>(key));
			return im.range_search<float>(inner.table, inner.key_column, k, k);
		}
		case DataType::BOOL: {
			bool k = std::get<bool>(key);
			return im.range_search<bool>(inner.table, inner.key_column, k, k);
		}
		case DataType::STRING: {
			const std::string& k = std::get<std::string>(key);
			return im.range_search<std::string>(inner.table, inner.key_column, k, k);
		}
		}
		return {};
	}

	// Naive nested loop join: for every outer tuple, scan every inner tuple.
	JoinedRows nested_loop_join(TableManager& tm, const JoinSide& outer, const JoinSide& inner) {
		JoinedRows out;
		// TODO (Lab 3):
		//  for each record id of outer (tm.scan), decode it (decode_record + tm.get)
		//    for each record id of inner, decode it
		//      if both rows have their key column and the keys are equal,
		//      append (outer row, inner row) to out
		return out;
	}

	// Page nested loop join: for every outer page, scan every inner page.
	JoinedRows page_nested_loop_join(BufferManager& bm, const JoinSide& outer,
									 const JoinSide& inner) {
		(void)bm;
		(void)outer;
		(void)inner;
		// TODO (Lab 3, requirement 4):
		//  for each outer page (bm.fetch_page, SlottedPage, decode_record on every valid slot)
		//    for each inner page, decode its tuples and join them with the outer page's tuples
		//  unpin every page you fetch
		throw std::logic_error("TODO(lab3): implement page_nested_loop_join");
	}

	// Chunk (block) nested loop join: keep chunk_pages outer pages pinned, scan the inner
	// table once per chunk.
	JoinedRows chunk_nested_loop_join(BufferManager& bm, const JoinSide& outer,
									  const JoinSide& inner, size_t chunk_pages) {
		(void)bm;
		(void)outer;
		(void)inner;
		(void)chunk_pages;
		// TODO (Lab 3, requirement 4):
		//  pin up to chunk_pages outer pages, then for each inner page join its tuples with
		//  every tuple of the chunk; unpin the chunk and continue with the next one
		//  never hold more than chunk_pages + 1 pages pinned
		throw std::logic_error("TODO(lab3): implement chunk_nested_loop_join");
	}

	// Index nested loop join: for every outer tuple, probe the inner table's index.
	JoinedRows index_nested_loop_join(TableManager& tm, IndexManager& im, const JoinSide& outer,
									  const JoinSide& inner) {
		JoinedRows out;
		// TODO (Lab 3):
		//  for each outer row, look up its key with index_lookup(im, inner, key)
		//  and fetch every matching inner record by its RecordId
		return out;
	}

} // namespace

std::string Executor::execute(const Query& query) {
	if (auto* q = dynamic_cast<const CreateTableQuery*>(&query)) {
		return exec_create(*q);
	} else if (auto* q = dynamic_cast<const InsertQuery*>(&query)) {
		return exec_insert(*q);
	} else if (auto* q = dynamic_cast<const SelectQuery*>(&query)) {
		return exec_select(*q);
	} else if (auto* q = dynamic_cast<const CreateIndexQuery*>(&query)) {
		return exec_create_index(*q);
	} else {
		return ("UNKNOWN QUERY");
	}
}

std::string Executor::exec_create(const CreateTableQuery& query) {
	// update db tablemanager
	// register table in catalog

	std::string path = Config::get_instance()->datadir + "/" + query.table_name() + ".tbl";

	int id = db.get_table_manager().create_table(query.table_name(), path);
	if (id == -1) {
		return QueryResult(false, "TABLE " + query.table_name() + " ALREADY EXIST")();
	}

	db.getCatalog().createTable({query.table_name(), query.columns()});
	return QueryResult(true, "TABLE " + query.table_name() + " CREATED")();
}

std::string Executor::exec_insert(const InsertQuery& query) {
	// schema validation
	try {
		SemanticVerifier verifier(db.getCatalog());
		verifier.verify_insert_query(query);
	} catch (const std::exception& e) {
		return QueryResult(false, e.what())();
	}

	int table_id = db.get_table_manager().get_table_id(query.table_name());

	auto table_info_opt = db.getCatalog().getTable(query.table_name());
	auto schema			= table_info_opt->columns;

	std::sort(
		schema.begin(), schema.end(),
		[](const ColumnDefinition& a, const ColumnDefinition& b) { return (a.name < b.name); });

	// serialization;
	auto				 value_map = query.columns_to_values();
	std::vector<uint8_t> record;
	for (const auto& col : schema) {
		auto			   it	   = value_map.find(col.name);
		const std::string& val_str = it->second;

		switch (col.type_) {
		case DataType::INT: {
			int v = std::stoi(val_str);
			record.insert(record.end(), reinterpret_cast<uint8_t*>(&v),
						  reinterpret_cast<uint8_t*>(&v) + sizeof(int));
			break;
		}
		case DataType::FLOAT: {
			float v = std::stof(val_str);
			record.insert(record.end(), reinterpret_cast<uint8_t*>(&v),
						  reinterpret_cast<uint8_t*>(&v) + sizeof(float));
			break;
		}
		case DataType::BOOL: {
			bool v = (val_str == "true");
			record.push_back(static_cast<uint8_t>(v));
			break;
		}
		case DataType::STRING: {
			uint16_t len = val_str.size();
			record.insert(record.end(), reinterpret_cast<uint8_t*>(&len),
						  reinterpret_cast<uint8_t*>(&len) + sizeof(uint16_t));
			record.insert(record.end(), val_str.begin(), val_str.end());
			break;
		}
		}
	}

	// inserting into table
	try {
		RecordId rid = db.get_table_manager().insert(table_id, record);

		const auto& indices = table_info_opt->indexes;
		if (!indices.empty()) {
			size_t offset = 0;
			for (const auto& col : schema) {
				// If there's an index on this column
				auto idx_it =
					std::find_if(indices.begin(), indices.end(),
								 [&](const IndexInfo& idx) { return idx.column == col.name; });
				if (idx_it != indices.end()) {
					// Extract value for this column
					switch (col.type_) {
					case DataType::INT: {
						int v;
						std::memcpy(&v, &record[offset], sizeof(int));
						db.get_index_manager().insert(query.table_name(), col.name, v, rid);
						break;
					}
					case DataType::FLOAT: {
						float v;
						std::memcpy(&v, &record[offset], sizeof(float));
						db.get_index_manager().insert(query.table_name(), col.name, v, rid);
						break;
					}
					case DataType::BOOL: {
						bool v = record[offset];
						db.get_index_manager().insert(query.table_name(), col.name, v, rid);
						break;
					}
					case DataType::STRING: {
						uint16_t len;
						std::memcpy(&len, &record[offset], sizeof(uint16_t));
						offset += sizeof(uint16_t);
						std::string v(reinterpret_cast<const char*>(&record[offset]), len);

						db.get_index_manager().insert<std::string>(query.table_name(), col.name, v,
																   rid);
						offset += len; // string case has extra shift
						continue;	   // skip generic offset update below
					}
					}
				}

				// Move offset even if column is not indexed
				switch (col.type_) {
				case DataType::INT:
					offset += sizeof(int);
					break;
				case DataType::FLOAT:
					offset += sizeof(float);
					break;
				case DataType::BOOL:
					offset += sizeof(uint8_t);
					break;
				case DataType::STRING: {
					uint16_t len;
					std::memcpy(&len, &record[offset], sizeof(uint16_t));
					offset += sizeof(uint16_t) + len;
					break;
				}
				}
			}
		}
		return QueryResult(1)();
	} catch (const std::exception& e) {
		return QueryResult(false, e.what())();
	}
}

std::string Executor::exec_select(const SelectQuery& query) {
	if (!query.joins().empty()) {
		return exec_join(query);
	}

	try {
		SemanticVerifier verifier(db.getCatalog());
		verifier.verify_select_query(query);
	} catch (const std::exception& e) {
		return QueryResult(false, e.what())();
	}

	auto& table_manager = db.get_table_manager();
	// linear scan of the table
	auto table_id = table_manager.get_table_id(query.from());

	auto schema = sorted_schema(db.getCatalog().getTable(query.from()).value());

	const auto& selected_cols = query.columns();

	std::vector<RecordId> record_ids = table_manager.scan(table_id);

	QueryResult results(selected_cols);

	for (const auto& rid : record_ids) {
		DecodedRow col_values = decode_record(schema, table_manager.get(table_id, rid));

		std::vector<std::string> row;
		for (const auto& col : selected_cols) {
			row.push_back(column_to_string(col_values, col));
		}
		results.add_row(row);
	}
	return (results());
}

std::string Executor::exec_create_index(const CreateIndexQuery& q) {
	try {
		SemanticVerifier verifier(db.getCatalog());
		verifier.verify_create_index(q);
	} catch (const std::exception& e) {
		return QueryResult(false, e.what())();
	}
	this->db.get_index_manager().create_index(q.table_name(), q.column_name());

	//1-  we need to insert the keys into the index

	int	  table_id		 = db.get_table_manager().get_table_id(q.table_name());
	auto  table_info_opt = db.getCatalog().getTable(q.table_name());
	auto& schema		 = table_info_opt->columns;

	// let's order the schema first
	std::sort(
		schema.begin(), schema.end(),
		[](const ColumnDefinition& a, const ColumnDefinition& b) { return (a.name < b.name); });

	// 2- Find the column definition for the indexed column
	auto it = std::find_if(schema.begin(), schema.end(), [&](const ColumnDefinition& col) {
		return col.name == q.column_name();
	});
	ColumnDefinition indexed_col = *it;

	// 3. Scan all record IDs
	std::vector<RecordId> record_ids = db.get_table_manager().scan(table_id);

	// 4. For each record, extract the key column and insert into index
	for (const auto& rid : record_ids) {
		auto record = db.get_table_manager().get(table_id, rid);

		// Deserialize record fields
		size_t offset = 0;
		for (const auto& col : schema) {
			if (offset >= record.size())
				break;

			if (col.name == indexed_col.name) {
				// Indexed column → extract its value
				switch (col.type_) {
				case DataType::INT: {
					int v;
					std::memcpy(&v, &record[offset], sizeof(int));
					db.get_index_manager().insert<int>(q.table_name(), q.column_name(), v, rid);
					break;
				}
				case DataType::FLOAT: {
					float v;
					std::memcpy(&v, &record[offset], sizeof(float));
					db.get_index_manager().insert<float>(q.table_name(), q.column_name(), v, rid);
					break;
				}
				case DataType::BOOL: {
					bool v = record[offset];
					db.get_index_manager().insert<bool>(q.table_name(), q.column_name(), v, rid);
					break;
				}
				case DataType::STRING: {
					uint16_t len;
					std::memcpy(&len, &record[offset], sizeof(uint16_t));
					offset += sizeof(uint16_t);

					std::string v(reinterpret_cast<const char*>(&record[offset]), len);
					db.get_index_manager().insert<std::string>(q.table_name(), q.column_name(), v,
															   rid);
					break;
				}
				}
			}

			// Advance offset regardless
			switch (col.type_) {
			case DataType::INT:
				offset += sizeof(int);
				break;
			case DataType::FLOAT:
				offset += sizeof(float);
				break;
			case DataType::BOOL:
				offset += sizeof(uint8_t);
				break;
			case DataType::STRING: {
				uint16_t len;
				std::memcpy(&len, &record[offset], sizeof(uint16_t));
				offset += sizeof(uint16_t) + len;
				break;
			}
			}
		}
	}

	return QueryResult(true, "INDEX CREATED SUCCESSFULLY")();
}

std::string Executor::exec_join(const SelectQuery& query) {
	try {
		SemanticVerifier verifier(db.getCatalog());
		verifier.verify_join_query(query);
	} catch (const std::exception& e) {
		return QueryResult(false, e.what())();
	}

	try {
		auto&				 tm	  = db.get_table_manager();
		const Join&			 join = query.joins().front();
		const JoinCondition& cond = join.condition;

		TableInfo left_info	 = db.getCatalog().getTable(query.from()).value();
		TableInfo right_info = db.getCatalog().getTable(join.table_name).value();

		// The ON condition may be written in either order: orient it as FROM = JOIN
		bool		left_first = (cond.left_table == query.from());
		std::string left_key   = left_first ? cond.left_column : cond.right_column;
		std::string right_key  = left_first ? cond.right_column : cond.left_column;

		// FROM table is the outer loop, JOIN table is the inner loop
		JoinSide outer = make_join_side(tm, left_info, left_key);
		JoinSide inner = make_join_side(tm, right_info, right_key);

		JoinedRows rows;
		// TODO (Lab 3): choose the join algorithm.
		//  - right_info.indexes has an index on inner.key_column -> index_nested_loop_join
		//  - otherwise                                          -> nested_loop_join
		// Set last_join_algorithm_ to JoinAlgorithm::INDEX_NESTED_LOOP or NESTED_LOOP.



		bool has_index = false;
		for (auto index : right_info.indexes) {
			if (index.column == inner.key_column) {
				has_index = true;
				break;
			}
		}
		if (has_index) {
			rows = index_nested_loop_join(tm, db.get_index_manager(), outer, inner);
			last_join_algorithm_ = JoinAlgorithm::INDEX_NESTED_LOOP;
		}
		else {
			rows = nested_loop_join(tm, outer, inner);
			last_join_algorithm_ = JoinAlgorithm::NESTED_LOOP;
		}

		// Resolve each selected column to (side, column name)
		std::vector<std::pair<bool, std::string>> projection; // (from outer?, column)
		for (const auto& col : query.columns()) {
			size_t dot = col.find('.');
			if (dot != std::string::npos) {
				projection.emplace_back(col.substr(0, dot) == query.from(), col.substr(dot + 1));
			} else {
				bool in_outer = std::any_of(outer.schema.begin(), outer.schema.end(),
											[&](const ColumnDefinition& c) { return c.name == col; });
				projection.emplace_back(in_outer, col);
			}
		}

		QueryResult results(query.columns());
		for (const auto& [o, i] : rows) {
			std::vector<std::string> row;
			for (const auto& [from_outer, col] : projection) {
				row.push_back(column_to_string(from_outer ? o : i, col));
			}
			results.add_row(row);
		}
		return results();
	} catch (const std::exception& e) {
		return QueryResult(false, e.what())();
	}
}

NljRun run_nlj_flavor(DataBase& db, const std::string& outer_table, const std::string& outer_key,
					  const std::string& inner_table, const std::string& inner_key,
					  NljFlavor flavor, size_t pool_frames) {
	TableInfo outer_info = db.getCatalog().getTable(outer_table).value();
	TableInfo inner_info = db.getCatalog().getTable(inner_table).value();

	// Write everything to disk, then join through a fresh, empty pool of pool_frames frames
	db.get_buffer_manager().flush_all();
	BufferManager bm(pool_frames);
	TableManager  tm(bm);
	std::string	  dir = Config::get_instance()->datadir;
	tm.create_table(outer_table, dir + "/" + outer_table + ".tbl");
	tm.create_table(inner_table, dir + "/" + inner_table + ".tbl");
	JoinSide outer = make_join_side(tm, outer_info, outer_key);
	JoinSide inner = make_join_side(tm, inner_info, inner_key);

	HeapFile::reads = 0;
	auto	   t0	= std::chrono::steady_clock::now();
	JoinedRows rows;
	switch (flavor) {
	case NljFlavor::TUPLE:
		rows = nested_loop_join(tm, outer, inner);
		break;
	case NljFlavor::PAGE:
		rows = page_nested_loop_join(bm, outer, inner);
		break;
	case NljFlavor::CHUNK:
		rows = chunk_nested_loop_join(bm, outer, inner, pool_frames - 2);
		break;
	}
	auto t1 = std::chrono::steady_clock::now();

	NljRun run{{}, HeapFile::reads, bm.peak_frames(),
			   std::chrono::duration<double, std::milli>(t1 - t0).count()};
	auto signature = [](const JoinSide& side, const DecodedRow& row) {
		std::string s;
		for (const auto& col : side.schema) {
			s += (s.empty() ? "" : ",") + column_to_string(row, col.name);
		}
		return s;
	};
	for (const auto& [o, i] : rows) {
		run.rows.push_back(signature(outer, o) + " | " + signature(inner, i));
	}
	std::sort(run.rows.begin(), run.rows.end());
	return run;
}
