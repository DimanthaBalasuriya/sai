#pragma once
#include <tracy/Tracy.hpp>
// Instrumentation owns no engine state; Tracy records scopes on the calling thread.
#define ENGINE_PROFILE_SCOPE(name) ZoneScopedN(name)
#define ENGINE_PROFILE_FRAME() FrameMark
