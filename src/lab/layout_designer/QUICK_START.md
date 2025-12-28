# Layout Designer - Quick Start Guide

## Opening the Designer

Build with `-DYAZE_BUILD_LAB=ON`, then run the `lab` executable. The designer
opens by default in the lab host.

---

## Mode 1: Panel Layout Design

**Use this to arrange where panels appear in your application**

### Visual Guide

```
┌──────────────────────────────────────────────────────────────┐
│ ◉ Panel Layout | ○ Widget Design                            │
├────────────┬─────────────────────────────┬──────────────────┤
│  PANELS    │         CANVAS              │   PROPERTIES     │
│            │                              │                  │
│ Dungeon ▼  │  Drag panels here →         │                  │
│  🏰 Room   │                              │                  │
│     List   │  [Drop zones appear         │                  │
│  📝 Object │   when dragging]             │                  │
│     Editor │                              │                  │
└────────────┴─────────────────────────────┴──────────────────┘
```

### Steps

1. **Search** for panels in left palette
2. **Drag** panel from palette
3. **Drop** on canvas (left/right/top/bottom/center)
4. **Watch** blue drop zone appear
5. **Release** to dock panel
6. **Repeat** to build complex layouts
7. **Save** as JSON file

### Example: 3-Panel Layout

```
Drag "Room List" → Drop LEFT
  Result: ┌─────┬───────┐
          │Room │       │
          │List │       │
          └─────┴───────┘

Drag "Object Editor" → Drop CENTER
  Result: ┌─────┬───────┐
          │Room │Object │
          │List │Editor │
          └─────┴───────┘

Drag "Palette" → Drop BOTTOM-RIGHT
  Result: ┌─────┬───────┐
          │Room │Object │
          │List ├───────┤
          │     │Palette│
          └─────┴───────┘
```

---

## Mode 2: Widget Design

**Use this to design what's INSIDE a panel**

### Visual Guide

```
┌──────────────────────────────────────────────────────────────┐
│ ○ Panel Layout | ◉ Widget Design                            │
├────────────┬─────────────────────────────┬──────────────────┤
│  WIDGETS   │         CANVAS              │   PROPERTIES     │
│            │                              │                  │
│ Basic ▼    │  Panel: My Panel            │ Widget: Button   │
│  📝 Text   │                              │                  │
│  🔘 Button │  ┌──────────────────┐       │ label: [Save ]   │
│  ☑️ Check  │  │ 📝 Text          │       │                  │
│  📋 Input  │  │ ➖ Separator     │       │ callback:        │
│            │  │ 🔘 Button        │       │ [OnSave     ]    │
│ Tables ▼   │  └──────────────────┘       │                  │
│  📊 Table  │  [Drop widgets here]        │ tooltip:         │
│  ➡️ Column │                              │ [Save file  ]    │
└────────────┴─────────────────────────────┴──────────────────┘
```

### Steps

1. **Switch** to Widget Design mode
2. **Create** new panel design (or select existing)
3. **Drag** widgets from palette
4. **Drop** on canvas to add
5. **Click** widget to select
6. **Edit** properties in right panel
7. **Export** code to copy

### Example: Simple Form

```
1. Drag "Text" widget
   → Set text: "Enter Name:"

2. Drag "InputText" widget  
   → Set label: "Name"
   → Set hint: "Your name here"

3. Drag "Button" widget
   → Set label: "Submit"
   → Set callback: "OnSubmit"
   → Set tooltip: "Submit the form"

4. Click "Export Code"

Generated:
  ImGui::Text("Enter Name:");
  ImGui::InputTextWithHint("Name", "Your name here", 
                           name_buffer_, sizeof(name_buffer_));
  if (ImGui::Button("Submit")) {
    OnSubmit();
  }
```

---

## Widget Types Cheat Sheet

### Basic Widgets
- 📝 **Text** - Display text
- 🔘 **Button** - Clickable button
- ☑️ **Checkbox** - Toggle boolean
- 📋 **InputText** - Text input field
- 🎚️ **Slider** - Value slider
- 🎨 **ColorEdit** - Color picker

### Layout Widgets
- ➖ **Separator** - Horizontal line
- ↔️ **SameLine** - Place next widget on same line
- ⬇️ **Spacing** - Add vertical space
- 📏 **Dummy** - Invisible spacing

### Tables
- 📊 **BeginTable** - Start a table (requires columns)
- ➡️ **TableNextColumn** - Move to next column
- ⬇️ **TableNextRow** - Move to next row

### Containers
- 📦 **BeginGroup** - Group widgets together
- 🪟 **BeginChild** - Scrollable sub-window
- 🌲 **TreeNode** - Collapsible tree
- 📑 **TabBar** - Tabbed interface

### Custom
- 🖌️ **Canvas** - Custom drawing area
- 📊 **ProgressBar** - Progress indicator
- 🖼️ **Image** - Display image

---

## Tips & Tricks

### Panel Layout Tips

**Tip 1: Use Drop Zones Strategically**
- **Left/Right** (30%) - Sidebars, lists
- **Top/Bottom** (25%) - Toolbars, status
- **Center** - Main content area

**Tip 2: Plan Before Designing**
- Sketch layout on paper first
- Identify main vs secondary panels
- Group related panels together

**Tip 3: Test with Real Data**
- Use Preview to see layout in action
- Check panel visibility and sizing
- Adjust ratios as needed

### Widget Design Tips

**Tip 1: Start Simple**
- Add title text first
- Add separators for structure
- Build form from top to bottom

**Tip 2: Use Same Line**
- Put related widgets on same line
- Use for button groups (Apply/Cancel)
- Use for label + input pairs

**Tip 3: Table Organization**
- Use tables for data grids
- Set columns before adding rows
- Enable scrolling for large datasets

**Tip 4: Group Related Widgets**
- Use BeginGroup for visual grouping
- Use BeginChild for scrollable sections
- Use CollapsingHeader for optional sections

---

## Common Patterns

### Pattern 1: Simple Panel
```
Text: "Title"
Separator
Content widgets...
Separator
Button: "Action"
```

### Pattern 2: Form Panel
```
Text: "Field 1:"
InputText: "field1"
Text: "Field 2:"
InputInt: "field2"
Separator
Button: "Submit" | SameLine | Button: "Cancel"
```

### Pattern 3: List Panel
```
Text: "Items"
Separator
BeginTable: 3 columns
  (Add rows in code)
EndTable
Separator
Button: "Add Item"
```

### Pattern 4: Tabbed Panel
```
TabBar: "tabs"
  TabItem: "General"
    (General widgets)
  TabItem: "Advanced"
    (Advanced widgets)
```

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New layout/design |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save file |
| `Ctrl+P` | Preview |
| `Del` | Delete selected (when implemented) |
| `Esc` | Close designer |

---

## Troubleshooting

### Q: Designer doesn't open
**A:** Make sure you're running the `lab` executable (built with `YAZE_BUILD_LAB`).

### Q: Palette is empty (Panel mode)
**A:** In the lab host, confirm `RegisterLabPanels()` is seeding descriptors. If embedding in the main editor, load a ROM so session panels register.

### Q: Palette is empty (Widget mode)
**A:** This is a bug. Widget palette should always show all 40+ widget types.

### Q: Can't drag widgets
**A:** Make sure you click and hold on the widget, then drag to canvas.

### Q: Properties don't save
**A:** Changes to properties are immediate. No "Apply" button needed currently.

### Q: Generated code doesn't compile
**A:** 
- Check that all callbacks exist in your panel class
- Add member variables for widget state
- Replace TODO comments with actual logic

### Q: How to delete a widget?
**A:** Currently not implemented. Will be added in Phase 10.

---

## Examples

### Example 1: Object Properties Panel

**Design in Widget Mode:**
1. Text: "Object Properties"
2. Separator
3. InputInt: "ID" (0-255)
4. InputInt: "X" (0-512)
5. InputInt: "Y" (0-512)
6. Checkbox: "Visible"
7. Separator
8. Button: "Apply"

**Result:** Clean property editor for objects

### Example 2: Room Selector Panel

**Design in Widget Mode:**
1. Text: "Dungeon Rooms"
2. Separator
3. BeginTable: 4 columns (ID, Name, Type, Actions)
4. Button: "Add Room"

**Result:** Professional room list with table

### Example 3: Complex Dashboard

**Design in Panel Layout Mode:**
1. Left (20%): Navigation panel
2. Center (60%): Main editor
3. Right-Top (20%, 50%): Properties
4. Right-Bottom (20%, 50%): Preview

**Then design each panel in Widget Mode:**
- Navigation: Tree of categories
- Main: Canvas widget
- Properties: Form widgets
- Preview: Image widget

**Result:** Complete IDE-like interface!

---

## Support

**Documentation:** See `docs/internal/architecture/imgui-layout-designer.md`  
**Examples:** Check `docs/internal/architecture/layout-designer-integration-example.md`  
**Issues:** Report in yaze repository

---

Happy designing! 🎨
