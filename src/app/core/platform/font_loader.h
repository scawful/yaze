// FontLoader.h
#ifndef FONTLOADER_H
#define FONTLOADER_H

// Function declaration for loading system fonts into ImGui
void LoadSystemFonts();

#ifdef _WIN32
#include <Windows.h>
// Windows specific function declaration for loading system fonts into ImGui
int CALLBACK EnumFontFamExProc(const LOGFONT* lpelfe, const TEXTMETRIC* lpntme,
                               DWORD FontType, LPARAM lParam);
#endif

#endif  // FONTLOADER_H
