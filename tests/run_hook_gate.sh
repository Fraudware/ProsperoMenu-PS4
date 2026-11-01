#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
trap 'rm -f tests/hook_gate_host tests/dispatch_host' EXIT
clang -std=c11 -Wall -Wextra -Werror -Isource tests/hook_gate_host.c -o tests/hook_gate_host
tests/hook_gate_host

clang -std=c11 -Wall -Wextra -Werror -Isource tests/dispatch_host.c -o tests/dispatch_host
tests/dispatch_host
