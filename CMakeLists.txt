cmake_minimum_required(VERSION 3.10)
project(xs LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(XS_SANITIZE "Build with AddressSanitizer + UBSan" OFF)

if(XS_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

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

# pkg_check_modules gives library *names* but not their search paths; add the
# link directories so the linker can find SDL2_ttf (and friends).
link_directories(${SDL2_TTF_LIBRARY_DIRS} ${SDL2_LIBRARY_DIRS})

file(GLOB GUMBO_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gumbo_src/*.c")

# Vendored MuJS and Gumbo emit many -Wall/-Wextra warnings; silence them so our
# own code's warnings stand out. Applied per-source-group below.
set(VENDOR_FLAGS "-w")

# ---- Core engine sources shared by the app and the test binary ----------
# (Render/network/main are app-only: render needs a window, network needs curl.)
set(XS_CORE_SOURCES
    util.c
    arena.c
    parser.c
    tagid.c
    css.c
    style.c
    select.c
    encoding.c
    url.c
    table.c
    forms.c
    ui.c
    layout.c
    javascript.c
    document.c
)

# ---- Main application ----------------------------------------------------
add_executable(xs
    main.c
    network.c
    render.c
    ${XS_CORE_SOURCES}
    mujs/one.c
    ${GUMBO_SOURCES}
)

target_link_libraries(xs
    ${CURL_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    m
)

# ---- Tests ---------------------------------------------------------------
enable_testing()

add_executable(xs_tests
    tests/test_main.c
    tests/test_core.c
    tests/test_util.c
    tests/test_arena.c
    tests/test_parser.c
    tests/test_css.c
    tests/test_style.c
    tests/test_select.c
    tests/test_encoding.c
    tests/test_url.c
    tests/test_js.c
    tests/test_table.c
    tests/test_forms.c
    tests/test_ui.c
    tests/test_layout.c
    tests/test_network.c
    ${XS_CORE_SOURCES}
    network.c
    mujs/one.c
    ${GUMBO_SOURCES}
)

# Tests run headless: they may link SDL2_ttf for measurement but must never
# open a window. Enable the test-only allocation hooks via XS_TESTING.
target_compile_definitions(xs_tests PRIVATE XS_TESTING=1)

target_link_libraries(xs_tests
    ${CURL_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    m
)

add_test(NAME xs_tests COMMAND xs_tests)

# Golden + smoke tests run the real xs binary headlessly over fixtures.
add_test(NAME xs_golden
    COMMAND ${CMAKE_COMMAND} -E env sh
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_golden.sh
            $<TARGET_FILE:xs>
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden)

add_test(NAME xs_smoke
    COMMAND ${CMAKE_COMMAND} -E env sh
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_smoke.sh
            $<TARGET_FILE:xs>
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures)

# Fuzz/crash-corpus harness (testing/fuzzing.md). Build with -DXS_SANITIZE=ON
# and run over fixtures: ./xs_fuzz tests/fixtures/*.html
add_executable(xs_fuzz
    tests/fuzz_harness.c
    ${XS_CORE_SOURCES}
    network.c
    mujs/one.c
    ${GUMBO_SOURCES}
)
target_compile_definitions(xs_fuzz PRIVATE XS_TESTING=1)
target_link_libraries(xs_fuzz
    ${CURL_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    m
)

# Silence warnings from vendored code only.
set_source_files_properties(${GUMBO_SOURCES} mujs/one.c
    PROPERTIES COMPILE_FLAGS "${VENDOR_FLAGS}")
