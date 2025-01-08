zephyr_library_named(flashdb)

zephyr_library_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)
zephyr_library_compile_options(
    -Wno-unused-parameter
)

zephyr_library_sources(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fdb_file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fdb_kvdb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fdb_tsdb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fdb_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fdb.c
)
