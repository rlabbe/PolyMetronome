#pragma once

#if defined(POLYMETRONOMELIB_BUILDING)
#define POLYMETRONOME_API __declspec(dllexport)
#elif defined(POLYMETRONOMELIB_STATIC)
#define POLYMETRONOME_API
#else
#define POLYMETRONOME_API __declspec(dllimport)
#endif
