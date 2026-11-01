// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell

#include "./string.hpp"

#ifndef DOCTEST_CONFIG_DISABLE
#include <cstring>
#include <string_view>
#include <utility>

#include <doctest/doctest.h>

namespace vulkandemo::text
{
TEST_CASE("[pure] shared strings retain owned terminated bytes through copies and moves")
{
    char source[] = "extension";
    String original{source};
    auto copy = original;
    auto moved = std::move(original);
    source[0] = 'X';

    CHECK_EQ(copy.view(), "extension");
    CHECK(moved.view() == copy.view());
    CHECK(copy.c_str() == moved.c_str());
    CHECK_EQ(std::strcmp(moved.c_str(), "extension"), 0);
    std::string_view converted = moved;
    CHECK_EQ(converted, "extension");
    // A moved-from string remains observable even if its backing handle is empty.
    CHECK(original.view().size() == std::strlen(original.c_str()));
}

TEST_CASE("[pure] empty shared strings provide a terminated empty view")
{
    String empty;
    String explicit_empty{std::string_view{}};

    CHECK(empty.empty());
    CHECK(explicit_empty.empty());
    CHECK(empty == explicit_empty);
    CHECK_EQ(empty.c_str()[0], '\0');
    CHECK_EQ(explicit_empty.c_str()[0], '\0');
}
}  // namespace vulkandemo::text
#endif  // DOCTEST_CONFIG_DISABLE
