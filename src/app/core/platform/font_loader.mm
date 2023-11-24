// FontLoader.mm
#include "app/core/platform/font_loader.h"
#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#include <imgui/imgui.h>

void LoadSystemFonts() {
  // List of common macOS system fonts
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
      ImFont *imFont = io.Fonts->AddFontFromFileTTF([fontPath UTF8String], 14.0f);
      if (!imFont) {
        NSLog(@"Failed to load font: %@", fontPath);
      }
    } else {
      NSLog(@"Font file not accessible: %@", fontPath);
    }

    if (fontURL) {
      CFRelease(fontURL);
    }
  }
}