#!/bin/bash
# Makes ArkOS start the rhythm machine at power-on, before the games menu: once per boot, and leaving it (hold
# SELECT + START) goes on to the games as before. It adds one marked block to ArkOS's start script and keeps the
# original next to it.
#   sudo tools/port/boot-into-rhythm.sh /run/media/$USER/root          do it
#   sudo tools/port/boot-into-rhythm.sh /run/media/$USER/root --undo   back to the games menu at power-on
set -e
ROOT="${1:?usage: $0 ROOT-PARTITION-OF-THE-R36S-CARD [--undo]}"
ES="$ROOT/usr/bin/emulationstation/emulationstation.sh"
[ -f "$ES" ] || { echo "no $ES: is $ROOT the R36S card's root partition?"; exit 1; }
if [ "$2" = "--undo" ]; then
  if [ -f "$ES.before-rhythm" ]; then cp -p "$ES.before-rhythm" "$ES" && sync && echo "undone: power-on goes to the games menu again"; else echo "nothing to undo"; fi
  exit 0
fi
if grep -q 'rhythm machine' "$ES"; then echo "already set up"; exit 0; fi
grep -q '^  esdir="$(dirname $0)"' "$ES" || { echo "the start script is not the one I know: not touching it"; exit 1; }
cp -p "$ES" "$ES.before-rhythm"
sed -i '/^  esdir="\$(dirname \$0)"/i\
  # the rhythm machine: at power-on, before the games menu (once per boot; hold SELECT + START to leave it).\
  # Added by boot-into-rhythm.sh; the original is emulationstation.sh.before-rhythm.\
  if [ -f /roms/ports/Rhythm.sh ] \&\& [ ! -e /dev/shm/rhythm-started ]; then\
    touch /dev/shm/rhythm-started\
    bash /roms/ports/Rhythm.sh\
  fi' "$ES"
sync
grep -n -A5 'the rhythm machine' "$ES"
echo "done: the R36S starts the rhythm machine at power-on"
