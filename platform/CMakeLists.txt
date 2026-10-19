# PC simulator: a window-less driver (bricksim) plus thin front-ends. Not part of the firmware image.
add_library(bricksim STATIC sim/sim.c)
target_include_directories(bricksim PUBLIC sim)
target_link_libraries(bricksim PUBLIC brickcore brickcatalog)
target_compile_options(bricksim PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes)

if(UNIX)
  add_executable(brick_sim_term sim/term/main.c)
  target_link_libraries(brick_sim_term PRIVATE bricksim)
  target_compile_options(brick_sim_term PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
  # clock_nanosleep, termios: POSIX.1-2008
  target_compile_definitions(brick_sim_term PRIVATE _POSIX_C_SOURCE=200809L)
endif()
