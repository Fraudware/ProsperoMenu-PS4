#pragma once
#include <stdint.h>
#include <GoldHEN.h>
#include <stddef.h>
#include <string.h>
#include "process_records.h"
#define PROBE_VERSION "0.1.0"
#define PROBE_BUILD "pm-overlay-pkg23"
#define PROBE_PATH "/Temp/pm_overlay_pkg23/pm_shellui_marker.prx"
#define PROBE_STAGE "pm_overlay_pkg23"
#define PROBE_LOCK "/data/GoldHEN/pm_overlay_pkg23.lock"
#define PROBE_STATUS "/data/GoldHEN/pm_overlay_pkg23_status.txt"
#define PROBE_HOOK_SOURCE "/data/GoldHEN/plugins/pm_pkg23_marker.prx"
#define PROBE_EXPORT __attribute__((visibility("default")))
void probe_log(const char *, const char *, ...) __attribute__((format(printf, 2, 3)));
void probe_notify(const char *);
int probe_process_report(const char *, struct proc_info *);
int probe_find_process(const char *, struct process_identity *);
int probe_same_process(struct process_identity, struct process_identity);
int probe_lock_file(const char *);
int probe_write_text(const char *, const char *);
