add_library(bktest STATIC support/bk_test.c support/tetris_lab.c support/mini_json.c support/bk_digest.c support/frame_dump.c support/fake_hal.c support/stub_game.c support/conf_runner.c support/snake_lab.c support/racing_lab.c)
target_include_directories(bktest PUBLIC support ${CMAKE_SOURCE_DIR}/firmware/games/tetris ${CMAKE_SOURCE_DIR}/firmware/games/snake ${CMAKE_SOURCE_DIR}/firmware/games/racing)
target_link_libraries(bktest PUBLIC brickcore)
target_compile_options(bktest PRIVATE -Wall -Wextra -Wpedantic -Wshadow)

function(brick_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE bktest)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
  add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

brick_test(test_tetris_rules unit/test_tetris_rules.c)

add_executable(test_tetris_conformance conformance/test_tetris_conformance.c)
target_link_libraries(test_tetris_conformance PRIVATE bktest)
target_compile_options(test_tetris_conformance PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
add_test(NAME test_tetris_conformance
         COMMAND test_tetris_conformance ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tetris/tetris.conformance.json)

add_executable(test_snake_conformance conformance/test_snake_conformance.c)
target_link_libraries(test_snake_conformance PRIVATE bktest)
target_compile_options(test_snake_conformance PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
add_test(NAME test_snake_conformance
         COMMAND test_snake_conformance ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/snake/snake.conformance.json)

brick_test(test_snake_rules unit/test_snake_rules.c)
brick_test(test_racing_rules unit/test_racing_rules.c)

add_executable(test_racing_conformance conformance/test_racing_conformance.c)
target_link_libraries(test_racing_conformance PRIVATE bktest)
target_compile_options(test_racing_conformance PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
add_test(NAME test_racing_conformance
         COMMAND test_racing_conformance ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/racing/racing.conformance.json)

brick_test(test_render unit/test_render.c)
brick_test(test_clips unit/test_clips.c)
brick_test(test_save unit/test_save.c)
brick_test(test_audio unit/test_audio.c)
brick_test(test_game_base unit/test_game_base.c)
brick_test(test_console unit/test_console.c)
brick_test(test_minlibc unit/test_minlibc.c) # the C library of the WebAssembly build, against the host's

add_executable(test_sim unit/test_sim.c)
target_link_libraries(test_sim PRIVATE bktest bricksim)
target_compile_options(test_sim PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
add_test(NAME test_sim COMMAND test_sim ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sim)

add_executable(test_screens unit/test_screens.c)
target_link_libraries(test_screens PRIVATE bktest)
target_compile_options(test_screens PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
add_test(NAME test_screens COMMAND test_screens ${CMAKE_CURRENT_SOURCE_DIR}/golden)

add_executable(test_cpp_headers api/test_cpp_headers.cpp)
target_link_libraries(test_cpp_headers PRIVATE brickcore)
target_compile_options(test_cpp_headers PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
add_test(NAME test_cpp_headers COMMAND test_cpp_headers)

# Reproducible arithmetic behind docs/DISPLAY_CANDIDATES-related notes (not a test of correctness).
add_executable(panel_fit ${CMAKE_SOURCE_DIR}/tools/experiments/panel_fit.c)
target_link_libraries(panel_fit PRIVATE brickcore)
add_test(NAME panel_fit_runs COMMAND panel_fit)

# The menu of every game as pictures (docs/UI_MENU.md section 4; tools/frames/menu_gif.py makes GIFs of them).
add_executable(menu_frames ${CMAKE_SOURCE_DIR}/tools/experiments/menu_frames.c)
target_link_libraries(menu_frames PRIVATE brickcore brickcatalog)
add_test(NAME menu_frames_runs
         COMMAND sh -c "mkdir -p '${CMAKE_BINARY_DIR}/menu_frames_out' && '$<TARGET_FILE:menu_frames>' '${CMAKE_BINARY_DIR}/menu_frames_out'")

# The simulator core as WebAssembly must give the same results as the native build (tests/fixtures/sim). Needs clang with
# the wasm32 target, wasm-ld and node; skipped (with a note) when one is missing.
find_program(BRICK_CLANG NAMES clang)
find_program(BRICK_WASM_LD NAMES wasm-ld)
find_program(BRICK_NODE NAMES node nodejs)
set(BRICK_HAVE_WASM_TARGET OFF)
if(BRICK_CLANG)
  execute_process(COMMAND ${BRICK_CLANG} --print-targets OUTPUT_VARIABLE BRICK_CLANG_TARGETS ERROR_QUIET)
  if(BRICK_CLANG_TARGETS MATCHES "wasm32")
    set(BRICK_HAVE_WASM_TARGET ON)
  endif()
endif()
if(BRICK_HAVE_WASM_TARGET AND BRICK_WASM_LD AND BRICK_NODE)
  add_test(NAME test_wasm
           COMMAND sh -c "CC='${BRICK_CLANG}' '${CMAKE_SOURCE_DIR}/platform/sim/web/build_wasm.sh' '${CMAKE_BINARY_DIR}/wasm' && '${BRICK_NODE}' '${CMAKE_SOURCE_DIR}/tests/web/test_wasm.mjs' '${CMAKE_BINARY_DIR}/wasm/brick-sim.wasm' '${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sim'")
else()
  message(STATUS "test_wasm skipped: needs clang with the wasm32 target, wasm-ld and node")
endif()
