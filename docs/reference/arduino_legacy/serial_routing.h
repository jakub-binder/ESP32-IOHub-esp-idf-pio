#pragma once

#include <Arduino.h>

inline Stream &serialCommands()
{
#if defined(APP_MODE_DEBUG)
  return Serial;
#elif defined(APP_MODE_PROD)
  return Serial1;
#else
#error "Application mode is not selected. Define APP_MODE_DEBUG or APP_MODE_PROD."
#endif
}

inline Stream &serialCommandsFlash()
{
#if defined(APP_MODE_DEBUG)
  return Serial1;
#elif defined(APP_MODE_PROD)
  return Serial;
#else
#error "Application mode is not selected. Define APP_MODE_DEBUG or APP_MODE_PROD."
#endif
}
