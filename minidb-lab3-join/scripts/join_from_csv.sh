#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build-stdin"
BIN="${BUILD_DIR}/bin/minidb_bin"
DATA_DIR="${REPO_ROOT}/demo-data"

CSV_PRODUCTS="${SCRIPT_DIR}/products.csv"
CSV_SALES="${SCRIPT_DIR}/sales.csv"
rm -rf "${BUILD_DIR}" "${DATA_DIR}"
mkdir -p "${BUILD_DIR}" "${DATA_DIR}"
pushd "${BUILD_DIR}" >/dev/null
cmake -DENABLE_THREADPOOL=OFF .. >/dev/null
cmake --build . --config Release -j >/dev/null
popd >/dev/null

# Emit SQL
{
	echo "CREATE TABLE products (product_id INT, product_name STRING, price INT);"
	echo "CREATE TABLE sales (product_id INT, quantity INT);"

	# Insert products (skip header)
	awk -F',' 'NR>1 && NF>=3 { printf("INSERT INTO products (product_id, product_name, price) VALUES (%s, %s, %s);\n", $1, $2, $3) }' "${CSV_PRODUCTS}"
	# Insert sales (skip header)
	awk -F',' 'NR>1 && NF>=2 { printf("INSERT INTO sales (product_id, quantity) VALUES (%s, %s);\n", $1, $2) }' "${CSV_SALES}"

	# Build index on right side join key (engine will use it if present)
	echo "CREATE INDEX ON sales product_id"

	# Join
	echo "SELECT products.product_id, sales.quantity FROM products JOIN sales ON products.product_id = sales.product_id;"
} | "${BIN}" --datadir "${DATA_DIR}"


