#pragma once
#include "Logger.hpp"
#include <iostream>
#include <format>

using Logger = util::TsSingletonLogger<util::StreamLogger>;
using LogLevel = util::Logger::LogLevel;

void initLogger(LogLevel lvl);

#define Log (Logger::get())