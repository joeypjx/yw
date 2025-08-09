#include "log_manager.h"
#include "json.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/async.h"
#include "spdlog/details/os.h" // Required for create_dir
#include <fstream>
#include <iostream>

using json = nlohmann::json;

// Define the static logger instance
std::shared_ptr<spdlog::logger> g_logger;

void LogManager::init(const std::string& configPath) {
    std::string log_level = "info";
    std::string log_file = "logs/app.log";
    size_t max_file_size = 5 * 1024 * 1024; // 5 MB
    size_t max_files = 3;

    std::ifstream configFile(configPath);
    if (configFile.is_open()) {
        try {
            json config;
            configFile >> config;
            log_level = config.value("log_level", "info");
            log_file = config.value("log_file", "logs/app.log");
            max_file_size = config.value("max_file_size_mb", 5) * 1024 * 1024;
            max_files = config.value("max_files", 3);
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse log config file: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Log config file not found. Using default settings." << std::endl;
    }

    try {
        // Ensure the log directory exists
        auto sink_dir = spdlog::details::os::dir_name(log_file);
        if (!sink_dir.empty()) {
            spdlog::details::os::create_dir(sink_dir);
        }

        // Create sinks: one for console, one for a rotating file
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::from_str(log_level));

        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file, max_file_size, max_files);
        file_sink->set_level(spdlog::level::from_str(log_level));

        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

        // Create an asynchronous logger
        spdlog::init_thread_pool(8192, 1); // 8k queue size, 1 background thread
        g_logger = std::make_shared<spdlog::async_logger>("multi_sink_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        
        g_logger->set_level(spdlog::level::trace); // Set logger to lowest level, sinks control the output
        g_logger->flush_on(spdlog::level::warn);   // Auto-flush on warnings and above

        spdlog::register_logger(g_logger);
        spdlog::set_default_logger(g_logger);
        
        // Set a global format
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

std::shared_ptr<spdlog::logger>& LogManager::getLogger() {
    // If not initialized, create a basic console logger as a fallback
    if (!g_logger) {
        g_logger = spdlog::stdout_color_mt("console_fallback");
        g_logger->warn("Logger was not initialized. Using fallback console logger.");
    }
    return g_logger;
}
