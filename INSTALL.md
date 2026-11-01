# Installation

ProsperoMenu runs as a dashboard overlay using two `.prx` modules and a `.bin` starter payload.

## Compatibility

Tested on PS4 Pro running firmware 13.52 with GoldHEN. Other firmware versions have not been verified. The current loader checks for 13.52 and rejects other versions.

## Transfer the modules

Copy the two modules from `dist/` to the following paths, renaming them as shown:

| Release file | Console destination |
|---|---|
| `pm_shellui_keeper.prx` | `/data/GoldHEN/plugins/pm_pkg23_keeper.prx` |
| `pm_shellui_marker.prx` | `/data/GoldHEN/plugins/pm_pkg23_marker.prx` |

Create `/data/GoldHEN/plugins/` if needed. Both modules must come from the same release as the starter. The `pkg23` portion of the destination names is an internal build identifier required by this loader.

## Run the starter

With GoldHEN active and the payload loader available on port **9090**, send `dist/pm_shellui_start.bin` to the console.

The starter loads the keeper, which stages and loads the menu module into ShellUI.

## Open the menu

Press **L3 + R3** to open or close the overlay.

- **D-pad Up / Down** — move selection.
- **Cross** — select.
- **Circle** — go back, or close the menu at the top level.

The overlay needs to be loaded again after restarting the console.

## Troubleshooting

- Confirm GoldHEN is active and both module paths match the table above.
- Check `/data/GoldHEN/pm_overlay_pkg23_status.txt` and console logs for startup status.
- A payload that never reached the console may require rearming the payload loader before resending.
- Once the starter records an injection attempt, another attempt may be blocked until a fresh ShellCore session. Restart the console and reactivate GoldHEN before retrying in that case.
- Avoid empty TCP connection probes to the payload loader; a connection can consume its available load attempt.

Back up plugin configuration before testing. A failed overlay load can freeze the dashboard and require a reboot.
