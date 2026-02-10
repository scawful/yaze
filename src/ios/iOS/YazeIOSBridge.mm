#import "YazeIOSBridge.h"

#include <string>

#include "app/application.h"
#include "app/controller.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor.h"
#include "app/editor/editor_manager.h"
#include "app/platform/ios/ios_platform_state.h"
#include "core/hack_manifest.h"
#include "core/oracle_progression.h"
#include "core/project.h"
#include "rom/rom.h"

@implementation YazeIOSBridge

+ (void)loadRomAtPath:(NSString *)path {
  if (!path || path.length == 0) {
    return;
  }
  std::string cpp_path([path UTF8String]);
  yaze::Application::Instance().LoadRom(cpp_path);
}

+ (void)openProjectAtPath:(NSString *)path {
  if (!path || path.length == 0) {
    return;
  }
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller || !controller->editor_manager()) {
    return;
  }

  std::string cpp_path([path UTF8String]);
  (void)controller->editor_manager()->OpenRomOrProject(cpp_path);
}

+ (NSString *)currentRomTitle {
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller) {
    return @"";
  }
  auto *rom = controller->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    return @"";
  }
  return [NSString stringWithUTF8String:rom->title().c_str()];
}

+ (void)setOverlayTopInset:(double)inset {
  yaze::platform::ios::SetOverlayTopInset(static_cast<float>(inset));
}

+ (void)setTouchScale:(double)scale {
  yaze::platform::ios::SetTouchScale(static_cast<float>(scale));
}

+ (void)showProjectFileEditor {
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller || !controller->editor_manager()) {
    return;
  }
  controller->editor_manager()->ShowProjectFileEditor();
}

+ (void)showProjectManagement {
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller || !controller->editor_manager()) {
    return;
  }
  controller->editor_manager()->ShowProjectManagement();
}

+ (void)showPanelBrowser {
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller || !controller->editor_manager()) {
    return;
  }
  controller->editor_manager()->panel_manager().TriggerShowPanelBrowser();
}

+ (void)showCommandPalette {
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller || !controller->editor_manager()) {
    return;
  }
  controller->editor_manager()->panel_manager().TriggerShowCommandPalette();
}

// ─── Editor Status ──────────────────────────────

+ (nullable NSString *)currentEditorType {
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller || !controller->editor_manager()) {
    return nil;
  }
  auto *editor = controller->editor_manager()->GetCurrentEditor();
  if (!editor) {
    return nil;
  }
  auto index = yaze::editor::EditorTypeIndex(editor->type());
  if (index < yaze::editor::kEditorNames.size()) {
    return [NSString stringWithUTF8String:yaze::editor::kEditorNames[index]];
  }
  return @"Unknown";
}

+ (nullable NSString *)currentRoomStatus {
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller || !controller->editor_manager()) {
    return nil;
  }
  auto *editor = controller->editor_manager()->GetCurrentEditor();
  if (!editor || editor->type() != yaze::editor::EditorType::kDungeon) {
    return nil;
  }
  auto *dungeon_editor =
      static_cast<yaze::editor::DungeonEditorV2 *>(editor);
  int room_id = dungeon_editor->current_room_id();
  return [NSString stringWithFormat:@"Room 0x%03X", room_id];
}

// ─── Oracle Integration ──────────────────────────

+ (OracleProgressionData)getProgressionState {
  OracleProgressionData data = {};
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller || !controller->editor_manager()) {
    return data;
  }

  const auto &project = *controller->editor_manager()->GetCurrentProject();
  if (!project.hack_manifest.loaded()) {
    return data;
  }

  // Note: In a real scenario, this would read from a loaded .srm file
  // or live emulator state. For now, return the default zero state.
  yaze::core::OracleProgressionState state;
  data.crystal_bitfield = state.crystal_bitfield;
  data.game_state = state.game_state;
  data.oosprog = state.oosprog;
  data.oosprog2 = state.oosprog2;
  data.side_quest = state.side_quest;
  data.pendants = state.pendants;
  data.crystal_count = state.GetCrystalCount();
  return data;
}

+ (nullable NSString *)getStoryEventsJSON {
  auto *controller = yaze::Application::Instance().GetController();
  if (!controller || !controller->editor_manager()) {
    return nil;
  }

  const auto &project = *controller->editor_manager()->GetCurrentProject();
  if (!project.hack_manifest.loaded() ||
      !project.hack_manifest.HasProjectRegistry()) {
    return nil;
  }

  const auto &graph = project.hack_manifest.project_registry().story_events;
  if (!graph.loaded()) {
    return nil;
  }

  // Build a JSON array of events
  NSMutableArray *events = [NSMutableArray array];
  for (const auto &node : graph.nodes()) {
    NSMutableDictionary *event = [NSMutableDictionary dictionary];
    event[@"id"] = [NSString stringWithUTF8String:node.id.c_str()];
    event[@"name"] = [NSString stringWithUTF8String:node.name.c_str()];
    event[@"notes"] = [NSString stringWithUTF8String:node.notes.c_str()];

    NSMutableArray *flags = [NSMutableArray array];
    for (const auto &f : node.flags) {
      [flags addObject:[NSString stringWithUTF8String:f.name.c_str()]];
    }
    event[@"flags"] = flags;

    NSMutableArray *locations = [NSMutableArray array];
    for (const auto &loc : node.locations) {
      [locations addObject:[NSString stringWithUTF8String:loc.name.c_str()]];
    }
    event[@"locations"] = locations;

    NSMutableArray *text_ids = [NSMutableArray array];
    for (const auto &tid : node.text_ids) {
      [text_ids addObject:[NSString stringWithUTF8String:tid.c_str()]];
    }
    event[@"text_ids"] = text_ids;

    NSMutableArray *scripts = [NSMutableArray array];
    for (const auto &s : node.scripts) {
      [scripts addObject:[NSString stringWithUTF8String:s.c_str()]];
    }
    event[@"scripts"] = scripts;

    NSMutableArray *deps = [NSMutableArray array];
    for (const auto &d : node.dependencies) {
      [deps addObject:[NSString stringWithUTF8String:d.c_str()]];
    }
    event[@"dependencies"] = deps;

    NSMutableArray *unlocks = [NSMutableArray array];
    for (const auto &u : node.unlocks) {
      [unlocks addObject:[NSString stringWithUTF8String:u.c_str()]];
    }
    event[@"unlocks"] = unlocks;

    [events addObject:event];
  }

  NSError *error = nil;
  NSData *jsonData = [NSJSONSerialization dataWithJSONObject:events
                                                    options:0
                                                      error:&error];
  if (error || !jsonData) {
    return nil;
  }
  return [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
}

+ (NSArray<NSDictionary *> *)getDungeonRooms:(NSString *)dungeonId {
  if (!dungeonId) {
    return @[];
  }

  auto *controller = yaze::Application::Instance().GetController();
  if (!controller || !controller->editor_manager()) {
    return @[];
  }

  const auto &project = *controller->editor_manager()->GetCurrentProject();
  if (!project.hack_manifest.loaded() ||
      !project.hack_manifest.HasProjectRegistry()) {
    return @[];
  }

  std::string target_id([dungeonId UTF8String]);
  const auto &registry = project.hack_manifest.project_registry();

  for (const auto &dungeon : registry.dungeons) {
    if (dungeon.id == target_id) {
      NSMutableArray *rooms = [NSMutableArray array];
      for (const auto &room : dungeon.rooms) {
        [rooms addObject:@{
          @"room_id" : @(room.id),
          @"name" : [NSString stringWithUTF8String:room.name.c_str()],
          @"type" : [NSString stringWithUTF8String:room.type.c_str()],
        }];
      }
      return rooms;
    }
  }
  return @[];
}

@end
