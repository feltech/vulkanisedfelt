// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#pragma once
#include <memory>

namespace spdlog
{
class logger;
}

namespace vulkandemo
{
using LoggerPtr = std::shared_ptr<spdlog::logger>;
LoggerPtr create_logger(const std::string & name = "console");
}