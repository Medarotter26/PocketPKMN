// app_migrate.c - see include/app_migrate.h.
#include <stdio.h>
#include <sys/stat.h>

#include "app_migrate.h"

static int is_dir(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

int app_migrate(const char *old_dir, const char *new_dir)
{
	int has_old = is_dir(old_dir), has_new = is_dir(new_dir);
	if (has_new)
		return has_old ? APP_MIGRATE_BOTH : APP_MIGRATE_NONE;
	if (!has_old)
		return APP_MIGRATE_NONE;
	// One folder rename on the same card: the folder entry changes, every file stays where it is.
	if (rename(old_dir, new_dir) != 0)
		return APP_MIGRATE_FAILED;
	return is_dir(new_dir) && !is_dir(old_dir) ? APP_MIGRATE_MOVED : APP_MIGRATE_FAILED;
}
