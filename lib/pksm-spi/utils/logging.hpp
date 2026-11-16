// logging.hpp - PocketPKMN's stand-in for PKSM's utils/logging.hpp, so lib/pksm-spi/spi.cpp builds unchanged: its
// Logging::info / Logging::warning calls (std::format style) go into PocketPKMN's debug log (dbg.c, link_log.txt).
#ifndef POCKETPKMN_PKSM_LOGGING_SHIM
#define POCKETPKMN_PKSM_LOGGING_SHIM

#include <format>
#include <string>
#include <utility>

extern "C" void dbg_printf(const char* fmt, ...);

namespace Logging
{
    template <typename... Args>
    void info(std::format_string<Args...> f, Args&&... args)
    {
        dbg_printf("spi: %s", std::format(f, std::forward<Args>(args)...).c_str());
    }

    template <typename... Args>
    void warning(std::format_string<Args...> f, Args&&... args)
    {
        dbg_printf("spi WARNING: %s", std::format(f, std::forward<Args>(args)...).c_str());
    }
}

#endif
