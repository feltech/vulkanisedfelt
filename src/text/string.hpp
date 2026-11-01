// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2026 David Feltell
/**
 * @file text/string.hpp
 *
 * Immutable, shared strings with stable C-string access.
 */
#pragma once

#include <algorithm>
#include <string_view>
#include <utility>

#include <immer/array.hpp>
#include <immer/array_transient.hpp>

namespace vulkandemo::text
{
/** Owns string bytes and their terminating NUL with cheap immutable sharing. */
class String
{
public:
    String() = default;

    /**
     * Copies a string view into independently owned, terminated storage.
     *
     * @param value String bytes, excluding the terminating NUL.
     */
    explicit String(std::string_view value) : bytes_{terminated_bytes(value)} {}

    /** @return Stable terminated bytes while this owner remains alive. */
    [[nodiscard]] auto c_str() const noexcept -> char const *
    {
        return bytes_.empty() ? "" : bytes_.data();
    }

    /** @return String bytes without the terminating NUL. */
    [[nodiscard]] auto view() const noexcept -> std::string_view
    {
        return {c_str(), bytes_.empty() ? 0 : bytes_.size() - 1};
    }

    /** @return Borrowed string bytes without the terminating NUL. */
    operator std::string_view() const noexcept
    {
        return view();
    }

    /** @return Whether the string has no content bytes. */
    [[nodiscard]] auto empty() const noexcept -> bool
    {
        return view().empty();
    }

    /**
     * Compares logical string contents.
     *
     * @param lhs First string.
     * @param rhs Second string.
     * @return Whether both strings have identical bytes.
     */
    friend auto operator==(String const & lhs, String const & rhs) noexcept -> bool
    {
        return lhs.view() == rhs.view();
    }

private:
    static auto terminated_bytes(std::string_view value)
        -> immer::array<char> pre(value.find('\0') == std::string_view::npos)
    {
        auto bytes = immer::array<char>(value.size() + 1).transient();
        auto * output = bytes.data_mut();
        std::ranges::copy(value, output);
        output[value.size()] = '\0';
        return std::move(bytes).persistent();
    }

    immer::array<char> bytes_;
};
}  // namespace vulkandemo::text
