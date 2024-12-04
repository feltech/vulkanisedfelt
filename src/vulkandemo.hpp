#pragma once
#include <memory>

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace vulkandemo
{

using Logger = std::shared_ptr<spdlog::logger>;
Logger create_logger(const std::string & name = "console");

void vulkandemo(const Logger& logger);
}  // namespace vulkandemo