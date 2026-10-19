# Cross-compile the portable core for a Cortex-M4F class MCU (e.g. Nordic nRF52840) -- a PORTABILITY SMOKE TEST only.
# It does not select a board, a SoC or a toolchain version for the product (those decisions are OPEN, see docs/DECISIONS.md).
#
#   cmake -S . -B build-m4 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DBRICK_BUILD_TESTS=OFF
#   cmake --build build-m4 && cmake --build build-m4 --target brickcore_size
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_SIZE_UTIL arm-none-eabi-size)
set(CMAKE_NM_UTIL arm-none-eabi-nm)

set(BRICK_MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_FLAGS_INIT "${BRICK_MCU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${BRICK_MCU_FLAGS} -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti")
set(CMAKE_C_FLAGS_RELEASE_INIT "-Os")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
