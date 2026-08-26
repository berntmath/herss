
#include "logger.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

static std::string nowAsText() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
    localtime_r(&t, &tm_now);

    std::ostringstream os;
    os << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    return os.str();
}

void Logger::log(LogLevel level, const std::string& text, const char* file, int line, const char* func) {
    std::lock_guard<std::mutex> lock(mtx_);

    const char* lvl = "MSG";
    if (level == LogLevel::Warning) { lvl = "WRN"; warning_count_++; }
    if (level == LogLevel::Error)   lvl = "ERR";

    std::ostringstream entry;
    entry << nowAsText() << " [" << lvl << "] "
          << text << " (" << file << ":" << line << ", " << func << ")";

    // If we want to print to screen, we can do it here.
    // I turned it off, too much screen flushing  
    // std::cout << entry.str() << "\n";           // til skjerm (stdout)

    if (file_.is_open()) file_ << entry.str() << "\n";  // til loggfil

    if (level == LogLevel::Error) {
        std::cout << "ERROR:   " << entry.str() << "\n";           // til skjerm (stdout)
        std::cout << "An error occurred. Check the log file for details.\n";
        std::cout << "Log file: " << this->logfilename << "\n";
        std::exit(EXIT_FAILURE);
    }
}