zephyr_library_named(basework)

zephyr_include_directories(
    cpu/cortexm
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)

if (CONFIG_KASAN)
    add_library(kasan INTERFACE )
    target_compile_options(kasan
        INTERFACE
        $<$<CXX_COMPILER_ID:GNU>:-fsanitize=kernel-address>
        $<$<CXX_COMPILER_ID:ARMClang>:-fsanitize=address>
    )
    target_link_libraries(basework PRIVATE kasan)
endif(CONFIG_KASAN)

if (CONFIG_FTRACE)
    add_library(fntrace INTERFACE)
    target_compile_options(fntrace
        INTERFACE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:ARMClang>>:-finstrument-functions>
    )
    target_link_libraries(basework PRIVATE fntrace)
endif()

zephyr_library_compile_options(
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:ARMClang>>:-Wextra>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:ARMClang>>:-Wextra>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:ARMClang>>:-Wno-unused-parameter>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:ARMClang>>:-Wno-implicit-fallthrough>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:ARMClang>>:-Wno-missing-field-initializers>
#    $<$<CXX_COMPILER_ID:GNU>:-Wno-cast-function-type>
)

zephyr_library_sources_ifndef(CONFIG_BOOTLOADER 
    rq.c
    log.c
    fsm.c
    async_call.c
)

if (NOT CONFIG_BOOTLOADER)
zephyr_library_sources(
    modmgr.c
	system.c
    params.c
    module.c
)
zephyr_library_sources_ifdef(CONFIG_FLASHTRACE
    tools/flash_trace.c
)
endif()

if (WINDOWS OR LINUX)
zephyr_library_sources(ccinit.cc)
endif()

add_subdirectory(container)
add_subdirectory(lib)
add_subdirectory(dev)
add_subdirectory(os)
add_subdirectory(utils)
# add_subdirectory(misc)
add_subdirectory_ifndef(CONFIG_BOOTLOADER package)
add_subdirectory_ifndef(CONFIG_BOOTLOADER thirdparty)
add_subdirectory_ifdef(CONFIG_BASEWORK_TEST tests)

if (NOT CONFIG_BOOTLOADER)
if (NOT WINDOWS)
	add_subdirectory(drive)
endif()

if (NOT WINDOWS)
    add_subdirectory(debug)
    if (NOT LINUX)
        add_subdirectory_ifdef(CONFIG_BASEWORK_UI ui)
    endif()
endif()
endif(NOT CONFIG_BOOTLOADER)

add_subdirectory_ifdef(CONFIG_BOOTLOADER boot)
