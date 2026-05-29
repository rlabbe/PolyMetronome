#pragma once

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

// Logging output is compiled in only when METRONOME_DEBUG_LOG is defined (add
// it to the project's preprocessor definitions). When it is not defined,
// kLogEnabled is false and the body of LOGT becomes a discarded
// `if constexpr` branch: it is still type-checked (so the message expression
// and every variable it names stay "used" — no unused-variable warnings under
// /W4 warnings-as-errors) but generates no code and prints nothing.
//
// log_now_ms() / log_tid() are defined unconditionally because the call sites
// also use them outside LOGT (to time sections), so they must always exist.

#ifdef METRONOME_DEBUG_LOG
inline constexpr bool kLogEnabled = true;
#else
inline constexpr bool kLogEnabled = false;
#endif

inline double log_now_ms()
{
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

inline unsigned log_tid()
{
    return static_cast<unsigned>(std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xffffu);
}

#define LOGT(msg)                                                                          \
    do {                                                                                   \
        if constexpr (kLogEnabled) {                                                       \
            std::ostringstream oss_;                                                        \
            oss_ << "[" << log_now_ms() << "ms t:" << log_tid() << "] " << msg << '\n';     \
            std::cerr << oss_.str() << std::flush;                                          \
        }                                                                                  \
    } while (0)
