# Brick V1 — nRF52840 platform application (nRF Connect SDK v3.4.1 / Zephyr).
#
# Build only with scripts/build.sh (sysbuild: MCUboot + this application, dev key).
# This application does NOT link firmware/ (libbrickcore) yet: bk_hal_t is
# connected in a separate PR (issue #10). Nothing from Nordic/Zephyr may be
# included from firmware/.
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(brick_nrf52840)

target_sources(app PRIVATE src/main.c)
