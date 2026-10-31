
function(add_minidb_test name source)
	add_executable(${name} ${source})
	target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/include)
	target_link_libraries(${name} PRIVATE minidb)
	add_test(NAME ${name} COMMAND ${name})
endfunction()

add_minidb_test(test_page test_page.cpp)
add_minidb_test(test_heapfile test_heapfile.cpp)
add_minidb_test(test_buffer_manager test_buffer_manager.cpp)
add_minidb_test(test_table_manager test_table_manager.cpp)
add_minidb_test(test_catalog_manager test_catalog.cpp)
add_minidb_test(test_key_serialize_deserialize test_keys.cpp)
add_minidb_test(test_bptree_node test_bptree_node.cpp)
add_minidb_test(test_bptree test_bptree.cpp)
add_minidb_test(test_index_manager test_index_manager.cpp)
add_minidb_test(test_id_generator test_id_generator.cpp)

add_minidb_test(test_join test_join.cpp)
# test_join drives the stdin-mode DataBase; it skips (77) in thread-pool builds
set_tests_properties(test_join PROPERTIES SKIP_RETURN_CODE 77)

# Lab 3 NLJ benchmark (tuple / page / chunk): run by hand, not part of ctest or CI
add_executable(bench_join bench_join.cpp)
target_include_directories(bench_join PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(bench_join PRIVATE minidb)
