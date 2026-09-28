/*
 * fsspec.h — the FSSpec <-> POSIX resolver owned by shimdb/impl/fsspec.c,
 * exposed deliberately to the other classic-File-Manager-adjacent code (the
 * Alias Manager, and libabiconv's i386 glue).
 *
 * WHY A HEADER AND NOT A COPY. fsspec.c is the ONE place that knows
 * how a classic {vRefNum, dirID, HFS name} triple maps onto a modern path:
 * FSGetVolumeInfo for the volume root, the /.vol/<st_dev>/<CNID> volfs namespace
 * for the directory ID, the '/'<->':' HFS/POSIX leaf swap, and the dirID table
 * that cfs_find_folder mints into because modern CarbonCore's FindFolder hands
 * back tokens rather than catalog node ids. Every one of those is a hard-won
 * behaviour with a comment explaining it; a second copy in another shim would
 * drift. carbon_alias_shim.c needs exactly this mapping (an AliasHandle is
 * created from, and resolved to, an FSSpec), so it takes it from here.
 *
 * Names are `cfs_`-prefixed: they are exported from every shimgen shim that
 * links fsspec.c, and the prefix says which module owns them.
 */
#ifndef SHIMDB_FSSPEC_H
#define SHIMDB_FSSPEC_H

#include <stdint.h>
#include <stddef.h>

/* FSSpec is packed: SInt16 vRefNum @0, SInt32 parID @2, Str63 name @6. */
#define CFS_FSSPEC_SIZE  70
#define CFS_FSSPEC_NAME  6
#define CFS_FSREF_SIZE   80

/* The '/' <-> ':' exchange that turns a classic HFS leaf name into the POSIX
 * name of the same file. `pname` is a Pascal string (length byte first). */
void cfs_hfs_leaf_to_posix(const uint8_t *pname, char *out, size_t outsz);

/* The same exchange the other way: a POSIX leaf name -> a Pascal HFS name of at
 * most 63 characters, written into `pout` (>= 64 bytes). Involutive with the
 * above; kept as its own entry point so callers do not have to know that. */
void cfs_posix_leaf_to_hfs(const char *leaf, uint8_t *pout);

/* Directory (vRefNum, dirID) -> POSIX path. Returns 1 on success. */
int cfs_fsdir_to_path(int16_t vRefNum, int32_t dirID, char *out, size_t outsz);

/* Full FSSpec -> POSIX path. `exists` (optional) reports whether the resulting
 * path is actually present — "resolvable" and "exists" are distinct answers,
 * because a spec for a not-yet-created file is still fully formed. Returns 1
 * when the spec could be resolved at all. */
int cfs_fsspec_to_path(const uint8_t *spec, char *out, size_t outsz, int *exists);

/* POSIX path -> FSSpec (70 bytes, always fully written before any failure
 * return). Returns 1 on success. The parent directory is recorded in the dirID
 * table on the way out, so the resulting spec round-trips back through
 * cfs_fsspec_to_path even on a volume where the /.vol namespace is unavailable. */
int cfs_path_to_fsspec(const char *path, uint8_t *spec);

/* Remember that (vRefNum, dirID) names `path`, so the consumer half can resolve
 * a dirID we minted rather than one the file system assigned. */
void cfs_dirtab_remember(int16_t vref, int32_t id, const char *path);

/* FindFolder with a dirID the rest of this module can resolve (modern CarbonCore's
 * own FindFolder returns internal tokens). Out-params are defined first. OSErr. */
int32_t cfs_find_folder(int16_t vRefNum, uint32_t folderType, uint8_t createFolder,
                        int16_t *foundVRefNum, int32_t *foundDirID);

#endif /* ABICONV_CARBON_FSSPEC_H */
