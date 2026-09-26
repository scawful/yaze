// AppDelegate.mm

// Must define before any ImGui includes (needed by imgui_test_engine via editor_manager.h)
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#import "app/platform/app_delegate.h"
#import "app/controller.h"
#import "app/application.h"
#import "app/editor/shell/coordinator/ui_coordinator.h"
#import "app/editor/shell/feedback/popup_manager.h"
#import "app/editor/system/commands/shortcut_manager.h"
#import "app/editor/system/workspace/workspace_window_manager.h"
#import "app/platform/native_menu_bridge.h"
#import "core/project.h"
#import "util/file_util.h"
#import "app/editor/editor.h"
#import "rom/rom.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
/* Apple OSX and iOS (Darwin). */
#include <TargetConditionals.h>

#import <CoreText/CoreText.h>

#if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
/* iOS in Xcode simulator */

#elif TARGET_OS_MAC == 1
/* macOS */
#import <Cocoa/Cocoa.h>

#ifndef YAZE_USE_SDL3
#include <SDL.h>
#endif

namespace nm = yaze::platform::native_menu;

namespace {

// This file builds without ARC; hand freshly allocated menu objects to the
// current autorelease pool so their owning NSMenu holds the only reference.
template <typename T>
T* Autoreleased(T* object) {
#if __has_feature(objc_arc)
  return object;
#else
  return [object autorelease];
#endif
}

yaze::editor::EditorManager* CurrentEditorManager() {
  auto& app = yaze::Application::Instance();
  if (!app.IsReady() || app.GetController() == nullptr) {
    return nullptr;
  }
  return app.GetController()->editor_manager();
}

// NativeMenuHost over the live EditorManager. Every side effect is queued on
// EditorManager's deferred-action queue so it runs inside the next ImGui
// frame, exactly where ShortcutManager callbacks run for keyboard chords.
class EditorManagerMenuHost final : public nm::NativeMenuHost {
 public:
  yaze::editor::ShortcutManager* shortcut_manager() override {
    auto* manager = CurrentEditorManager();
    return manager ? manager->shortcut_manager() : nullptr;
  }

  bool HasLoadedRom() const override {
    auto* manager = CurrentEditorManager();
    if (manager == nullptr) {
      return false;
    }
    auto* rom = manager->GetCurrentRom();
    return rom != nullptr && rom->is_loaded();
  }

  bool HasCurrentEditor() const override {
    auto* manager = CurrentEditorManager();
    return manager != nullptr && manager->GetCurrentEditor() != nullptr;
  }

  // Not every editor keeps its history in the shared UndoManager (palette and
  // assembly editors use their own stacks, others hold a pending action until
  // Undo() finalizes it), so an empty UndoManager is not proof that Undo would
  // do nothing. Enable whenever an editor is active; the title carries the
  // UndoManager description when there is one.
  bool CanUndo() const override { return HasCurrentEditor(); }
  bool CanRedo() const override { return HasCurrentEditor(); }

  bool WantsTextInput() const override {
    return ImGui::GetCurrentContext() != nullptr &&
           ImGui::GetIO().WantTextInput;
  }

  void Defer(std::function<void()> action) override {
    if (auto* manager = CurrentEditorManager()) {
      manager->QueueDeferredAction(std::move(action));
    }
  }

  void InjectCommandChord(ImGuiKey key) override {
    if (ImGui::GetCurrentContext() == nullptr) {
      return;
    }
    ImGuiIO& io = ImGui::GetIO();
    // The SDL backend reports Cmd as Super; ImGui swaps it to Ctrl when
    // ConfigMacOSXBehaviors is on. Feed the same thing a real Cmd+key sends.
    const ImGuiKey mod = io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
    io.AddKeyEvent(mod, true);
    io.AddKeyEvent(key, true);
    io.AddKeyEvent(key, false);
    io.AddKeyEvent(mod, false);
  }

  void RunHostAction(nm::MenuAction action) override {
    auto* manager = CurrentEditorManager();
    if (manager == nullptr) {
      return;
    }
    manager->QueueDeferredAction([action]() {
      auto* m = CurrentEditorManager();
      if (m == nullptr) {
        return;
      }
      switch (action) {
        case nm::MenuAction::kAbout:
          if (auto* popups = m->popup_manager()) {
            popups->Show(yaze::editor::PopupID::kAbout);
          }
          break;
        case nm::MenuAction::kSettings:
          m->SwitchToEditor(yaze::editor::EditorType::kSettings);
          break;
        case nm::MenuAction::kQuit:
          m->Quit();
          break;
        case nm::MenuAction::kOpenRom:
          (void)m->LoadRom();
          break;
        case nm::MenuAction::kOpenProject:
          (void)m->OpenProject();
          break;
        case nm::MenuAction::kSave:
          (void)m->SaveRom();
          break;
        case nm::MenuAction::kSaveAs:
          if (auto* popups = m->popup_manager()) {
            popups->Show(yaze::editor::PopupID::kSaveAs);
          }
          break;
        case nm::MenuAction::kCloseRom:
          m->CloseCurrentSession();
          break;
        case nm::MenuAction::kUndo:
          if (auto* editor = m->GetCurrentEditor()) {
            (void)editor->Undo();
          }
          break;
        case nm::MenuAction::kRedo:
          if (auto* editor = m->GetCurrentEditor()) {
            (void)editor->Redo();
          }
          break;
        case nm::MenuAction::kToggleSidebar:
          m->window_manager().ToggleSidebarVisibility();
          break;
        case nm::MenuAction::kKeyboardShortcuts:
          if (auto* ui = m->ui_coordinator()) {
            ui->ShowShortcutsBrowser();
          }
          break;
        default:
          break;
      }
    });
  }

  std::vector<std::string> RecentFiles() const override {
    return yaze::project::RecentFilesManager::GetInstance().GetRecentFiles();
  }

  void OpenRecentFile(const std::string& path) override {
    Defer([path]() {
      if (auto* m = CurrentEditorManager()) {
        (void)m->OpenRomOrProject(path);
      }
    });
  }

  void ClearRecentFiles() override {
    auto& recent = yaze::project::RecentFilesManager::GetInstance();
    recent.Clear();
    recent.Save();
  }
};

EditorManagerMenuHost& MenuHost() {
  static EditorManagerMenuHost host;
  return host;
}

NSEventModifierFlags ModifierFlagsForChord(const nm::KeyChord& chord) {
  NSEventModifierFlags flags = 0;
  if (chord.command) flags |= NSEventModifierFlagCommand;
  if (chord.shift) flags |= NSEventModifierFlagShift;
  if (chord.option) flags |= NSEventModifierFlagOption;
  if (chord.control) flags |= NSEventModifierFlagControl;
  return flags;
}

nm::KeyChord ChordFromNSEvent(NSEvent* event) {
  nm::KeyChord chord;
  if (event.type != NSEventTypeKeyDown) {
    return chord;
  }
  NSString* chars = nil;
  if (@available(macOS 10.15, *)) {
    chars = [event charactersByApplyingModifiers:0];
  }
  if (chars.length == 0) {
    chars = event.charactersIgnoringModifiers;
  }
  if (chars.length != 1) {
    return chord;
  }
  const unichar c = [chars.lowercaseString characterAtIndex:0];
  if (c < 0x20 || c > 0x7e) {
    return chord;
  }
  const NSEventModifierFlags flags = event.modifierFlags;
  chord.key = static_cast<char>(c);
  chord.command = (flags & NSEventModifierFlagCommand) != 0;
  chord.shift = (flags & NSEventModifierFlagShift) != 0;
  chord.option = (flags & NSEventModifierFlagOption) != 0;
  chord.control = (flags & NSEventModifierFlagControl) != 0;
  return chord;
}

void ApplyChordToItem(NSMenuItem* item, const nm::ResolvedItem& resolved) {
  if (!resolved.chord.valid()) {
    item.keyEquivalent = @"";
    item.keyEquivalentModifierMask = 0;
    return;
  }
  item.keyEquivalent = [NSString stringWithFormat:@"%c", resolved.chord.key];
  item.keyEquivalentModifierMask = ModifierFlagsForChord(resolved.chord);
}

#ifndef YAZE_USE_SDL3
SDL_EventFilter g_prev_sdl_filter = nullptr;
void* g_prev_sdl_filter_userdata = nullptr;

nm::KeyChord ChordFromSdlKey(const SDL_KeyboardEvent& key) {
  nm::KeyChord chord;
  const SDL_Keycode sym = key.keysym.sym;
  if (sym < 0x20 || sym > 0x7e) {
    return chord;
  }
  const Uint16 mod = key.keysym.mod;
  chord.key = static_cast<char>(sym >= 'A' && sym <= 'Z' ? sym - 'A' + 'a' : sym);
  chord.command = (mod & KMOD_GUI) != 0;
  chord.shift = (mod & KMOD_SHIFT) != 0;
  chord.option = (mod & KMOD_ALT) != 0;
  chord.control = (mod & KMOD_CTRL) != 0;
  return chord;
}

// SDL's Cocoa pump turns every NSEvent into an SDL key event *before*
// [NSApp sendEvent:] lets NSMenu perform key equivalents. For chords the
// native menu owns, drop the SDL copy so ImGui/ShortcutManager never sees it
// (ShortcutManager matches modifiers loosely: Ctrl+Cmd+F would also fire Find).
int YazeNativeMenuSdlFilter(void* userdata, SDL_Event* event) {
  (void)userdata;
  if (event != nullptr &&
      (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP)) {
    const nm::KeyChord chord = ChordFromSdlKey(event->key);
    if (chord.valid() &&
        nm::IsNativeOwnedChord(MenuHost().shortcut_manager(), chord)) {
      return 0;
    }
  }
  if (g_prev_sdl_filter != nullptr) {
    return g_prev_sdl_filter(g_prev_sdl_filter_userdata, event);
  }
  return 1;
}
#endif  // YAZE_USE_SDL3

}  // namespace

// Root menu: never performs a key equivalent ImGui owns. ImGui-owned chords (⌘S, ⌘Z, ⌘W, ...) are displayed on their items for
// discoverability but fall through so SDL/ImGui runs them exactly once.
@interface YazeMainMenu : NSMenu
@end

@implementation YazeMainMenu
- (BOOL)performKeyEquivalent:(NSEvent*)event {
  const nm::KeyChord chord = ChordFromNSEvent(event);
  if (!nm::ShouldMenuPerformKeyEquivalent(MenuHost().shortcut_manager(),
                                          chord)) {
    return NO;
  }
  return [super performKeyEquivalent:event];
}
@end

@interface YazeMenuController : NSObject <NSMenuDelegate, NSMenuItemValidation>
@property(nonatomic, assign) NSMenu* openRecentMenu;
- (void)performMenuAction:(NSMenuItem*)sender;
- (void)openRecentFile:(NSMenuItem*)sender;
- (void)clearRecentFiles:(NSMenuItem*)sender;
@end

@implementation YazeMenuController

- (void)performMenuAction:(NSMenuItem*)sender {
  nm::PerformAction(MenuHost(), static_cast<nm::MenuAction>(sender.tag));
}

- (void)openRecentFile:(NSMenuItem*)sender {
  NSString* path = sender.representedObject;
  if (path.length > 0) {
    MenuHost().OpenRecentFile(path.UTF8String);
  }
}

- (void)clearRecentFiles:(NSMenuItem*)sender {
  (void)sender;
  MenuHost().ClearRecentFiles();
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
  if (item.action == @selector(performMenuAction:)) {
    const auto action = static_cast<nm::MenuAction>(item.tag);
    if (action == nm::MenuAction::kUndo || action == nm::MenuAction::kRedo) {
      std::string title = action == nm::MenuAction::kUndo ? "Undo" : "Redo";
      if (auto* manager = CurrentEditorManager()) {
        if (auto* editor = manager->GetCurrentEditor()) {
          const std::string desc = action == nm::MenuAction::kUndo
                                       ? editor->GetUndoDescription()
                                       : editor->GetRedoDescription();
          if (!desc.empty()) {
            title += " " + desc;
          }
        }
      }
      item.title = [NSString stringWithUTF8String:title.c_str()];
    }
    return nm::IsActionEnabled(MenuHost(), action) ? YES : NO;
  }
  if (item.action == @selector(openRecentFile:)) {
    return CurrentEditorManager() != nullptr;
  }
  if (item.action == @selector(clearRecentFiles:)) {
    return !MenuHost().RecentFiles().empty();
  }
  return YES;
}

- (void)rebuildOpenRecent:(NSMenu*)menu {
  [menu removeAllItems];
  const std::vector<std::string> files = MenuHost().RecentFiles();
  for (const std::string& path : files) {
    NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
    NSString* label = [NSString
        stringWithUTF8String:nm::RecentFileLabel(path).c_str()];
    if (nsPath == nil || label == nil) {
      continue;
    }
    NSMenuItem* item = Autoreleased([[NSMenuItem alloc] initWithTitle:label
                                                  action:@selector(openRecentFile:)
                                           keyEquivalent:@""]);
    item.target = self;
    item.representedObject = nsPath;
    item.toolTip = nsPath;
    [menu addItem:item];
  }
  if (!files.empty()) {
    [menu addItem:[NSMenuItem separatorItem]];
  }
  NSMenuItem* clear = Autoreleased([[NSMenuItem alloc] initWithTitle:@"Clear Menu"
                                                 action:@selector(clearRecentFiles:)
                                          keyEquivalent:@""]);
  clear.target = self;
  [menu addItem:clear];
}

// Refresh chords every time a menu opens so rebinding in ShortcutManager (or a
// compiled-out shortcut) is reflected, and repopulate Open Recent.
- (void)menuNeedsUpdate:(NSMenu*)menu {
  if (menu == self.openRecentMenu) {
    [self rebuildOpenRecent:menu];
    return;
  }
  auto* shortcuts = MenuHost().shortcut_manager();
  for (NSMenuItem* item in menu.itemArray) {
    if (item.isSeparatorItem || item.hasSubmenu || item.tag < 0 ||
        item.tag >= static_cast<NSInteger>(nm::MenuAction::kCount)) {
      continue;
    }
    ApplyChordToItem(item, nm::ResolveItem(
                               shortcuts, static_cast<nm::MenuAction>(item.tag)));
  }
}

@end

namespace {

YazeMenuController* g_menu_controller = nil;

SEL CocoaSelectorFor(nm::MenuAction action) {
  switch (action) {
    case nm::MenuAction::kHide:
      return @selector(hide:);
    case nm::MenuAction::kHideOthers:
      return @selector(hideOtherApplications:);
    case nm::MenuAction::kShowAll:
      return @selector(unhideAllApplications:);
    case nm::MenuAction::kToggleFullScreen:
      return @selector(toggleFullScreen:);
    case nm::MenuAction::kMinimize:
      return @selector(performMiniaturize:);
    case nm::MenuAction::kZoom:
      return @selector(performZoom:);
    case nm::MenuAction::kBringAllToFront:
      return @selector(arrangeInFront:);
    default:
      return nullptr;
  }
}

NSMenuItem* AddActionItem(NSMenu* menu, nm::MenuAction action) {
  const nm::ActionSpec* spec = nm::FindSpec(action);
  NSString* title = [NSString stringWithUTF8String:spec ? spec->title : "?"];
  const nm::ResolvedItem resolved =
      nm::ResolveItem(MenuHost().shortcut_manager(), action);
  NSMenuItem* item = nil;
  if (spec != nullptr && spec->dispatch == nm::Dispatch::kCocoa) {
    // nil target: the responder chain routes these to NSApp or the SDL
    // NSWindow, which also validates them (e.g. Minimize with no window).
    item = Autoreleased([[NSMenuItem alloc] initWithTitle:title
                                      action:CocoaSelectorFor(action)
                               keyEquivalent:@""]);
  } else {
    item = Autoreleased([[NSMenuItem alloc] initWithTitle:title
                                      action:@selector(performMenuAction:)
                               keyEquivalent:@""]);
    item.target = g_menu_controller;
  }
  item.tag = static_cast<NSInteger>(action);
  ApplyChordToItem(item, resolved);
  [menu addItem:item];
  return item;
}

NSMenu* AddTopLevelMenu(NSMenu* mainMenu, NSString* title) {
  NSMenu* menu = Autoreleased([[NSMenu alloc] initWithTitle:title]);
  menu.delegate = g_menu_controller;
  NSMenuItem* holder = Autoreleased([[NSMenuItem alloc] initWithTitle:title
                                                  action:nil
                                           keyEquivalent:@""]);
  holder.submenu = menu;
  [mainMenu addItem:holder];
  return menu;
}

// Build and install the main menu. Called after Application::Initialize, so
// the SDL window exists and EditorManager has configured ShortcutManager.
// SDL does not create its own menu here (NSApp already existed when SDL
// registered), so this replaces whatever mainMenu there is.
void InstallNativeMainMenu() {
  if (g_menu_controller == nil) {
    g_menu_controller = [[YazeMenuController alloc] init];
  }
  YazeMainMenu* mainMenu = Autoreleased([[YazeMainMenu alloc] initWithTitle:@"MainMenu"]);

  // App menu (title is replaced by the process name in the menu bar).
  NSMenu* appMenu = AddTopLevelMenu(mainMenu, @"yaze");
  AddActionItem(appMenu, nm::MenuAction::kAbout);
  [appMenu addItem:[NSMenuItem separatorItem]];
  AddActionItem(appMenu, nm::MenuAction::kSettings);
  [appMenu addItem:[NSMenuItem separatorItem]];
  NSMenu* servicesMenu = Autoreleased([[NSMenu alloc] initWithTitle:@"Services"]);
  NSMenuItem* servicesItem = Autoreleased([[NSMenuItem alloc] initWithTitle:@"Services"
                                                        action:nil
                                                 keyEquivalent:@""]);
  servicesItem.submenu = servicesMenu;
  servicesItem.tag = -1;
  [appMenu addItem:servicesItem];
  [NSApp setServicesMenu:servicesMenu];
  [appMenu addItem:[NSMenuItem separatorItem]];
  AddActionItem(appMenu, nm::MenuAction::kHide);
  AddActionItem(appMenu, nm::MenuAction::kHideOthers);
  AddActionItem(appMenu, nm::MenuAction::kShowAll);
  [appMenu addItem:[NSMenuItem separatorItem]];
  AddActionItem(appMenu, nm::MenuAction::kQuit);

  NSMenu* fileMenu = AddTopLevelMenu(mainMenu, @"File");
  AddActionItem(fileMenu, nm::MenuAction::kOpenRom);
  AddActionItem(fileMenu, nm::MenuAction::kOpenProject);
  NSMenu* recentMenu = Autoreleased([[NSMenu alloc] initWithTitle:@"Open Recent"]);
  recentMenu.delegate = g_menu_controller;
  g_menu_controller.openRecentMenu = recentMenu;
  NSMenuItem* recentItem = Autoreleased([[NSMenuItem alloc] initWithTitle:@"Open Recent"
                                                      action:nil
                                               keyEquivalent:@""]);
  recentItem.submenu = recentMenu;
  recentItem.tag = -1;
  [fileMenu addItem:recentItem];
  [g_menu_controller rebuildOpenRecent:recentMenu];
  [fileMenu addItem:[NSMenuItem separatorItem]];
  AddActionItem(fileMenu, nm::MenuAction::kSave);
  AddActionItem(fileMenu, nm::MenuAction::kSaveAs);
  [fileMenu addItem:[NSMenuItem separatorItem]];
  AddActionItem(fileMenu, nm::MenuAction::kCloseRom);

  NSMenu* editMenu = AddTopLevelMenu(mainMenu, @"Edit");
  AddActionItem(editMenu, nm::MenuAction::kUndo);
  AddActionItem(editMenu, nm::MenuAction::kRedo);
  [editMenu addItem:[NSMenuItem separatorItem]];
  AddActionItem(editMenu, nm::MenuAction::kCut);
  AddActionItem(editMenu, nm::MenuAction::kCopy);
  AddActionItem(editMenu, nm::MenuAction::kPaste);
  AddActionItem(editMenu, nm::MenuAction::kSelectAll);

  NSMenu* viewMenu = AddTopLevelMenu(mainMenu, @"View");
  AddActionItem(viewMenu, nm::MenuAction::kToggleSidebar);
  [viewMenu addItem:[NSMenuItem separatorItem]];
  AddActionItem(viewMenu, nm::MenuAction::kToggleFullScreen);

  NSMenu* windowMenu = AddTopLevelMenu(mainMenu, @"Window");
  AddActionItem(windowMenu, nm::MenuAction::kMinimize);
  AddActionItem(windowMenu, nm::MenuAction::kZoom);
  [windowMenu addItem:[NSMenuItem separatorItem]];
  AddActionItem(windowMenu, nm::MenuAction::kBringAllToFront);

  NSMenu* helpMenu = AddTopLevelMenu(mainMenu, @"Help");
  AddActionItem(helpMenu, nm::MenuAction::kKeyboardShortcuts);

  [NSApp setMainMenu:mainMenu];
  // setAppleMenu: is not public API on recent SDKs; the first item of the
  // main menu is the application menu by convention.
  [NSApp setWindowsMenu:windowMenu];
  [NSApp setHelpMenu:helpMenu];

#ifndef YAZE_USE_SDL3
  SDL_EventFilter existing = nullptr;
  void* existing_userdata = nullptr;
  if (SDL_GetEventFilter(&existing, &existing_userdata) &&
      existing != YazeNativeMenuSdlFilter) {
    g_prev_sdl_filter = existing;
    g_prev_sdl_filter_userdata = existing_userdata;
  }
  SDL_SetEventFilter(YazeNativeMenuSdlFilter, nullptr);
#endif
}

std::string DescribeChord(const nm::KeyChord& chord) {
  if (!chord.valid()) {
    return "-";
  }
  std::string out;
  if (chord.control) out += "Ctrl+";
  if (chord.option) out += "Opt+";
  if (chord.shift) out += "Shift+";
  if (chord.command) out += "Cmd+";
  out += chord.key;
  return out;
}

const char* OwnerName(nm::ChordOwner owner) {
  switch (owner) {
    case nm::ChordOwner::kImGui:
      return "imgui";
    case nm::ChordOwner::kNative:
      return "native";
    default:
      return "none";
  }
}

// Diagnostics for hidden runs: YAZE_NATIVE_MENU_DUMP=<path> appends the live
// menu tree, chord ownership, validation state, and dispatch self-checks.
// Read-only: the self-checks never perform an ImGui-owned chord, and they
// only ask whether a native chord is claimed (never perform ⌘M etc).
void DumpNativeMenu(const char* phase) {
  const char* path = std::getenv("YAZE_NATIVE_MENU_DUMP");
  if (path == nullptr || path[0] == '\0') {
    return;
  }
  FILE* out = std::fopen(path, "a");
  if (out == nullptr) {
    return;
  }
  auto* shortcuts = MenuHost().shortcut_manager();
  std::fprintf(out, "== phase=%s shortcut_manager=%s rom_loaded=%d\n", phase,
               shortcuts ? "yes" : "no", MenuHost().HasLoadedRom() ? 1 : 0);
  NSMenu* mainMenu = [NSApp mainMenu];
  std::fprintf(out, "mainMenu class=%s items=%ld\n",
               mainMenu ? NSStringFromClass([mainMenu class]).UTF8String : "nil",
               static_cast<long>(mainMenu.numberOfItems));
  for (NSMenuItem* top in mainMenu.itemArray) {
    NSMenu* sub = top.submenu;
    // Mirror what AppKit does before display so chords/titles are current.
    if (sub.delegate != nil &&
        [sub.delegate respondsToSelector:@selector(menuNeedsUpdate:)]) {
      [sub.delegate menuNeedsUpdate:sub];
    }
    std::fprintf(out, "[%s]\n", top.title.UTF8String);
    for (NSMenuItem* item in sub.itemArray) {
      if (item.isSeparatorItem) {
        std::fprintf(out, "  ---\n");
        continue;
      }
      std::string owner = "n/a";
      std::string resolved_chord = "-";
      if (item.tag >= 0 &&
          item.tag < static_cast<NSInteger>(nm::MenuAction::kCount)) {
        const auto r = nm::ResolveItem(
            shortcuts, static_cast<nm::MenuAction>(item.tag));
        owner = OwnerName(r.owner);
        resolved_chord = DescribeChord(r.chord);
      }
      BOOL enabled = YES;
      id target = item.target;
      if (target != nil &&
          [target respondsToSelector:@selector(validateMenuItem:)]) {
        enabled = [target validateMenuItem:item];
      }
      std::fprintf(out,
                   "  %-24s key='%s' mods=0x%lx chord=%s owner=%s "
                   "action=%s enabled=%d%s%s\n",
                   item.title.UTF8String, item.keyEquivalent.UTF8String,
                   static_cast<unsigned long>(item.keyEquivalentModifierMask),
                   resolved_chord.c_str(), owner.c_str(),
                   item.action ? NSStringFromSelector(item.action).UTF8String
                               : "(none)",
                   enabled ? 1 : 0, item.hasSubmenu ? " [submenu]" : "",
                   item.isHidden ? " [hidden]" : "");
      if (item.hasSubmenu) {
        for (NSMenuItem* child in item.submenu.itemArray) {
          std::fprintf(out, "      - %s\n",
                       child.isSeparatorItem ? "---" : child.title.UTF8String);
        }
      }
    }
  }

  // Self-check 1: the real NSMenu must decline ImGui-owned chords, so ⌘S
  // reaches only SDL/ImGui. Safe to call: a NO result performs nothing.
  struct Probe {
    const char* label;
    NSString* chars;
    unsigned short key_code;
    NSEventModifierFlags flags;
  };
  const Probe probes[] = {
      {"Cmd+S", @"s", 1, NSEventModifierFlagCommand},
      {"Cmd+Z", @"z", 6, NSEventModifierFlagCommand},
      {"Cmd+W", @"w", 13, NSEventModifierFlagCommand},
      {"Cmd+Q", @"q", 12, NSEventModifierFlagCommand},
  };
  for (const Probe& probe : probes) {
    NSEvent* ev = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                   location:NSZeroPoint
                              modifierFlags:probe.flags
                                  timestamp:0
                               windowNumber:0
                                    context:nil
                                 characters:probe.chars
                charactersIgnoringModifiers:probe.chars
                                  isARepeat:NO
                                    keyCode:probe.key_code];
    const nm::KeyChord chord = ChordFromNSEvent(ev);
    // Only call into AppKit for chords the gate declines; a declined chord
    // performs nothing, so the probe has no side effects.
    if (nm::ShouldMenuPerformKeyEquivalent(shortcuts, chord)) {
      std::fprintf(out, "selfcheck nsmenu %s gate=native (not probed)\n",
                   probe.label);
      continue;
    }
    const BOOL performed = [mainMenu performKeyEquivalent:ev];
    std::fprintf(out, "selfcheck nsmenu %s gate=imgui performed=%d\n",
                 probe.label, performed ? 1 : 0);
  }

#ifndef YAZE_USE_SDL3
  // Self-check 2: the SDL filter keeps ImGui-owned chords and drops the SDL
  // copy of native-owned ones (evaluated directly, no events are posted).
  struct SdlProbe {
    const char* label;
    SDL_Keycode sym;
    Uint16 mod;
  };
  const SdlProbe sdl_probes[] = {
      {"Cmd+S", SDLK_s, KMOD_LGUI},
      {"Cmd+Q", SDLK_q, KMOD_LGUI},
      {"Cmd+W", SDLK_w, KMOD_LGUI},
      {"Cmd+,", SDLK_COMMA, KMOD_LGUI},
      {"Cmd+M", SDLK_m, KMOD_LGUI},
      {"Ctrl+Cmd+F", SDLK_f, static_cast<Uint16>(KMOD_LGUI | KMOD_LCTRL)},
      {"Opt+Cmd+H", SDLK_h, static_cast<Uint16>(KMOD_LGUI | KMOD_LALT)},
      {"Cmd+H", SDLK_h, KMOD_LGUI},
  };
  for (const SdlProbe& probe : sdl_probes) {
    SDL_Event ev{};
    ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = probe.sym;
    ev.key.keysym.mod = probe.mod;
    const int keep = YazeNativeMenuSdlFilter(nullptr, &ev);
    std::fprintf(out, "selfcheck sdlfilter %s keep=%d\n", probe.label, keep);
  }
#endif
  std::fclose(out);
}

}  // namespace

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  // The main menu is installed after SDL initializes (InstallNativeMainMenu);
  // [NSApp mainMenu] is still nil here because there is no nib.

  // Disable automatic UI state persistence to prevent crashes
  // macOS NSPersistentUIManager can crash if state gets corrupted
  [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"NSQuitAlwaysKeepsWindows"];

  // The native menu adds its own View > Enter Full Screen (⌃⌘F); stop AppKit
  // from inserting a second copy.
  [[NSUserDefaults standardUserDefaults] setBool:NO
                                          forKey:@"NSFullScreenMenuItemEverywhere"];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:
    (NSApplication *)sender {
  (void)sender;
  auto& app = yaze::Application::Instance();
  if (!app.IsReady() || app.GetController() == nullptr ||
      app.GetController()->editor_manager() == nullptr) {
    return NSTerminateNow;
  }

  // Cocoa's default terminate path calls exit() immediately. Because yaze
  // drives its own SDL/ImGui loop instead of NSApplication::run, that bypasses
  // Application::Shutdown and leaves the ImGui test engine bound to a live
  // ImGui context. Route native Quit through EditorManager so unsaved-work
  // guarding runs and the main loop can unwind resources in ownership order.
  app.GetController()->editor_manager()->Quit();
  return NSTerminateCancel;
}

@end

extern "C" void yaze_initialize_cocoa() {
  @autoreleasepool {
    AppDelegate *delegate = [[AppDelegate alloc] init];
    [NSApplication sharedApplication];
    [NSApp setDelegate:delegate];
    [NSApp finishLaunching];
  }
}

extern "C" int yaze_run_cocoa_app_delegate(const yaze::AppConfig& config) {
  yaze_initialize_cocoa();

  // Initialize the Application singleton with the provided config
  // This will create the Controller and the SDL Window
  yaze::Application::Instance().Initialize(config);

  // Main loop
  // We continue to run our own loop rather than [NSApp run]
  // because we're driving SDL/ImGui manually.
  // SDL's event polling works fine with Cocoa in this setup.

  auto& app = yaze::Application::Instance();

  if (app.IsReady()) {
    @autoreleasepool {
      InstallNativeMainMenu();
      DumpNativeMenu("install");
    }
  }

  const auto started = std::chrono::steady_clock::now();
  bool late_dump_done = false;
  while (app.IsReady() && app.GetController()->IsActive()) {
    @autoreleasepool {
      app.Tick();
      if (!late_dump_done &&
          std::chrono::steady_clock::now() - started > std::chrono::seconds(10)) {
        late_dump_done = true;
        DumpNativeMenu("late");
      }
    }
  }

  app.Shutdown();
  return EXIT_SUCCESS;
}

#endif

#endif
