cmake_minimum_required(VERSION 3.16)
project(brick VERSION 0.1.0 LANGUAGES C CXX)

# Host build of the platform-neutral Brick firmware core and its tests.
# There is deliberately no board, display or PCB dependency here (AGENTS.md: hardware gates are not passed yet).

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BRICK_BUILD_TESTS "Build host tests" ON)
option(BRICK_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer (host only)" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(BRICK_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(firmware)
if(NOT CMAKE_CROSSCOMPILING)
  add_subdirectory(platform) # PC simulator, host only
endif()

if(BRICK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
