# vulkanisedfelt
# SPDX-License-Identifier: MIT
# Copyright 2026 David Feltell

# Exercise the consumer target without Vulkan, a display or fnfelt's own dev suite.
add_executable(vulkanisedfelt.fnfelt fnfelt.cpp)
target_link_libraries(vulkanisedfelt.fnfelt PRIVATE fnfelt::fnfelt)
vulkanisedfelt_target_linters(vulkanisedfelt.fnfelt)
add_test(NAME vulkanisedfelt.fnfelt COMMAND vulkanisedfelt.fnfelt)
set_tests_properties(
  vulkanisedfelt.fnfelt
  PROPERTIES LABELS
             "cpu"
             TIMEOUT
             30)
