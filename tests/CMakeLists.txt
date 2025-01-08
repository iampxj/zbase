
if (WINDOWS)
    set(CURRENT_TARGET ${TargetName})
else ()
    set(CURRENT_TARGET basework)
endif()

if (NOT WINDOWS)
    target_sources(${CURRENT_TARGET}
        PRIVATE
        # ${CMAKE_CURRENT_SOURCE_DIR}/rq_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/fs_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/ccinit_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/blkdev_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/fw_selfwrite_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/partition_test.cc
        #${CMAKE_CURRENT_SOURCE_DIR}/ota_fstream_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/msg_storage_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/async_call_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/ptfs_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/pagefs_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/xipfs_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/lua_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/rb_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/ahash_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/circlebuf_test.cc
        # ${CMAKE_CURRENT_SOURCE_DIR}/fifofs_test.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/idr_test.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/libenv_test.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/flash_kv_test.cc
    )
endif()

# target_sources(${CURRENT_TARGET}
#     PRIVATE
#     # ${CMAKE_CURRENT_SOURCE_DIR}/timer_test.cc
#     ${CMAKE_CURRENT_SOURCE_DIR}/env_test.cc
# )
