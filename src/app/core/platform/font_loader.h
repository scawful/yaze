// FontLoader.h
#ifndef FONTLOADER_H
#define FONTLOADER_H

#include "TargetConditionals.h"

#ifdef _WIN32
#include <Windows.h>
// Windows specific function declaration for loading system fonts into ImGui
int CALLBACK EnumFontFamExProc(const LOGFONT* lpelfe, const TEXTMETRIC* lpntme,
                               DWORD FontType, LPARAM lParam);
#elif __APPLE__

#ifdef TARGET_OS_MAC

void LoadSystemFonts();

#elif TARGET_OS_IPHONE

void LoadSystemFonts();

#endif

#endif

#endif  // FONTLOADER_H
