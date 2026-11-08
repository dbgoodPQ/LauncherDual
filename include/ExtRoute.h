// Which panel(s) receive pixels on m5stack-cardputer-dual (see ExtMirrorBus.h).
#ifndef EXT_ROUTE_H
#define EXT_ROUTE_H
#include <stdint.h>
#define EXT_INT 1 // built-in screen
#define EXT_EXT 2 // external screen
extern uint8_t extRoute; // src/display.cpp
#endif
