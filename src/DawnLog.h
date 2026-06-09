#pragma once

// Central logging include for Dawn.
//
// Firefly's <firefly/log.hpp> unconditionally `#define`s FIREFLY_DEFAULT_LOGGER to
// "Client" (it is not #ifndef-guarded), so defining the macro *before* including the
// header — as some docs suggest — has no effect; the header simply overrides it.
//
// The LOG_* macros expand FIREFLY_DEFAULT_LOGGER at each call site, so redefining it
// here, *after* the include, makes every Dawn translation unit log to the "Dawn"
// logger. Every Dawn .cpp that logs includes this header instead of firefly/log.hpp.
#include <firefly/log.hpp>
#include <firefly/log-registry.hpp>

#ifdef FIREFLY_DEFAULT_LOGGER
#undef FIREFLY_DEFAULT_LOGGER
#endif
#define FIREFLY_DEFAULT_LOGGER "Dawn"
