/* Safe enumeration of the loaded images. See dyld_image_list.c for WHY the dyld
 * indexed APIs must not be used for this: querying an entry whose image is still
 * mid-load ABORTS the process from inside dyld's own assert.
 *
 * ⚠ The index here is an index into dyld's infoArray, which is NOT dyld's Loader
 * index order and includes images the Loader list does not. Use it only to iterate
 * this set; never hand it to a dyld API.
 *
 * Compiled into BOTH libabiconv and libwrapper: the wrapper walks images too
 * (fixup_translated_dylib_slots), from inside a dlopen, so it has the same
 * exposure and needs the same cure.
 */
#ifndef X64_DYLD_IMAGE_LIST_H
#define X64_DYLD_IMAGE_LIST_H

#include <stdint.h>
#include <mach-o/loader.h>

/* 1 (the DEFAULT) when the accessors chain to dyld's indexed APIs. The infoArray
 * path is opt-in via M64_IMGLIST_INFOARRAY=1 and is NOT a correct fix yet -- it
 * redefines "loaded" from ready to merely mapped. See dyld_image_list.c. */
int x64_img_legacy_mode(void);
/* Number of entries; 0 if the list is unavailable. */
uint32_t x64_img_count(void);
/* Validated header for entry i, or NULL (missing, or read mid-update). */
const struct mach_header *x64_img_header(uint32_t i);
/* Path for entry i, or NULL. */
const char *x64_img_path(uint32_t i);
/* Last path component (no allocation; points into `path`). NULL-safe. */
const char *x64_img_leaf(const char *path);
/* Slide, computed from the image's own mapped __TEXT. Safe for in-flight images. */
intptr_t x64_img_slide(const struct mach_header *mh);
/* First image whose path's last component equals `leaf`. */
const struct mach_header *x64_img_find_leaf(const char *leaf, intptr_t *slide_out);
/* Path of the image with this exact header, or NULL. */
const char *x64_img_path_for_header(const struct mach_header *mh);

/* 1 iff the image at this header speaks the i386 convention: every binary the
 * pipeline emits loads libabiconv; native dylibs (and libabiconv itself) do not.
 * THE structural "is translated" test -- keyed on the dependency, never on where
 * dyld mapped the image (a native dylib can land below 4GB). */
int x64_img_is_translated(const void *mh);

#endif /* X64_DYLD_IMAGE_LIST_H */
