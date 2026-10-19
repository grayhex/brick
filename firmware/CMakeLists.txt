# Platform-neutral core: no dynamic allocation, no globals, no I/O. Builds for the host and (as a smoke test) for
# Cortex-M4 with cmake/arm-none-eabi.cmake, see docs/FIRMWARE_ARCHITECTURE.md.

set(BRICK_CORE_SOURCES
  core/rng.c
  core/input.c
  core/event.c
  core/game.c
  core/frame.c
  core/font.c
  core/render.c
  core/audio.c
  core/save.c
  core/console.c
  games/tetris/tetris_shapes.c
  games/tetris/tetris.c
)

add_library(brickcore STATIC ${BRICK_CORE_SOURCES})
target_include_directories(brickcore
  PUBLIC include
  PRIVATE games/tetris)

target_compile_options(brickcore PRIVATE
  -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wcast-qual -Wformat=2
  # Bit-exact conformance with the reference needs IEEE double results without fused multiply-add.
  -ffp-contract=off)

# libm: fmod() in the gravity timer (ADR-024).
target_link_libraries(brickcore PUBLIC m)

# Size report of the core (meaningful for the ARM cross build; works on the host too).
find_program(BRICK_SIZE_TOOL NAMES arm-none-eabi-size size)
find_program(BRICK_NM_TOOL NAMES arm-none-eabi-nm nm)
if(BRICK_SIZE_TOOL)
  add_custom_target(brickcore_size
    COMMAND ${BRICK_SIZE_TOOL} -t $<TARGET_FILE:brickcore>
    COMMENT "Size of brickcore (text = flash, data/bss = RAM; excludes libc/libm)"
    VERBATIM)
endif()
if(BRICK_NM_TOOL)
  add_custom_target(brickcore_undefined
    COMMAND ${BRICK_NM_TOOL} -u $<TARGET_FILE:brickcore>
    COMMENT "External symbols the core needs from the C library"
    VERBATIM)
endif()
