// app_paths.h - the app's name and its folder on the SD card (v0.33: WalkerBox -> PocketPKMN, docs/RENAME_PLAN.md).
// Plain macros, also used by the PC tests.
#ifndef APP_PATHS_H
#define APP_PATHS_H

#define APP_NAME    "PocketPKMN"
#define APP_DIR     "sdmc:/3ds/pocketpkmn"
#define APP_OLD_DIR "sdmc:/3ds/walkerbox" // before v0.33; moved to APP_DIR at start-up (app_migrate)

#endif
