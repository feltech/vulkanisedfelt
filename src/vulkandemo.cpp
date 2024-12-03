#include "vulkandemo.hpp"

#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace vulkandemo
{

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

Logger default_logger()
{
	Logger logger = spdlog::stdout_color_mt("console");
	logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
	logger->set_level(spdlog::level::trace);
#elif SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_INFO
	logger->set_level(spdlog::level::info);
#endif
	return logger;
}

void vulkandemo(const Logger & logger)
{
	logger->info("vulkandemo: Hello World!");
}
}  // namespace vulkandemo