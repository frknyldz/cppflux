#pragma once
// Private header — not installed, not visible to library consumers.
// Sets the I/O thread's event_base so async_sleep can find it without
// user code ever mentioning event_base.

#include <event2/event.h>

inline thread_local event_base* tl_base = nullptr;  // NOLINT
