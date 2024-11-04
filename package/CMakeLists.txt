if (CONFIG_SCREEN_MANAGER)
target_sources(basework
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/screen_manager.c
)
endif()