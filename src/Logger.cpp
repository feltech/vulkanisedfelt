// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#include "Logger.hpp"

#include <string>

#include <spdlog/common.h>
#include <spdlog/logger.h>	// NOLINT(misc-include-cleaner)
#include <spdlog/sinks/stdout_color_sinks.h>

namespace vulkandemo
{
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

LoggerPtr create_logger(const std::string & name)
{
	LoggerPtr logger = spdlog::stdout_color_mt(name);
	logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%L] %v");
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
	logger->set_level(spdlog::level::trace);
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
	logger->set_level(spdlog::level::debug);
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_INFO
	logger->set_level(spdlog::level::info);
#endif
	return logger;
}
}  // namespace vulkandemo