#pragma once

#include <string>
#include <sstream>
#include <windows.h>
#include <memory>
#include <fstream>

namespace dxiided {
namespace debug {

enum class DebugChannel : uint32_t {
    API     = 0x01,
    SHADER  = 0x02
};

enum class DebugLevel {
    NONE,
    ERR,
    WARN,
    FIXME,
    TRACE
};

class Logger {
public:
    static Logger& Instance();
    
    void Initialize();
    void Trace(const char* file, unsigned int line, const char* fmt, ...);
    
    bool HasChannel(DebugChannel channel) const { return (m_debugLevel & static_cast<uint32_t>(channel)) != 0; }
    
    // Helper functions for COM objects and Windows types
    static std::string GuidToString(const GUID& guid);
    static std::string WideToString(const WCHAR* wstr);
    
private:
    Logger() = default;
    ~Logger();
    
    void OpenLogFile();
    void WriteToOutputs(const char* message);
    
    uint32_t m_debugLevel = 0;
    std::ofstream m_logFile;
    static constexpr const char* LOG_FILE = "dxiided.log";
    static constexpr const char* DEBUG_ENV = "DXIIDED_DEBUG";
};

} // namespace debug
} // namespace dxiided

// Debug macros
#ifdef NDEBUG
#define TRACE(fmt, ...)
#define ERR(fmt, ...)
#define WARN(fmt, ...)
#define FIXME(fmt, ...)
#else
#define TRACE(fmt, ...) \
    dxiided::debug::Logger::Instance().Trace(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ERR(fmt, ...) \
    dxiided::debug::Logger::Instance().Trace(__FILE__, __LINE__, "err: " fmt, ##__VA_ARGS__)
#define WARN(fmt, ...) \
    dxiided::debug::Logger::Instance().Trace(__FILE__, __LINE__, "warn: " fmt, ##__VA_ARGS__)
#define FIXME(fmt, ...) \
    dxiided::debug::Logger::Instance().Trace(__FILE__, __LINE__, "fixme: " fmt, ##__VA_ARGS__)
#endif

// Helper functions
inline std::string debugstr_guid(const GUID* id) {
    return id ? dxiided::debug::Logger::GuidToString(*id) : "(null)";
}

inline std::string debugstr_w(const WCHAR* wstr) {
    return dxiided::debug::Logger::WideToString(wstr);
}
