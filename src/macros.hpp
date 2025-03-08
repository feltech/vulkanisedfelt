// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#pragma once

#define VK_CHECK(func, msg)                                                                \
	do { /* NOLINT(cppcoreguidelines-avoid-do-while)  */                                   \
		if (const VkResult result = func; result != VK_SUCCESS)                            \
		{                                                                                  \
			throw std::runtime_error{std::format("{}: {}", msg, string_VkResult(result))}; \
		}                                                                                  \
	} while (false)

#define FW(a) std::forward<decltype(a)>(a)

#define AUTO(T) std::convertible_to<T> auto &&