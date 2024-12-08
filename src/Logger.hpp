// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#pragma once
#include <memory>
#include <string>

namespace spdlog
{
class logger;  // NOLINT(readability-identifier-naming)
}  // namespace spdlog

namespace vulkandemo
{
using LoggerPtr = std::shared_ptr<spdlog::logger>;
LoggerPtr create_logger(std::string const & name = "console");
}  // namespace vulkandemo