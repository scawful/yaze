#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Interop structs for passing data from C++ to Swift.
typedef struct {
  uint8_t crystal_bitfield;
  uint8_t game_state;
  uint8_t oosprog;
  uint8_t oosprog2;
  uint8_t side_quest;
  uint8_t pendants;
  int crystal_count;
} OracleProgressionData;

typedef struct {
  int room_id;
  const char *name;
  const char *type;
} RoomInfoData;

@interface YazeIOSBridge : NSObject

// ─── Core ────────────────────────────────────────
+ (void)loadRomAtPath:(NSString *)path NS_SWIFT_NAME(loadRom(atPath:));
+ (void)openProjectAtPath:(NSString *)path NS_SWIFT_NAME(openProject(atPath:));
+ (NSString *)currentRomTitle NS_SWIFT_NAME(currentRomTitle());
+ (void)setOverlayTopInset:(double)inset NS_SWIFT_NAME(setOverlayTopInset(_:));
+ (void)setTouchScale:(double)scale NS_SWIFT_NAME(setTouchScale(_:));
+ (void)showProjectFileEditor NS_SWIFT_NAME(showProjectFileEditor());
+ (void)showProjectManagement NS_SWIFT_NAME(showProjectManagement());
+ (void)showPanelBrowser NS_SWIFT_NAME(showPanelBrowser());
+ (void)showCommandPalette NS_SWIFT_NAME(showCommandPalette());

// ─── Editor Actions ─────────────────────────────
+ (void)saveRom NS_SWIFT_NAME(saveRom());
+ (void)undo NS_SWIFT_NAME(undo());
+ (void)redo NS_SWIFT_NAME(redo());
+ (void)switchToEditor:(NSString *)editorName NS_SWIFT_NAME(switchToEditor(_:));
+ (NSArray<NSString *> *)availableEditorTypes NS_SWIFT_NAME(availableEditorTypes());

// ─── Editor Status ──────────────────────────────
+ (nullable NSString *)currentEditorType NS_SWIFT_NAME(currentEditorType());
+ (nullable NSString *)currentRoomStatus NS_SWIFT_NAME(currentRoomStatus());
+ (NSArray<NSDictionary *> *)getActiveDungeonRooms
    NS_SWIFT_NAME(getActiveDungeonRooms());
+ (void)focusDungeonRoom:(NSInteger)roomID NS_SWIFT_NAME(focusDungeonRoom(_:));

// ─── Oracle Integration ──────────────────────────
/// Returns the current Oracle progression state, or zeros if unavailable.
+ (OracleProgressionData)getProgressionState NS_SWIFT_NAME(getProgressionState());

/// Returns story events as a JSON string (array of event objects).
+ (nullable NSString *)getStoryEventsJSON NS_SWIFT_NAME(getStoryEventsJSON());

/// Returns the list of dungeon rooms for a given dungeon ID (e.g., "D4").
+ (NSArray<NSDictionary *> *)getDungeonRooms:(NSString *)dungeonId
    NS_SWIFT_NAME(getDungeonRooms(_:));

@end

NS_ASSUME_NONNULL_END
