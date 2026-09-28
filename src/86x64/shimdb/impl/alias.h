/*
 * alias.h — the Alias Manager record engine in shimdb/impl/alias.c, shared with
 * libabiconv's i386 glue (carbon_alias_shim.c). Records are byte buffers here;
 * only the AliasHandle that carries them differs per ABI. Every function
 * returns a classic OSErr and defines its out-params before any failure.
 */
#ifndef SHIMDB_ALIAS_H
#define SHIMDB_ALIAS_H

#include <stddef.h>
#include <stdint.h>

/* Mint a record for an existing path (malloc'd into *out; caller frees). */
int am_record_from_path(const char *path, int minimal, int with_bookmark,
                        uint8_t **out, uint32_t *outlen);
/* NewAliasMinimalFromFullPath's raw (HFS colon or POSIX) path -> existing POSIX path. */
int am_path_from_fullpath(const char *fp, int plen, char *path, size_t pathsz);
/* ResolveAlias on record bytes: FSSpec target, wasChanged. */
int am_resolve_bytes(const uint8_t *rec, uint32_t len, uint8_t *target, uint8_t *changed);
/* MatchAlias on record bytes: at most one candidate (FSSpec, or FSRef if fsref_list). */
int am_match_bytes(const uint8_t *rec, uint32_t len, int16_t *countv, uint8_t *list,
                   uint8_t *needsUpdate, int fsref_list);
/* ResolveAliasFile: resolve an FSSpec in place (Finder alias files and symlinks). */
int16_t ResolveAliasFile(uint8_t *spec, uint8_t chains, uint8_t *targetIsFolder,
                         uint8_t *wasAliased);

#endif
