// app_migrate.h - moves the old app folder to the new one at start-up (docs/RENAME_PLAN.md). Portable C (stdio,
// sys/stat) so the PC tests can run it on temp folders. Never call during IR.
#ifndef APP_MIGRATE_H
#define APP_MIGRATE_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
	APP_MIGRATE_NONE,   // nothing to do: only the new folder, or neither (first start)
	APP_MIGRATE_MOVED,  // the old folder was renamed to the new one (no file copied or deleted)
	APP_MIGRATE_BOTH,   // both exist: the new one is used, the old one is left alone
	APP_MIGRATE_FAILED, // the rename failed: nothing changed, the app must stop
};

int app_migrate(const char *old_dir, const char *new_dir);

#ifdef __cplusplus
}
#endif

#endif
