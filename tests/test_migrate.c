// test_migrate.c - app_migrate on temp folders: the four cases of docs/RENAME_PLAN.md, files kept byte for byte.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_migrate.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void put(const char *p, const char *text)
{
	FILE *f = fopen(p, "w");
	fputs(text, f);
	fclose(f);
}

static int same(const char *p, const char *text)
{
	char buf[256] = "";
	FILE *f = fopen(p, "r");
	if (!f)
		return 0;
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	buf[n] = 0;
	fclose(f);
	return strcmp(buf, text) == 0;
}

static int is_dir(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

int main(void)
{
	char root[] = "/tmp/wbx_migrate_XXXXXX", old_dir[128], new_dir[128], p[200];
	if (!mkdtemp(root))
		return 1;
	snprintf(old_dir, sizeof(old_dir), "%s/walkerbox", root);
	snprintf(new_dir, sizeof(new_dir), "%s/pocketpkmn", root);

	// 1. neither: first start, nothing made
	CHECK(app_migrate(old_dir, new_dir) == APP_MIGRATE_NONE, "neither -> NONE");
	CHECK(!is_dir(old_dir) && !is_dir(new_dir), "neither: no folder made");

	// 2. only the old one: moved, every file and subfolder the same
	mkdir(old_dir, 0777);
	snprintf(p, sizeof(p), "%s/sprites", old_dir);
	mkdir(p, 0777);
	snprintf(p, sizeof(p), "%s/trainer.txt", old_dir);
	put(p, "name=Sam\n");
	snprintf(p, sizeof(p), "%s/bank.wbk", old_dir);
	put(p, "WBXBANK1 data");
	snprintf(p, sizeof(p), "%s/sprites/001.bin", old_dir);
	put(p, "sprite");
	CHECK(app_migrate(old_dir, new_dir) == APP_MIGRATE_MOVED, "only old -> MOVED");
	CHECK(!is_dir(old_dir) && is_dir(new_dir), "moved: old gone, new there");
	snprintf(p, sizeof(p), "%s/trainer.txt", new_dir);
	CHECK(same(p, "name=Sam\n"), "trainer.txt kept");
	snprintf(p, sizeof(p), "%s/bank.wbk", new_dir);
	CHECK(same(p, "WBXBANK1 data"), "bank.wbk kept");
	snprintf(p, sizeof(p), "%s/sprites/001.bin", new_dir);
	CHECK(same(p, "sprite"), "sprites/ kept");

	// 3. only the new one: nothing to do
	CHECK(app_migrate(old_dir, new_dir) == APP_MIGRATE_NONE, "only new -> NONE");

	// 4. both: new used, old left alone
	mkdir(old_dir, 0777);
	snprintf(p, sizeof(p), "%s/trainer.txt", old_dir);
	put(p, "name=Other\n");
	CHECK(app_migrate(old_dir, new_dir) == APP_MIGRATE_BOTH, "both -> BOTH");
	CHECK(same(p, "name=Other\n"), "both: old folder untouched");
	snprintf(p, sizeof(p), "%s/trainer.txt", new_dir);
	CHECK(same(p, "name=Sam\n"), "both: new folder untouched");

	// 5. rename fails (new_dir's parent missing): FAILED, old untouched
	char bad[200];
	snprintf(bad, sizeof(bad), "%s/missing/pocketpkmn", root);
	CHECK(app_migrate(old_dir, bad) == APP_MIGRATE_FAILED, "rename fails -> FAILED");
	snprintf(p, sizeof(p), "%s/trainer.txt", old_dir);
	CHECK(same(p, "name=Other\n"), "failed: old folder untouched");

	char cmd[256];
	snprintf(cmd, sizeof(cmd), "rm -rf %s", root);
	if (system(cmd) != 0)
		printf("note: could not remove %s\n", root);
	printf(fails ? "MIGRATION TESTS: %d FAILED\n" : "migration tests passed (%d failures)\n", fails);
	return fails != 0;
}
