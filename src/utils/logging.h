#pragma once

struct NullStream {
  template <typename T>
  NullStream& operator<<(const T&) {
    return *this;
  }
};

#ifdef ENABLE_LOGGING
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Log.h>
#else
// Disable plog logging macros
#define PLOG_VERBOSE \
  if (false) NullStream()
#define PLOG_DEBUG \
  if (false) NullStream()
#define PLOG_INFO \
  if (false) NullStream()
#define PLOG_WARNING \
  if (false) NullStream()
#define PLOG_ERROR \
  if (false) NullStream()
#define PLOG_FATAL \
  if (false) NullStream()
#endif
