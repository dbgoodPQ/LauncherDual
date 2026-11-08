// Screen selection for m5stack-cardputer-dual (EXT_MIRROR builds only).
//
// Two modes, chosen at boot and stored in NVS ("launcher" / "extScreen"):
//   internal (default) - the built-in screen, mirrored scaled onto the external
//                        panel by ExtMirrorBus.
//   external           - the external 240x320 panel is the only display. The
//                        launcher lays itself out natively at 320x240, nothing
//                        is scaled, and the built-in screen stays dark.
//
// Switch with a 3 s hold of the G0 button (switches on release) or from
// Config > "Screen". Both save the other mode and reboot.
//
// Implementation: boards/m5stack-cardputer/interface.cpp.
#ifndef EXT_DISPLAY_H
#define EXT_DISPLAY_H
#if defined(EXT_MIRROR)

#include <stdint.h>

extern bool extPrimary;                  // true: this boot drives the external panel only
extern volatile uint32_t extLastProgress; // last progressHandler() call, to hold off a reboot mid-write

void extDisplaySetup();  // before tft->begin(): read the mode and retarget displayConfig
void extDisplayToggle(); // save the other mode and reboot

#endif
#endif // EXT_DISPLAY_H
