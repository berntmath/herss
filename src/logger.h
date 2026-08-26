/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     logger.h                                                        
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
********************************************************************************/

#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

enum class LogLevel {
    Message,
    Warning,
    Error
};

class Logger {
public:
    static Logger& instance() {
        static Logger inst;   // Singleton: opprettes én gang

        return inst;
    }
    
    bool init(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mtx_);
        file_.open(filename, std::ios::out);
        this->logfilename = filename;
        return file_.is_open();
    }

    void info(const std::string& text) {
        std::cout << text << "\n"; // Only for screen output, not logged to file
    }

    void message(const std::string& text, const char* file, int line, const char* func) {
        log(LogLevel::Message, text, file, line, func);
    }

    void warning(const std::string& text, const char* file, int line, const char* func) {
        log(LogLevel::Warning, text, file, line, func);
    }

    void error(const std::string& text, const char* file, int line, const char* func) {
        log(LogLevel::Error, text, file, line, func);
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (file_.is_open()) file_.close();
    }

    // Terje Sandø, 06.07.2026
    // Lets main.cpp print a terminal hint at the end of the run if any warnings
    // were logged, since warnings only go to the log file.
    size_t getWarningCount() const { return warning_count_; }

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    std::string logfilename;
    // Terje Sandø, 06.07.2026, addet warning_count_ to track number of warnings logged during a run.
    size_t warning_count_ = 0;

    void log(LogLevel level, const std::string& text, const char* file, int line, const char* func);

    std::ofstream file_;
    std::mutex mtx_;
};



#ifdef HERSS_NO_LOG
  #define LOG_INFO(x)  ((void)0)
  #define LOG_MSG(x)   ((void)0)
  #define LOG_WARN(x)  ((void)0)
  #define LOG_ERR(x)   std::exit(EXIT_FAILURE)   // evt ((void)0) hvis du virkelig vil ignorere alt
#else
  #define LOG_INFO(x)  Logger::instance().info((x))  
  #define LOG_MSG(x)   Logger::instance().message((x), __FILE__, __LINE__, __func__)
  #define LOG_WARN(x)  Logger::instance().warning((x), __FILE__, __LINE__, __func__)
  #define LOG_ERR(x)   Logger::instance().error((x), __FILE__, __LINE__, __func__)
#endif

