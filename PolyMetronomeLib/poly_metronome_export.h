#pragma once

#if defined(POLYMETRONOMELIB_BUILDING)
#define POLYMETRONOME_API __declspec(dllexport)
#else
#define POLYMETRONOME_API __declspec(dllimport)
#endif
