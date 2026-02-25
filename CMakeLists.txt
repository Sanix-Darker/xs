cmake_minimum_required(VERSION 3.10)
project(xs LANGUAGES C)

find_package(CURL REQUIRED)
find_package(SDL2 REQUIRED)

# Use pkg-config to locate SDL2_ttf.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

include_directories(
    ${CURL_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mujs
)

file(GLOB GUMBO_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gumbo_src/*.c")

add_executable(xs
    main.c
    network.c
    parser.c
    layout.c
    render.c
    javascript.c
    mujs/one.c
    css.c
    ${GUMBO_SOURCES}
)

target_link_libraries(xs
    ${CURL_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    m
)

enable_testing()

add_executable(xs_tests
    tests/test_core.c
    parser.c
    css.c
    layout.c
    javascript.c
    mujs/one.c
    ${GUMBO_SOURCES}
)

target_link_libraries(xs_tests
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    m
)

add_test(NAME xs_tests COMMAND xs_tests)
