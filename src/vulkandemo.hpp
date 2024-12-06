#pragma once
#include <memory>

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace vulkandemo
{

using LoggerPtr = std::shared_ptr<spdlog::logger>;
LoggerPtr create_logger(const std::string & name = "console");

void vulkandemo(const LoggerPtr& logger);
}  // namespace vulkandemo