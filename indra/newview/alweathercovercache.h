/**
 * @file alweathercovercache.h
 * @brief World-cell cache for precipitation-only cover traversals.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_WEATHER_COVER_CACHE_H
#define AL_WEATHER_COVER_CACHE_H
#include "alweathershelter.h"
// Basic cover and advanced shelter share WP7's regional update scheduler.
using ALWeatherCoverCache = ALWeatherShelter::Updates;
#endif
