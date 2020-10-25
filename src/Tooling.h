#pragma once

#if defined TRACE

#include "./../tracy/Tracy.hpp"

#define PROFILE_START_NAMED(title) ZoneScopedN(title);
#define PROFILE_START ZoneScopedN(__FUNCTION__);
#else
#define PROFILE_START_NAMED(x)
#define PROFILE_START
#endif
