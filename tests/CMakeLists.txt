add_library(bktest STATIC support/bk_test.c support/tetris_lab.c support/mini_json.c support/bk_digest.c support/frame_dump.c support/fake_hal.c support/stub_game.c)
target_include_directories(bktest PUBLIC support ${CMAKE_SOURCE_DIR}/firmware/games/tetris)
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

brick_test(test_render unit/test_render.c)
brick_test(test_save unit/test_save.c)
brick_test(test_audio unit/test_audio.c)
brick_test(test_game_base unit/test_game_base.c)
brick_test(test_console unit/test_console.c)

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
