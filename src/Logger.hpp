// vulkanisedfelt
// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell

/**
 * @file Logger.hpp
 *
 * Logging setup for the Vulkan demo.
 */

#pragma once
#include <memory>
#include <string>

namespace spdlog
{
class logger;  // NOLINT(readability-identifier-naming)
}  // namespace spdlog

namespace vulkandemo
{
/** Shared ownership of a configured logger. */
using LoggerPtr = std::shared_ptr<spdlog::logger>;
/**
 * Create a console logger using the configured build-time logging level.
 *
 * @param name Name used to register the logger.
 * @return Shared owner of the logger.
 */
LoggerPtr create_logger(std::string const & name = "console");
}  // namespace vulkandemo
