/*
 * carbon_fsspec.h — the FSSpec <-> POSIX resolver owned by carbon_fsspec_shim.c,
 * shared with the Alias Manager (carbon_alias_shim.c), which creates an
 * AliasHandle from an FSSpec and resolves one back to an FSSpec.
 *
 * carbon_fsspec_shim.c is the ONE place that knows how a classic {vRefNum, dirID,
 * HFS name} triple maps onto a modern path (volume root via FSGetVolumeInfo, the
 * /.vol/<st_dev>/<CNID> volfs namespace, the '/'<->':' leaf swap, the dirID table
 * FindFolder mints into). A second copy would drift, so the alias code takes it
 * from here. Hidden: internal to libabiconv, never exported.
 */
#ifndef ABICONV_CARBON_FSSPEC_H
#define ABICONV_CARBON_FSSPEC_H

#include <stdint.h>
#include <stddef.h>

/* FSSpec is packed: SInt16 vRefNum @0, SInt32 parID @2, Str63 name @6. */
#define CFS_FSSPEC_SIZE  70
#define CFS_FSSPEC_NAME  6
#define CFS_FSREF_SIZE   80

#define CFS_HIDDEN __attribute__((visibility("hidden")))

/* Full FSSpec -> POSIX path. `exists` (optional) reports whether the resulting
 * path is actually present — "resolvable" and "exists" are distinct answers,
 * because a spec for a not-yet-created file is still fully formed. Returns 1
 * when the spec could be resolved at all. */
CFS_HIDDEN int cfs_fsspec_to_path(const uint8_t *spec, char *out, size_t outsz, int *exists);

/* POSIX path -> FSSpec (70 bytes, always fully written before any failure
 * return). Returns 1 on success. The parent directory is recorded in the dirID
 * table on the way out, so the resulting spec round-trips back through
 * cfs_fsspec_to_path even on a volume where the /.vol namespace is unavailable. */
CFS_HIDDEN int cfs_path_to_fsspec(const char *path, uint8_t *spec);

/* POSIX path -> classic dirID: the real catalog node id (inode) where it fits the
 * classic SInt32, else a minted token; recorded either way so cfs_fsspec_to_path
 * resolves it back. Modern CarbonCore's own dirIDs (FindFolder, FSGetCatalogInfo
 * parentDirID/nodeID) are small sequential tokens no surviving API consumes, so
 * every producer hands out these instead. 0 if the path cannot be stat'ed. */
CFS_HIDDEN int32_t cfs_dirid_for_path(int16_t vRefNum, const char *path);

#endif /* ABICONV_CARBON_FSSPEC_H */
