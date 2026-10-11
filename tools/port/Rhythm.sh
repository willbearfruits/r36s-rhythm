#!/bin/bash
# The R36S rhythm machine, as an ArkOS Port. Hold SELECT + START to leave.
# Uses PortMaster's controller mapping when PortMaster is there; without it the pad is read raw (see log.txt).
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then controlfolder="$XDG_DATA_HOME/PortMaster"
else controlfolder="/roms/ports/PortMaster"; fi
if [ -f "$controlfolder/control.txt" ]; then
  source "$controlfolder/control.txt"
  get_controls
  [ -n "$sdl_controllerconfig" ] && export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
fi

GAMEDIR="$(cd "$(dirname "$0")" && pwd)/rhythm"
cd "$GAMEDIR" || exit 1
export CARD="$GAMEDIR/card"
export RHYTHM_LOG="$GAMEDIR/log.txt"
chmod +x ./rhythm
./rhythm > "$GAMEDIR/stdout.txt" 2>&1
printf "\033c" > /dev/tty1 2>/dev/null
