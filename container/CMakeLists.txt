zephyr_library_sources(
    ${CMAKE_CURRENT_SOURCE_DIR}/observer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/kfifo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ahash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/circ_buffer.c

    ${CMAKE_CURRENT_SOURCE_DIR}/radix-tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rbtree/rb_prepend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rbtree/rb_extract.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rbtree/rb_max.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rbtree/rb_iterate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rbtree/rb_append.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rbtree/rb_prev.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rbtree/rb_postorder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rbtree/rb_replace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rbtree/rb_min.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rbtree/rb_insert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rbtree/rb_next.c
)

add_subdirectory(ring)
