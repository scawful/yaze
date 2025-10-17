#include "app/platform/font_loader.h"

#import <CoreText/CoreText.h>
#include <TargetConditionals.h>

#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

#if TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1
/* iOS */
void yaze::LoadSystemFonts() {}

#elif TARGET_OS_MAC == 1
/* macOS */
#import <Cocoa/Cocoa.h>

void yaze::LoadSystemFonts() {
  NSArray *fontNames = @[ @"Helvetica", @"Times New Roman", @"Courier", @"Arial", @"Verdana" ];

  for (NSString *fontName in fontNames) {
    NSFont *font = [NSFont fontWithName:fontName size:14.0];
    if (!font) {
      NSLog(@"Font not found: %@", fontName);
      continue;
    }

    CTFontDescriptorRef fontDescriptor =
        CTFontDescriptorCreateWithNameAndSize((CFStringRef)font.fontName, font.pointSize);
    CFURLRef fontURL = (CFURLRef)CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontURLAttribute);
    NSString *fontPath = [(NSURL *)fontURL path];
    CFRelease(fontDescriptor);

    if (fontPath != nil && [[NSFileManager defaultManager] isReadableFileAtPath:fontPath]) {
      // Load the font into ImGui
      ImGuiIO &io = ImGui::GetIO();
      ImFontConfig icons_config;
      icons_config.MergeMode = true;
      icons_config.GlyphOffset.y = 5.0f;
      icons_config.GlyphMinAdvanceX = 13.0f;
      icons_config.PixelSnapH = true;
      static const ImWchar icons_ranges[] = {ICON_MIN_MD, 0xf900, 0};
      static const float ICON_FONT_SIZE = 18.0f;
      ImFont *imFont = io.Fonts->AddFontFromFileTTF([fontPath UTF8String], 14.0f);
      if (!imFont) {
        NSLog(@"Failed to load font: %@", fontPath);
      }
      io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_MD, ICON_FONT_SIZE, &icons_config,
                                   icons_ranges);
    } else {
      NSLog(@"Font file not accessible: %@", fontPath);
    }

    if (fontURL) {
      CFRelease(fontURL);
    }
  }
}

#endif
