cmake_minimum_required(VERSION 3.20)
project(intrinsic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(INTRINSIC_ENABLE_SANITIZERS "Enable ASan/UBSan instrumentation" OFF)

if(INTRINSIC_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-fsanitize=address,undefined
                            -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(WARNING "Sanitizers requested, but compiler is not Clang/GNU")
    endif()
endif()

find_package(Curses REQUIRED)
find_package(SQLite3 REQUIRED)
include(CTest)

set(INTRINSIC_CURSES_INCLUDES ${CURSES_INCLUDE_DIRS})
if(NOT INTRINSIC_CURSES_INCLUDES AND CURSES_INCLUDE_DIR)
    list(APPEND INTRINSIC_CURSES_INCLUDES ${CURSES_INCLUDE_DIR})
endif()

set(INTRINSIC_CURSES_LIBS ${CURSES_LIBRARIES})
if(NOT INTRINSIC_CURSES_LIBS AND CURSES_LIBRARY)
    list(APPEND INTRINSIC_CURSES_LIBS ${CURSES_LIBRARY})
endif()

if(NOT INTRINSIC_CURSES_LIBS)
    message(FATAL_ERROR "Curses library was found but no link libraries were provided")
endif()

file(GLOB_RECURSE INTRINSIC_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(intrinsic ${INTRINSIC_SOURCES})

target_include_directories(intrinsic PRIVATE
    ${INTRINSIC_CURSES_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(intrinsic PRIVATE ${INTRINSIC_CURSES_LIBS} SQLite::SQLite3)

install(TARGETS intrinsic RUNTIME DESTINATION bin)

if(BUILD_TESTING)
    add_executable(
        intrinsic_tests EXCLUDE_FROM_ALL
        tests/test_main.cpp
        tests/database_test.cpp
        tests/settings_test.cpp
        tests/view_helpers_test.cpp
        tests/state_and_settings_helpers_test.cpp
        tests/key_handlers_test.cpp
        tests/reset_nuke_test.cpp
        src/db/database.cpp
        src/db/database_queries.cpp)

    target_include_directories(intrinsic_tests PRIVATE
        ${INTRINSIC_CURSES_INCLUDES}
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

    target_compile_definitions(intrinsic_tests PRIVATE INTRINSIC_TESTING=1)

    target_link_libraries(intrinsic_tests PRIVATE ${INTRINSIC_CURSES_LIBS}
                                                  SQLite::SQLite3)

    add_test(NAME intrinsic_tests COMMAND intrinsic_tests)
endif()
