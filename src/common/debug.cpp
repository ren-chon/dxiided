#include "common/debug.hpp"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <ctime>
#include <iomanip>
#include <fstream>

namespace dxiided {
namespace debug {

static const std::array<const char*, 5> DEBUG_LEVEL_NAMES = {
    "none", "err", "warn", "fixme", "trace"};

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

void Logger::Initialize() {
    OpenLogFile();
    
    if (const char* env_var = std::getenv(DEBUG_ENV)) {
        std::string debug_str(env_var);
        for (size_t i = 0; i < DEBUG_LEVEL_NAMES.size(); i++) {
            if (debug_str.find(DEBUG_LEVEL_NAMES[i]) != std::string::npos) {
                m_debugLevel |= (1 << i);
            }
        }
    }
}

void Logger::OpenLogFile() {
    m_logFile.open(LOG_FILE, std::ios::out | std::ios::app);
    if (m_logFile.is_open()) {
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        m_logFile << "\n=== Log started at " 
                 << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
                 << " ===\n" << std::endl;
    }
}

void Logger::WriteToOutputs(const char* message) {
    if (m_logFile.is_open()) {
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        m_logFile << std::put_time(&tm, "%H:%M:%S") << " " << message << std::endl;
    }
    
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
    
    // Also write to stdout if available
    fprintf(stdout, "%s\n", message);
    fflush(stdout);
}

void Logger::Trace(const char* file, unsigned int line, const char* fmt, ...) {
    std::array<char, 512> buffer;

    int len = snprintf(buffer.data(), buffer.size(), "%s:%u: ", file, line);
    if (len < 0 || len >= static_cast<int>(buffer.size())) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer.data() + len, buffer.size() - len, fmt, args);
    va_end(args);

    WriteToOutputs(buffer.data());
}

std::string Logger::GuidToString(const GUID& guid) {
    std::array<char, 64> buffer;
    snprintf(buffer.data(), buffer.size(),
             "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", guid.Data1,
             guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
             guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5],
             guid.Data4[6], guid.Data4[7]);
    return std::string(buffer.data());
}

std::string Logger::WideToString(const WCHAR* wstr) {
    if (!wstr) return "(null)";

    int size_needed =
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size_needed, nullptr,
                        nullptr);
    return str;
}

}  // namespace debug
}  // namespace dxiided
