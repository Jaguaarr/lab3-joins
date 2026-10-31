# MiniDB - Educational Relational Database Management System

MiniDB is an educational relational database management system designed to help students learn about the core components of database systems.
The project focuses on:

1. **Storage Management**: Buffer pool management and disk I/O
2. **Indexing**: B+ tree implementation for efficient data access
3. **Query Processing**: Basic SQL parsing and execution
4. **Query Optimization**: Simple optimization strategies

## Project Structure

```
minidb/
├── .github/workflows     # github actions
├── docs/                 # Doxygen configuration
├── include/              # Public header files
├── src/                  # Source files for the database
├── tests/                # Unit and integration tests
├── .clang-format         # Clang-format configuration
├── .clang-format-ignore  # Files/directories to ignore when formatting
├── .gitignore            # Git ignore file
├── CMakeLists.txt        # CMake build script
├── LICENSE               # MIT License file
├── README.md             # Project overview and instructions

```

## Lab 3

See [LAB3.md](LAB3.md) for the JOIN assignment.

## Building the Project

### Prerequisites

- C++17 compatible compiler (GCC, Clang, etc.)
- CMake 3.10 or higher
- `clang-format` >= 18.1.0 (optional)
- `doxygen` >=1.14.0 (optional)

### Build Instructions

1. Clone the repository:

   ```
   git clone https://github.com/yourusername/minidb.git
   cd minidb
   ```

2. Create a build directory:

   ```
   mkdir build
   cd build
   ```

3. Run CMake and build:
   ```
   cmake -DENABLE_THREADPOOL=OFF ..
   make
   ```

## Running MiniDB

After building, you can run MiniDB from the build directory:

```
./bin/minidb_bin --help
```

To shut down the server, press `Ctrl+D` in the terminal.

## Supported SQL Commands

MiniDB supports a subset of SQL commands:

- `CREATE TABLE table_name (column1 type1, column2 type2, ...)`
- `INSERT INTO table_name (column1, column2, ...) VALUES (value1, value2, ...)`
- `SELECT column1, column2, ... FROM table_name`

## Testing

The project includes tests for each component. To run the tests:

```
cd build
make test
```

## Documentation

To generate Doxygen documentation

```
cd build
make doc_doxygen
python3 -m http.server -d doxygen/html
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.
