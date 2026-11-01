#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sh tests/run_hook_gate.sh
trap 'rm -f tests/renderer_host' EXIT
clang -std=c11 -Wall -Wextra -Werror -fno-builtin -DPM_HOST -Itests -Ilayout \
  tests/renderer_host.c source/pm_shellui_render.c source/deck-layout-docked.c layout/deck-layout.c \
  -lm -o tests/renderer_host
if [ "$#" -eq 0 ]; then
  set -- retry failure-cleanup remove-retry visibility ctor-failure add-exception pending-term capture page exclusive-app pending-release theme
fi
for case in "$@"; do tests/renderer_host "$case"; done