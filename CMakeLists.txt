# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) StackBlender LLC
cmake_minimum_required(VERSION 3.16)
project(snes_dsp4 CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(dsp4 STATIC src/dsp4/Dsp4.cpp)
target_include_directories(dsp4 PUBLIC src)

add_executable(dsp4_check tools/dsp4_check.cpp)
target_link_libraries(dsp4_check PRIVATE dsp4)

enable_testing()
add_executable(dsp4_state_test tests/state_test.cpp)
target_link_libraries(dsp4_state_test PRIVATE dsp4)
add_test(NAME dsp4_state COMMAND dsp4_state_test)
