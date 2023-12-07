#include "ProjLogger.hpp"

void initLogger(LogLevel lvl) {
	std::unique_ptr<util::StreamLogger> pslog = std::make_unique<util::StreamLogger>(lvl, util::Logger::Options(true, true, true, true, true), std::cout);
	//std::unique_ptr<util::FileLogger> pslog = std::make_unique<util::FileLogger>(lvl, util::Logger::Options(true, true, true, true, true), "/mnt/d/1.txt", std::ios_base::out);
	Logger::init(std::move(pslog));
}