// SPDX-License-Identifier: MIT
// Copyright 2024 David Feltell
#include <exception>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <spdlog/spdlog.h>

#include "Logger.hpp"
#include "vulkandemo.hpp"

int main(int const argc, char ** argv)
{
	doctest::Context context;
	context.applyCommandLine(argc, argv);

	int res = context.run();  // run

	if (context.shouldExit())  // i.e. --exit
		return res;

	vulkandemo::LoggerPtr const logger = vulkandemo::create_logger();
	try
	{
		vulkandemo::vulkandemo(logger);
	}
	catch (std::exception & exc)
	{
		logger->error(exc.what());
		res += 1;
	}

	return res;
}
