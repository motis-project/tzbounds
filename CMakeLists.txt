cmake_minimum_required(VERSION 3.21)
project(tzbounds)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(cmake/pkg.cmake)
endif ()

set(tzbounds-timezones
        "${CMAKE_CURRENT_SOURCE_DIR}/timezones/combined-with-oceans-1970.json.gz")
create_resource("${CMAKE_CURRENT_SOURCE_DIR}/timezones" "${tzbounds-timezones}"
        tzbounds_timezones)

add_library(tzbounds src/timezones.cc)
target_compile_features(tzbounds PUBLIC cxx_std_23)
target_include_directories(tzbounds PUBLIC include)
target_link_libraries(tzbounds
        PUBLIC geo
        PRIVATE
        boost-iostreams
        boost-json
        rtree
        tg
        utl
        tzbounds_timezones
        tzbounds_timezones-res
)

add_executable(tzbounds-test test/timezones_test.cc)
target_link_libraries(tzbounds-test PRIVATE tzbounds gtest_main)
