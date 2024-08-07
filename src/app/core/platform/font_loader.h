// FontLoader.h
#ifndef FONTLOADER_H
#define FONTLOADER_H

#ifdef _WIN32
#include <Windows.h>
// Windows specific function declaration for loading system fonts into ImGui
int CALLBACK EnumFontFamExProc(const LOGFONT* lpelfe, const TEXTMETRIC* lpntme,
                               DWORD FontType, LPARAM lParam);
#elif __APPLE__

#include "TargetConditionals.h"

#if TARGET_OS_MAC == 1

void LoadSystemFonts();

#elif TARGET_OS_IPHONE == 1

void LoadSystemFonts();

#endif

#elif __linux__

// TODO: Implement Linux specific function declaration for loading system fonts

#endif

#endif  // FONTLOADER_H
