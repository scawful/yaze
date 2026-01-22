# macOS Productivity Integration (Yabai & Sketchybar)

This guide describes how to integrate `yaze` server and headless modes into a macOS productivity setup using `yabai`, `skhd`, and `sketchybar`.

## 1. Yabai Window Rules

If you are using the `yaze` GUI but want it to behave well in a tiling environment, add these rules to your `.yabairow`:

```bash
# Float small dialogs and floating cards
yabai -m rule --add app="^Yaze$" title="^Popup" manage=off
yabai -m rule --add app="^Yaze$" title="^Dashboard" manage=off
yabai -m rule --add app="^Yaze$" title="^Welcome" manage=off

# Keep the main editor tiled
yabai -m rule --add app="^Yaze$" title="^Yet Another Zelda3 Editor" manage=on

## 2. Sketchybar Status Indicator

You can create a `sketchybar` item to monitor if the `yaze` server is running.

### Create the script `~/.config/sketchybar/plugins/yaze_status.sh`:

```bash
#!/bin/bash

# Check if yaze is running in server mode (port 8080)
HEALTH=$(curl -s -o /dev/null -w "%{{http_code}}" http://localhost:8080/api/v1/health)

if [ "$HEALTH" == "200" ]; then
  sketchybar --set $NAME icon=󰒄 label="YAZE" icon.color=0xffa6da95
else
  sketchybar --set $NAME icon=󰒅 label="" icon.color=0xffed8796
fi
```

### Add to `sketchybarrc`:

```bash
sketchybar --add item yaze_status right \
           --set yaze_status update_obj=10 \
           --set yaze_status script="~/.config/sketchybar/plugins/yaze_status.sh"
```

## 3. Skhd Hotkeys

Launch or restart the server with a keystroke in your `.skhdrc`:

```bash
# Start Yaze Server headlessly
ctrl + alt - y : ./build_ai/bin/yaze --server --rom_file ~/roms/zelda3.sfc &

# Kill Yaze Server
ctrl + shift + alt - y : pkill -f "yaze --server"
```

## 4. Headless & Yabai Workflow

Since `--headless` and `--server` modes create **no windows**, they are completely invisible to `yabai`. This allows you to:
1.  Run the `yaze` server in the background.
2.  Work in your terminal or VS Code.
3.  Have Mesen 2 (which is visible to `yabai`) sync its labels from the background `yaze` process.

```
