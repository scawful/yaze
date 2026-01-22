#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface YazeIOSBridge : NSObject
+ (void)loadRomAtPath:(NSString *)path NS_SWIFT_NAME(loadRom(atPath:));
+ (void)openProjectAtPath:(NSString *)path NS_SWIFT_NAME(openProject(atPath:));
+ (NSString *)currentRomTitle NS_SWIFT_NAME(currentRomTitle());
@end

NS_ASSUME_NONNULL_END
