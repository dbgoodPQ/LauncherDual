#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H
#include <Arduino.h>

#if defined(ARDUINO_M5STACK_CARDPUTER)
// SD file menu "Edit file": small text editor for config/text files (Cardputer keyboard only).
bool textEditorSupports(const String &path); // .json .conf .txt .ini .cfg .csv .md .log
void textEditor(const String &path);
#endif

#endif
