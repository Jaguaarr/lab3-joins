cmake_minimum_required(VERSION 3.12)

project(minidb)

set(CMAKE_CXX_STANDARD 17)


##############################
# Enable testing
##############################
enable_testing()

##############################
# Set output directory for binaries
# and Libraries
##############################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)


option(ENABLE_THREADPOOL "Enable multithreaded server mode" ON)

if (ENABLE_THREADPOOL)
	add_compile_definitions(ENABLE_THREADPOOL)
endif()

#############################
# minidb static library
#############################

add_library(minidb STATIC
	src/config.cpp
	src/network/connection.cpp
	src/network/connqueue.cpp
	src/network/threadpool.cpp
	src/network/server.cpp 
	src/utils.cpp
	src/database.cpp
	src/parser.cpp
	src/catalog.cpp
	src/table.cpp
	src/buffer_manager/buffer_manager.cpp
	src/buffer_manager/heapfile.cpp
	src/buffer_manager/page.cpp
	src/table_manager/table_manager.cpp
	src/executor.cpp
	src/semantic_verifier.cpp
	src/query_results.cpp
	src/id_generator.cpp
)

target_include_directories(minidb
	PUBLIC
		${PROJECT_SOURCE_DIR}/include
)


##############################
# minidb executable
##############################

add_executable(minidb_bin
	src/main.cpp
)

target_link_libraries(minidb_bin
	PRIVATE
		minidb
)

##############################
# client executable
##############################

add_executable(client
	src/client/client.cpp
)

##############################
# Adding tests
##############################
add_subdirectory(tests)

###############################
# Genrating docs
###############################

find_package(Doxygen)

if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(doc_doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()

##############################
# clang-format
##############################
find_program(CLANG_FORMAT_EXE NAMES clang-format)

if (CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_SOURCE_DIR}/include/*.h"
        "${CMAKE_SOURCE_DIR}/include/*.hpp"
        "${CMAKE_SOURCE_DIR}/tests/*.cpp"
    )

	list(FILTER ALL_SOURCE_FILES EXCLUDE REGEX ".*/nlohmann/.*")

    add_custom_target(format
    	COMMENT "Formatting source files with clang-format"
	)

    # Target to check formatting (used in CI)
    add_custom_target(check-format
        COMMENT "Checking code formatting with clang-format"
    )

    foreach(file ${ALL_SOURCE_FILES})
        # Format target (modifies files)
        add_custom_command(TARGET format
            PRE_BUILD
            COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${file}
            COMMENT "Formatting ${file}"
        )

        # Check target (fails if unformatted)
        add_custom_command(TARGET check-format
            PRE_BUILD
            COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror -style=file ${file}
            COMMENT "Checking formatting for ${file}"
        )
    endforeach()
else()
    message(WARNING "clang-format not found. Format and check-format targets will not be available.")
endif()
