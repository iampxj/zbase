
zephyr_library_named(googletest)

zephyr_include_directories(include)
zephyr_library_include_directories(gtest
  PRIVATE
  .
)

zephyr_library_sources(
  src/gtest-all.cc
  src/gtest_main.cc
)

if (NOT WINDOWS AND NOT LINUX)
  zephyr_library_sources(
    googletest_zephyr.c
  )
endif()
