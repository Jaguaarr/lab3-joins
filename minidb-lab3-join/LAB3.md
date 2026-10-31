# Lab 3 — JOIN queries

## Goals

Extend MiniDB so that it can run an **inner equi-join** of two tables, end to end:

```sql
SELECT products.product_id, sales.quantity
FROM products JOIN sales ON products.product_id = sales.product_id;
```

You will work on every stage of query processing:

1. **Parse** the `JOIN ... ON ...` clause into the `Join` structure of `SelectQuery`.
2. **Verify** the join against the catalog (tables, columns, types, ambiguity).
3. **Execute** it with two algorithms, chosen automatically:
   - **Nested loop join (NLJ)**: no index on the inner table's join column.
   - **Index nested loop join (INLJ)**: the inner table has a B+ tree index on its join column.
4. **Compare** three flavors of nested loop join (tuple, page and chunk) and write a short benchmark report.

The buffer manager and B+ tree from Labs 1 and 2 are provided and working.

## Build

```bash
mkdir -p build && cd build
cmake -DENABLE_THREADPOOL=OFF ..
cmake --build . -j
```

`-DENABLE_THREADPOOL=OFF` is required: the JOIN tests run the database in stdin mode.

## Read first

| File | Why |
|------|-----|
| `include/minidb/query.hpp` | `SelectQuery`, `Join`, `JoinCondition` (already defined) |
| `src/parser.cpp` | `parse_select` finds the JOIN clause and calls `parse_join` |
| `src/semantic_verifier.cpp` | `verify_select_query` shows the error style to follow |
| `src/executor.cpp` | `decode_record`, `JoinSide`, `index_lookup` helpers; `exec_join` |
| `include/minidb/index/b_plus_tree.hpp` | `IndexManager::range_search` |
| `tests/test_join.cpp` | What is graded |
| `tests/bench_join.cpp` | Benchmark for the NLJ flavors (requirement 4) |

## Supported syntax

```sql
SELECT <cols> FROM <left> [INNER] JOIN <right> ON <a>.<col> = <b>.<col>[;]
```

- Keywords are case-insensitive (`join`, `INNER JOIN`, `on`).
- The ON condition must be qualified (`table.column`) and may be written in either order (`sales.product_id = products.product_id` is valid).
- Selected columns are either qualified (`products.price`) or bare (`price`) when the name exists in only one of the two tables.
- One JOIN per query, no self-join, no WHERE on joins.

## Requirements

### 1. Parser: `Parser::parse_join` (`src/parser.cpp`)

Input is the clause starting at `JOIN`, e.g. `JOIN sales ON products.product_id = sales.product_id;`.

- Use `tokenize`. Strip a trailing `;`.
- Expect exactly: `JOIN <table> ON <t>.<c> = <t>.<c>`.
- Return `Join(table, JoinType::INNER, JoinCondition(lt, lc, rt, rc))`, keeping the condition in the order it was written.
- Throw `std::runtime_error` for anything malformed (missing `ON`, missing `=`, unqualified or badly qualified columns, extra tokens).

### 2. Semantic checks: `SemanticVerifier::verify_join_query` (`src/semantic_verifier.cpp`)

Throw `std::runtime_error` when:

- the query has more than one join, or a non-INNER join;
- either table does not exist, or both sides are the same table;
- the ON condition does not compare one column of each table;
- an ON column does not exist;
- the two join columns have different types;
- a selected column is unknown, refers to another table, or is ambiguous.

### 3. Execution (`src/executor.cpp`)

`exec_join` orients the condition, builds the two `JoinSide`s (FROM table = **outer**, JOIN table = **inner**), and projects the selected columns. You implement:

- **Algorithm choice** in `exec_join`: use INLJ if the inner table has an index on its join column, otherwise NLJ. Set `last_join_algorithm_` to the algorithm used.
- **`nested_loop_join`**: for every outer tuple, scan every inner tuple and keep the pairs whose join keys are equal.
- **`index_nested_loop_join`**: for every outer tuple, probe the inner index with `index_lookup` (it handles duplicate keys) and fetch each matching inner tuple by its `RecordId`.

Each returns the `(outer row, inner row)` pairs. Row order is not graded.

### 4. Nested loop join flavors and benchmark report

The NLJ above works **one tuple at a time**: it re-reads the whole inner table for every outer tuple. Real systems read the outer table in bigger units so the inner table is scanned fewer times. You add two flavors (page and chunk) and measure all three with the provided benchmark.

Notation: the outer table has `M` pages and `p_R` tuples per page, the inner table has `N` pages, and the buffer pool has `B` frames.

| Flavor | Outer unit | Inner table is scanned | Page reads (I/O) |
|--------|-----------|------------------------|------------------|
| Tuple NLJ (`nested_loop_join`, above) | 1 tuple | once per outer **tuple** | `M + (M · p_R) · N` |
| Page NLJ | 1 page | once per outer **page** | `M + M · N` |
| Chunk (block) NLJ | `B − 2` pages | once per outer **chunk** | `M + ⌈M / (B − 2)⌉ · N` |

**Why chunks of `B − 2` pages.** The pool has `B` frames. The join needs one frame for the inner page it is scanning and one frame for the output, and every other frame can hold outer pages. Filling `B − 2` frames with outer pages means each inner scan is shared by as many outer tuples as fit in memory. The inner table, usually the larger cost, is then read `⌈M / (B − 2)⌉` times instead of `M · p_R` times. With `B = 3`, the default pool, a chunk is 1 page, so chunk NLJ equals page NLJ. That is why the pool size must change in the benchmark.

#### What to implement (`src/executor.cpp`)

Two stubs are provided next to `nested_loop_join`. **Keep their names and signatures**: the benchmark calls them.

```cpp
JoinedRows page_nested_loop_join(BufferManager& bm, const JoinSide& outer, const JoinSide& inner);
JoinedRows chunk_nested_loop_join(BufferManager& bm, const JoinSide& outer, const JoinSide& inner,
                                  size_t chunk_pages);   // chunk_pages = B - 2
```

- Work on pages directly: `bm.get_heap_file(table_id)->get_num_pages()`, `bm.fetch_page(PageId{table_id, page_no})`, then `SlottedPage` (`get_num_slots`, `is_slot_valid`, `get_record`) and `decode_record`.
- **Page NLJ:** for each outer page, scan every inner page and join the two pages' tuples.
- **Chunk NLJ:** pin up to `chunk_pages` outer pages, scan the inner table once page by page, join every inner tuple with every outer tuple of the chunk, then unpin the chunk and load the next one.
- Keep the outer tuples only while their pages are pinned. Loading the whole outer table into memory bypasses the buffer pool and defeats the experiment.
- Never pin more than `chunk_pages + 1` pages at the same time. The output is kept in memory (`JoinedRows`); it stands for the output frame.
- Unpin every page you fetch. All three flavors must return the same rows.
- `exec_join` keeps calling the tuple `nested_loop_join`, so CI is unchanged.
- Optional: inside a chunk, put the outer keys in a hash map so each inner tuple is matched in O(1). Report whether it changes the time (it does not change the I/O).

#### Benchmark (provided: `tests/bench_join.cpp`)

```bash
cd build
./bin/bench_join
```

It creates tables of 5, 10, 50, 50 and 100 full pages (45 tuples per page, random join keys), then runs each flavor on a **cold buffer pool of exactly `B` frames**. It is not part of CI. The three experiments are:

1. **flavors:** tuple vs page vs chunk on the same tables (`M = 5`, `N = 10`, `B = 4`).
2. **pool size:** chunk NLJ on two 50-page tables with `B` = 3, 4, 6, 10, 18, 34, 52 (page NLJ as reference).
3. **who is outer:** chunk NLJ with the 10-page table as outer, then with the 100-page table as outer, at `B` = 4 and 12.

For every run it prints:

| Column | Meaning |
|--------|---------|
| `rows`, `ok` | rows returned, and whether they are exactly the expected rows |
| `reads` | pages read from disk during the join (each `HeapFile::read_page` call) |
| `estimate` | the textbook formula from the table above |
| `peak` | most buffer frames held at once; `!` means more than `B` (too many pages pinned) |
| `ms` | wall-clock time of the join |

A correct implementation shows `ok = yes` and no `!` on every line, and its `reads` follow the formulas. You may add your own experiments (other sizes, other `B`, the hash-map variant) by editing the benchmark.

#### Report (`docs/lab3-report.md`, or a PDF in `docs/`, included in your PR)

Paste the benchmark output, then answer:

1. **Flavors:** how do `reads` and time change from tuple to page to chunk NLJ? Your tuple NLJ will likely read more pages than its estimate: explain where the extra reads come from.
2. **Pool size:** plot or tabulate `reads` against `B` for chunk NLJ. How does it follow `M + ⌈M / (B − 2)⌉ · N`? What happens when the outer table fits in `B − 2` frames? Why are `B = 3` chunk and page NLJ identical?
3. **Which table goes in the chunk:** the classic question is *"why fill the `B − 2` frames with the outer table and not the inner one, and which table should be outer?"* Answer with experiment 3 and the formula. Which order gives fewer reads, and why does the gap grow or shrink with `B`?
4. **Time vs I/O:** do the times follow the page reads? If not, what else costs time here (decoding, comparisons)?
5. **Conclusion:** which flavor a query engine should use, and when the index nested loop join beats all three.

Keep it short: 1–3 pages, numbers first, then the explanation.

### 5. Quality

- No crashes, leaks or stranded buffer pins. The buffer pool is only 3 frames by default, so a join that leaks pins or keeps pages alive fails fast.
- Optional check: build with `-DCMAKE_CXX_FLAGS="-fsanitize=address"` and run `./bin/test_join`.

## Run tests

```bash
cd build
./bin/test_join
# or
ctest --output-on-failure -R test_join
```

End-to-end demo (builds into `build-stdin/`, prints a 3-row result):

```bash
bash scripts/join_from_csv.sh
```

## Submit

1. Clone this lab repo (you were added as a collaborator).
2. Create a branch, implement the TODOs.
3. Implement the two NLJ flavors, run `./bin/bench_join` and add the report (`docs/lab3-report.md` or a PDF in `docs/`).
4. Open a **Pull Request** into this repository's `main`.
5. CI must be green.

## Grading signal (CI)

| Check | What it covers |
|-------|----------------|
| Parsing | `JOIN ... ON` parsed into `Join`, malformed clauses rejected |
| Semantic checks | unknown table / column, type mismatch, ambiguity |
| Nested loop join | small join, reversed ON, empty result, STRING keys |
| Index nested loop join | chosen when the inner index exists; same rows as NLJ on 900 rows with duplicate keys |
| Algorithm choice | index on the outer table only falls back to NLJ |
| Demo script | `scripts/join_from_csv.sh` returns 3 rows |

The NLJ flavors (requirement 4) and the report are not checked by CI. Check them yourself with `./bin/bench_join`; the instructors grade them from your code, the benchmark output and your report.
