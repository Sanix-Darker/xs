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
)

file(GLOB GUMBO_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gumbo_src/*.c")

add_executable(xs
    main.c
    network.c
    parser.c
    layout.c
    render.c
    javascript.c
    duktape.c
    css.c
    # gumbo sources (for parsing, because SKILL ISSUES)
    ${GUMBO_SOURCES}
)

target_link_libraries(xs
    ${CURL_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    m # <- this shit was hard as fuck to figured out
)
