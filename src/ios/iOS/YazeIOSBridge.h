#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface YazeIOSBridge : NSObject
+ (void)loadRomAtPath:(NSString *)path NS_SWIFT_NAME(loadRom(atPath:));
+ (void)openProjectAtPath:(NSString *)path NS_SWIFT_NAME(openProject(atPath:));
+ (NSString *)currentRomTitle NS_SWIFT_NAME(currentRomTitle());
+ (void)setOverlayTopInset:(double)inset NS_SWIFT_NAME(setOverlayTopInset(_:));
+ (void)showProjectFileEditor NS_SWIFT_NAME(showProjectFileEditor());
+ (void)showProjectManagement NS_SWIFT_NAME(showProjectManagement());
+ (void)showPanelBrowser NS_SWIFT_NAME(showPanelBrowser());
+ (void)showCommandPalette NS_SWIFT_NAME(showCommandPalette());
@end

NS_ASSUME_NONNULL_END
