#ifndef __SEMANTIC_VERIFIER_HPP__
#define __SEMANTIC_VERIFIER_HPP__

class CatalogManager;
class InsertQuery;
class SelectQuery;
class CreateIndexQuery;

/**
 * @class SemanticVerifier
 * @brief Performs semantic validation of SQL queries using the catalog metadata.
 *
 * This class checks the correctness of queries such as INSERT and SELECT by
 * verifying their consistency with the database schema managed by the CatalogManager.
 */
class SemanticVerifier {
  private:
	const CatalogManager& catalog;

  public:
	SemanticVerifier(const CatalogManager&);
	void verify_insert_query(const InsertQuery& q) const;
	void verify_select_query(const SelectQuery& q) const;
	void verify_join_query(const SelectQuery& q) const;
	void verify_create_index(const CreateIndexQuery& q) const;
};

#endif
