/*
 * cfxml_shim.c — i386<->x86_64 struct-relayout shim for CFXMLNodeCreate /
 * CFXMLNodeGetInfoPtr's discriminated `const void *additionalInfoPtr`.
 *
 * THE BUG (Civ IV Steam, "Launch in Window" startup XML/mod-config parse):
 *   CFXMLNodeRef CFXMLNodeCreate(CFAllocatorRef, CFXMLNodeTypeCode xmlType,
 *                                CFStringRef, const void *additionalInfoPtr,
 *                                CFIndex version);
 * `additionalInfoPtr` is a DISCRIMINATED UNION selected by xmlType — for
 * kCFXMLNodeTypeElement it is a `CFXMLElementInfo *`, for Document a
 * `CFXMLDocumentInfo *`, etc. (see CFXMLNode.h's dataTypeCode table). Every one
 * of these info structs contains POINTER and CFIndex/CFStringEncoding fields, so
 * their LAYOUT DIFFERS between the ABIs:
 *
 *   CFXMLElementInfo        i386 (ILP32)                x86_64 (LP64)
 *     CFDictionaryRef attributes      @0  (4B)                @0  (8B)
 *     CFArrayRef      attributeOrder  @4  (4B)                @8  (8B)
 *     Boolean         isEmpty         @8  (1B)                @16 (1B)
 *     sizeof                          12                      24
 *
 * abigen cannot see through the `const void *` (its pointee is opaque `void`),
 * so the generated ___CFXMLNodeCreate shim forwards the i386 struct pointer
 * VERBATIM (routing the bare void* through x64_objc_unwrap, which passes a
 * genuine low pointer straight through). Native x86_64 CoreFoundation then reads
 * that 12-byte i386 record as a 24-byte x86_64 record: `attributes` fuses the
 * i386 attributes+attributeOrder 4-byte handles into one 8-byte value
 * (e.g. 0x08000100_00000008 — the crash's fault address), `attributeOrder` and
 * `isEmpty` read past the end of the i386 record, and even the field VALUES are
 * i386 proxy handles CF derefs as real CF objects. Result:
 *   EXC_BAD_ACCESS (SIGSEGV) at 0x0800010000000008 inside CFXMLNodeCreate,
 *   forwarded from libabiconv __CFXMLNodeCreate.l1.
 *
 * THE FIX (this shim): hand-marshal the info struct per xmlType. Reading the
 * i386 record field-by-field (4-byte fields at the i386 offsets, each pointer
 * field resolved handle->real via _86x64_unwrap_obj_arg), we build the native
 * x86_64 record on the stack and hand THAT to real CFXMLNodeCreate. CF copies
 * the info into the node during Create (it does not retain the caller's pointer),
 * so a stack local suffices. CFXMLNodeGetInfoPtr does the mirror: real CF returns
 * a native x86_64 record; we relayout it back into an i386 record in a low-4GB
 * buffer whose 32-bit address the i386 caller can hold, resolving each native
 * pointer field real->handle via x64_objc_wrap.
 *
 * GENERIC: triggers on the CFXML info-struct contract (xmlType selector + the
 * SDK-fixed layouts), not on any app. Any legacy i386 client of CFXMLNodeCreate/
 * GetInfoPtr with a pointer-bearing node type (Element/Document/ProcessingInstr/
 * Entity/EntityReference/DocumentType/Notation/ElementTypeDecl/AttributeListDecl)
 * is served; the info-less types (Comment/Text/CDATA/Whitespace/Attribute/
 * DocumentFragment, additionalInfoPtr == NULL) pass through unchanged.
 *
 * Reached via ___CFXMLNodeCreate / ___CFXMLNodeGetInfoPtr MTSHIM trampolines
 * (cfxml_tramp.asm), the same export names abigen used, so static-interpose /
 * resync route already-deployed binaries here. Entry ABI (MTSHIM): a =
 * &i386_args[0], 4-byte cdecl stack slots; the pointer return flows back in eax
 * (4 bytes) — a low-4GB CF handle.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>

/* proxy-arena bridge (objc_shim.c): CF handle <-> real CF ref. */
extern uint64_t _86x64_unwrap_obj_arg(uint32_t a); /* i386 handle -> real 64-bit CF ref */
extern uint32_t x64_objc_wrap(uint64_t real);      /* real 64-bit CF ref -> i386 handle */

/* CFXMLNodeTypeCode values (CFXMLNode.h). */
enum {
   kNodeDocument = 1, kNodeElement = 2, kNodeAttribute = 3,
   kNodeProcessingInstruction = 4, kNodeComment = 5, kNodeText = 6,
   kNodeCDATASection = 7, kNodeDocumentFragment = 8, kNodeEntity = 9,
   kNodeEntityReference = 10, kNodeDocumentType = 11, kNodeWhitespace = 12,
   kNodeNotation = 13, kNodeElementTypeDeclaration = 14,
   kNodeAttributeListDeclaration = 15
};

/* ---- i386-record readers -------------------------------------------------- */
/* i386 pointer/CFTypeRef field: a 4-byte proxy handle -> real 64-bit CF ref. */
static inline const void *i386_ref(const uint32_t *rec, unsigned off_bytes) {
   uint32_t h = *(const uint32_t *)((const char *)rec + off_bytes);
   return (const void *)(uintptr_t)_86x64_unwrap_obj_arg(h);
}
/* i386 4-byte scalar (CFIndex / CFStringEncoding / enum) — sign/zero as needed
 * by the field's native type at the call site. */
static inline uint32_t i386_u32(const uint32_t *rec, unsigned off_bytes) {
   return *(const uint32_t *)((const char *)rec + off_bytes);
}

/* ---- native x86_64 info records (LP64 layout; sizeof/offsets = the SDK) ----
 * We reproduce ONLY the fields real CF reads; the trailing Boolean/_reserved
 * padding is zeroed by the {0} initializer. */
typedef struct { const void *sysID; const void *pubID; } ExternalID64; /* CFXMLExternalID */

/* Relayout an i386 additionalInfoPtr -> a native x86_64 record on *out64 (a
 * buffer big enough for the largest info struct). Returns a pointer to the
 * native record, or NULL when the node type carries no info (pass NULL through).
 * `i386info` is the i386-laid-out record (may be NULL). */
static const void *relayout_info_to_native(long xmlType, const uint32_t *i386info,
                                           void *out64) {
   if (!i386info) { return NULL; }
   switch (xmlType) {
   case kNodeElement: {
      /* i386: attributes@0, attributeOrder@4, isEmpty@8 */
      struct { const void *attrs; const void *order; unsigned char isEmpty; char _r[7]; } *o = out64;
      memset(o, 0, sizeof(*o));
      o->attrs   = i386_ref(i386info, 0);
      o->order   = i386_ref(i386info, 4);
      o->isEmpty = *((const unsigned char *)i386info + 8);
      return o;
   }
   case kNodeProcessingInstruction: {
      /* i386: dataString@0 */
      struct { const void *dataString; } *o = out64;
      o->dataString = i386_ref(i386info, 0);
      return o;
   }
   case kNodeDocument: {
      /* i386: sourceURL@0, encoding@4 */
      struct { const void *sourceURL; uint32_t encoding; char _r[4]; } *o = out64;
      memset(o, 0, sizeof(*o));
      o->sourceURL = i386_ref(i386info, 0);
      o->encoding  = i386_u32(i386info, 4);
      return o;
   }
   case kNodeDocumentType:   /* {CFXMLExternalID externalID;} */
   case kNodeNotation: {     /* {CFXMLExternalID externalID;} */
      /* i386 CFXMLExternalID: systemID@0, publicID@4 */
      ExternalID64 *o = out64;
      o->sysID = i386_ref(i386info, 0);
      o->pubID = i386_ref(i386info, 4);
      return o;
   }
   case kNodeElementTypeDeclaration: {
      /* i386: contentDescription@0 */
      struct { const void *contentDescription; } *o = out64;
      o->contentDescription = i386_ref(i386info, 0);
      return o;
   }
   case kNodeAttributeListDeclaration: {
      /* i386: numberOfAttributes(CFIndex)@0, attributes(ptr)@4 — the attributes
       * pointer targets an ARRAY of CFXMLAttributeDeclarationInfo whose own
       * layout differs; CF reads numberOfAttributes of them. Relayout the array
       * too (each i386 entry = 3 pointers @0/4/8, 12B; native = 3 ptrs, 24B). */
      struct AttrDecl64 { const void *name; const void *typeStr; const void *dflt; };
      struct { CFIndex n; struct AttrDecl64 *attrs; } *o = out64;
      CFIndex n = (CFIndex)(int32_t)i386_u32(i386info, 0);
      uint32_t arr_h = i386_u32(i386info, 4);
      o->n = n;
      o->attrs = NULL;
      if (n > 0 && n < (1 << 20) && arr_h) {
         const char *ia = (const char *)(uintptr_t)_86x64_unwrap_obj_arg(arr_h);
         struct AttrDecl64 *na = calloc((size_t)n, sizeof(*na));
         if (ia && na) {
            for (CFIndex i = 0; i < n; i++) {
               const uint32_t *e = (const uint32_t *)(ia + (size_t)i * 12);
               na[i].name    = i386_ref(e, 0);
               na[i].typeStr = i386_ref(e, 4);
               na[i].dflt    = i386_ref(e, 8);
            }
         }
         o->attrs = na;   /* leaked intentionally: CF copies during Create */
      }
      return o;
   }
   case kNodeEntity: {
      /* i386: entityType(CFIndex)@0, replacementText@4, entityID(ExternalID: 2
       * ptrs)@8/@12, notationName@16 */
      struct { CFIndex entityType; const void *replacementText;
               const void *sysID; const void *pubID;
               const void *notationName; } *o = out64;
      o->entityType     = (CFIndex)(int32_t)i386_u32(i386info, 0);
      o->replacementText= i386_ref(i386info, 4);
      o->sysID          = i386_ref(i386info, 8);
      o->pubID          = i386_ref(i386info, 12);
      o->notationName   = i386_ref(i386info, 16);
      return o;
   }
   case kNodeEntityReference: {
      /* i386: entityType(CFIndex)@0 */
      struct { CFIndex entityType; } *o = out64;
      o->entityType = (CFIndex)(int32_t)i386_u32(i386info, 0);
      return o;
   }
   default:
      /* Info-less node types (Comment/Text/CDATA/Whitespace/Attribute/
       * DocumentFragment) or an unknown future type: additionalInfoPtr is
       * documented NULL. If a caller still passed something, forward it raw
       * rather than corrupt — the safest fallback for surface we don't model. */
      return i386info;
   }
}

/* Scratch big enough for the largest native info record used above
 * (CFXMLEntityInfo = 5 pointers = 40B; round up generously). */
union info_scratch { char _[64]; long double _a; void *_p; };

/* ---- ___CFXMLNodeCreate ---------------------------------------------------- *
 * i386 cdecl args (4-byte slots): a[0]=alloc, a[1]=xmlType(CFIndex),
 * a[2]=dataString, a[3]=additionalInfoPtr, a[4]=version(CFIndex).
 * Returns a low-4GB CF handle for the new node in eax. */
uint32_t shim_CFXMLNodeCreate(uint32_t *a) {
   CFAllocatorRef alloc = (CFAllocatorRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   long           xmlType = (long)(int32_t)a[1];
   CFStringRef    dataStr = (CFStringRef)(uintptr_t)_86x64_unwrap_obj_arg(a[2]);
   const uint32_t *i386info = (const uint32_t *)(uintptr_t)
                              _86x64_unwrap_obj_arg(a[3]);
   long           version = (long)(int32_t)a[4];

   union info_scratch scratch;
   const void *native_info =
      relayout_info_to_native(xmlType, i386info, &scratch);

   CFXMLNodeRef node =
      CFXMLNodeCreate(alloc, (CFXMLNodeTypeCode)xmlType, dataStr,
                      native_info, (CFIndex)version);

   uint32_t h = node ? x64_objc_wrap((uint64_t)(uintptr_t)node) : 0;
   return h;
}

/* ---- ___CFXMLNodeGetInfoPtr ------------------------------------------------ *
 * Mirror direction: real CF returns a NATIVE x86_64 info record; the i386 caller
 * will read it with i386 layout. Relayout into a low-4GB i386 record and return
 * its 32-bit address. The record must outlive the call, and since a node's info
 * is stable for the node's lifetime we cache one i386 buffer per node handle
 * (small LRU) so repeated GetInfoPtr on the same node returns a stable pointer,
 * matching CF's own contract (the returned pointer is owned by the node).
 * i386 cdecl args: a[0]=node. Returns the i386 info pointer (4B handle-of-buffer
 * or the low address of the i386 record) in eax; 0 for info-less nodes. */

/* Small fixed cache of {node -> i386 record buffer}. Buffers live on the
 * low-4GB heap (malloc_shim) so their address fits an i386 pointer. */
#define CFXML_INFO_CACHE 16
static struct { CFXMLNodeRef node; void *i386buf; } g_info_cache[CFXML_INFO_CACHE];
static unsigned g_info_next;

static uint32_t node_to_i386_info(CFXMLNodeRef node) {
   long xmlType = (long)CFXMLNodeGetTypeCode(node);
   const void *ni = CFXMLNodeGetInfoPtr(node);
   if (!ni) { return 0; }

   /* reuse a cached buffer for this node if present */
   void *buf = NULL;
   for (unsigned i = 0; i < CFXML_INFO_CACHE; i++)
      if (g_info_cache[i].node == node) { buf = g_info_cache[i].i386buf; break; }
   if (!buf) {
      buf = malloc(64);   /* low-4GB heap; big enough for any i386 info record */
      if (!buf) { return 0; }
      memset(buf, 0, 64);
      unsigned slot = g_info_next++ % CFXML_INFO_CACHE;
      /* free the buffer we evict (its node's pointer contract ends) */
      if (g_info_cache[slot].i386buf) { free(g_info_cache[slot].i386buf); }
      g_info_cache[slot].node = node;
      g_info_cache[slot].i386buf = buf;
   }
   uint32_t *o = (uint32_t *)buf;

   /* native pointer field -> i386 handle */
#define REF32(dst_off, native_ptr) \
      (*(uint32_t *)((char *)o + (dst_off)) = \
         (native_ptr) ? x64_objc_wrap((uint64_t)(uintptr_t)(native_ptr)) : 0)
#define U32(dst_off, val) (*(uint32_t *)((char *)o + (dst_off)) = (uint32_t)(val))

   switch (xmlType) {
   case kNodeElement: {
      const struct { const void *attrs; const void *order;
                     unsigned char isEmpty; } *n = ni;
      REF32(0, n->attrs); REF32(4, n->order);
      *((unsigned char *)o + 8) = n->isEmpty;
      break;
   }
   case kNodeProcessingInstruction: {
      const struct { const void *dataString; } *n = ni;
      REF32(0, n->dataString);
      break;
   }
   case kNodeDocument: {
      const struct { const void *sourceURL; uint32_t encoding; } *n = ni;
      REF32(0, n->sourceURL); U32(4, n->encoding);
      break;
   }
   case kNodeDocumentType:
   case kNodeNotation: {
      const ExternalID64 *n = ni;
      REF32(0, n->sysID); REF32(4, n->pubID);
      break;
   }
   case kNodeElementTypeDeclaration: {
      const struct { const void *contentDescription; } *n = ni;
      REF32(0, n->contentDescription);
      break;
   }
   case kNodeEntity: {
      const struct { CFIndex entityType; const void *replacementText;
                     const void *sysID; const void *pubID;
                     const void *notationName; } *n = ni;
      U32(0, n->entityType); REF32(4, n->replacementText);
      REF32(8, n->sysID); REF32(12, n->pubID); REF32(16, n->notationName);
      break;
   }
   case kNodeEntityReference: {
      const struct { CFIndex entityType; } *n = ni;
      U32(0, n->entityType);
      break;
   }
   default:
      /* AttributeListDeclaration's info holds a pointer to an array we would
       * also need to relayout with a stable lifetime; and info-less types
       * return 0 above. For unmodeled types return 0 rather than a raw native
       * pointer the i386 caller would truncate/misread. */
      return 0;
   }
#undef REF32
#undef U32
   return (uint32_t)(uintptr_t)buf;
}

uint32_t shim_CFXMLNodeGetInfoPtr(uint32_t *a) {
   CFXMLNodeRef node = (CFXMLNodeRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   if (!node) { return 0; }
   uint32_t r = node_to_i386_info(node);
   return r;
}
