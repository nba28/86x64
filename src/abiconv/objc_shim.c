/*
 * objc_shim.c — legacy-i386-ObjC → modern-x86_64-ObjC bridge runtime.
 *
 * The translated i386 program calls `objc_msgSend` with the i386 cdecl ABI
 * (self, _cmd, args all as 4-byte stack slots) and holds every `id` as a
 * 32-bit value. The real x86_64 objc_msgSend wants self/_cmd/args in
 * registers, and real objc objects live at 64-bit addresses that do not fit
 * in the program's 32-bit slots.
 *
 * This file provides the C side of the bridge:
 *   - a proxy table that maps real 64-bit object pointers <-> 32-bit handles
 *     (handles are addresses of cells in a low-4GB arena, so the translated
 *     code can store them in its 32-bit slots);
 *   - objc_bridge_prep(), which the __objc_msgSend asm trampoline calls to
 *     resolve self/_cmd and lay out the x86_64 register arguments.
 *
 * The asm trampoline (objc_msgSend.asm) handles the i386 stack ABI and the
 * actual register marshalling / tail call into the real objc_msgSend.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <objc/runtime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <pthread.h>

/* Low-4GB search window — same range the wrapper/malloc shim use. */
#define LOW_REGION_BASE 0x080000000UL
#define LOW_REGION_END  0x0F0000000UL

/*
 * Proxy arena: a flat low-4GB array of 64-bit reals. A "handle" is the
 * 32-bit address of one slot. unwrap(handle) = *slot. The arena must live
 * below 4GB so handles fit in the translated program's 32-bit id slots.
 */
#define ARENA_SLOTS (1u << 20)            /* 1M proxied objects */
#define MAP_CAP     (1u << 21)
struct map_ent { uint64_t real; uint32_t handle; };

/*
 * Cross-copy shared state. The translated bundle co-locates one
 * libabiconv.dylib per framework, and dyld loads each PATH as a SEPARATE image
 * with its own __DATA. Plain module statics would therefore give every copy
 * its OWN proxy arena + dedup map, so a 32-bit handle minted by one copy would
 * be meaningless to another — and handles cross framework boundaries on nearly
 * every message. Worse, each copy's arena_init would re-run, and the map
 * allocation routes through libabiconv's own malloc shim (it defines calloc),
 * which a later copy's shim region can't satisfy ("proxy map alloc failed").
 *
 * Fix: keep ONE arena/map per process. The first copy to need them creates the
 * arena (low-4GB, so handles fit in 32 bits), the dedup map, and this control
 * block, then publishes the block's address via the process environment (one
 * `environ` shared by all copies). Every other copy reads it back and adopts
 * the same pointers — all copies share a single address space, so the pointers
 * are directly usable. The map is allocated with mach_vm_allocate (not the
 * shim's calloc) so it never depends on a per-copy heap.
 */
struct objc_shared_ctrl {
   uint64_t        magic;        /* OBJC_CTRL_MAGIC once fully initialized */
   uint64_t        arena;        /* low-4GB slot array base */
   uint64_t        arena_base;
   uint64_t        arena_end;
   uint32_t        arena_used;   /* the one shared allocation cursor */
   struct map_ent *map;          /* real->handle dedup table */
   /* i386 instance-shadow arena (legacy class registration). Shared across the
    * N libabiconv copies so resolve_self in ANY copy recognizes a shadow that
    * another copy created (single address space). shadow_cur is the bump
    * cursor; the registering copy is the only one that allocates. */
   uint64_t        shadow_base;
   uint64_t        shadow_cur;
   uint64_t        shadow_end;
   /* Per-thread super-dispatch hints (legacy [super sel] -> reverse bridge).
    * MUST live in the shared ctrl, not a per-copy __thread var: the forward
    * super bridge (objc_bridge_prep_super) runs in the caller's libabiconv
    * copy, but reverse_prep always runs in the REGISTERING copy — different
    * copies, different statics. Keyed by thread id; (recv,sel) matched on
    * consume so a hash collision from another thread is a safe miss. */
   struct { uint64_t tid; uint64_t recv; uint64_t sel; uint64_t lookup;
            int valid; } super_hints[64];
};
#define SUPER_HINT_SLOTS 64
#define OBJC_CTRL_MAGIC 0x3836583634415243ULL  /* "86X64ARC" */
#define OBJC_CTRL_ENV   "ABICONV_OBJC_CTRL"

static struct objc_shared_ctrl *g_ctrl = NULL;
/* hot-path caches; identical across copies once attached to the shared ctrl */
static uint64_t       *g_arena      = NULL;
static struct map_ent *g_map        = NULL;
static uintptr_t       g_arena_base = 0;
static uintptr_t       g_arena_end  = 0;

static void arena_attach(struct objc_shared_ctrl *c) {
   g_ctrl       = c;
   g_arena      = (uint64_t *)(uintptr_t)c->arena;
   g_arena_base = (uintptr_t)c->arena_base;
   g_arena_end  = (uintptr_t)c->arena_end;
   g_map        = c->map;
}

static void arena_init(void) {
   if (g_ctrl) { return; }

   /* Adopt an existing process-wide arena published by an earlier copy. */
   const char *e = getenv(OBJC_CTRL_ENV);
   if (e && *e) {
      struct objc_shared_ctrl *c =
         (struct objc_shared_ctrl *)(uintptr_t)strtoull(e, NULL, 16);
      /* The address is published only after magic is set, but spin briefly in
       * case of a startup race with the creating copy. */
      volatile uint64_t *magicp = &c->magic;
      for (int i = 0; i < 1000000 && *magicp != OBJC_CTRL_MAGIC; ++i) { }
      if (*magicp == OBJC_CTRL_MAGIC) { arena_attach(c); return; }
   }

   /* We are the first: create the arena in the low-4GB window. */
   const size_t bytes = (size_t)ARENA_SLOTS * sizeof(uint64_t);
   const size_t len   = (bytes + 0xFFF) & ~(size_t)0xFFF;
   void *region = NULL;
   for (uintptr_t a = LOW_REGION_BASE; a + len <= LOW_REGION_END;
        a += len + 0x1000) {
      mach_vm_address_t addr = a;
      if (mach_vm_allocate(mach_task_self(), &addr, len,
                           VM_FLAGS_FIXED) == KERN_SUCCESS) {
         region = (void *)(uintptr_t)addr;
         break;
      }
   }
   if (!region) {
      fprintf(stderr, "objc_shim: no low-4GB region for proxy arena\n");
      abort();
   }

   /* Only the FIRST copy ever reaches here, so its malloc shim (libabiconv
    * defines calloc) allocates these exactly once per process — the original
    * crash came from EVERY copy re-allocating. mach_vm_allocate is no good
    * here: interpose.dylib constrains it to the low-4GB window, which can't
    * fit a 32MB ANYWHERE request (KERN_NO_SPACE). */
   struct map_ent *map =
      (struct map_ent *)calloc(MAP_CAP, sizeof(struct map_ent));
   if (!map) {
      fprintf(stderr, "objc_shim: proxy map alloc failed\n");
      abort();
   }

   struct objc_shared_ctrl *c =
      (struct objc_shared_ctrl *)calloc(1, sizeof(*c));
   if (!c) {
      fprintf(stderr, "objc_shim: ctrl alloc failed\n");
      abort();
   }
   c->arena      = (uint64_t)(uintptr_t)region;
   c->arena_base = (uint64_t)(uintptr_t)region;
   c->arena_end  = (uint64_t)(uintptr_t)region + bytes;
   c->arena_used = 0;
   c->map        = map;
   __sync_synchronize();
   c->magic      = OBJC_CTRL_MAGIC;

   char buf[32];
   snprintf(buf, sizeof buf, "0x%llx", (unsigned long long)(uintptr_t)c);
   setenv(OBJC_CTRL_ENV, buf, 1);

   arena_attach(c);
}

static uint32_t hash64(uint64_t x) {
   x ^= x >> 33;
   x *= 0xff51afd7ed558ccdULL;
   x ^= x >> 33;
   return (uint32_t)x;
}

/* real 64-bit object -> 32-bit handle the translated code can store. */
uint32_t x64_objc_wrap(uint64_t real) {
   if (real == 0) { return 0; }
   arena_init();

   uint32_t i = hash64(real) & (MAP_CAP - 1);
   while (g_map[i].handle != 0) {
      if (g_map[i].real == real) { return g_map[i].handle; }
      i = (i + 1) & (MAP_CAP - 1);
   }

   if (g_ctrl->arena_used >= ARENA_SLOTS) {
      fprintf(stderr, "objc_shim: proxy arena exhausted\n");
      abort();
   }
   uint32_t slot = g_ctrl->arena_used++;
   g_arena[slot] = real;
   uint32_t handle = (uint32_t)(uintptr_t)&g_arena[slot];

   g_map[i].real   = real;
   g_map[i].handle = handle;
   return handle;
}

/*
 * Bounce a 64-bit C-string return (UTF8String etc.) into a low-4GB buffer so
 * the translated i386 caller gets a 32-bit pointer it can dereference. Real
 * char* returns from Foundation point into 64-bit-mapped CFString storage,
 * which truncates to garbage in a 32-bit slot. We round-robin a small ring of
 * shim-malloc'd (low-4GB) buffers; UTF8String results are short-lived (used
 * immediately by the caller), so a buffer is safe to recycle long before it
 * wraps around. Returns 0 for NULL; passes an already-low pointer through.
 */
#define CSTR_RING 64
static char    *g_cstr_buf[CSTR_RING];
static size_t   g_cstr_cap[CSTR_RING];
static unsigned g_cstr_idx;

uint32_t x64_objc_bounce_cstr(const char *s) {
   if (!s) { return 0; }
   uintptr_t p = (uintptr_t)s;
   if (p < 0x100000000UL) { return (uint32_t)p; }   /* already low; usable */

   size_t n = strlen(s) + 1;
   unsigned i = g_cstr_idx;
   g_cstr_idx = (g_cstr_idx + 1) % CSTR_RING;
   if (g_cstr_cap[i] < n) {
      char *nb = (char *)realloc(g_cstr_buf[i], n);  /* shim malloc -> low-4GB */
      if (!nb) { return 0; }
      g_cstr_buf[i] = nb;
      g_cstr_cap[i] = n;
   }
   memcpy(g_cstr_buf[i], s, n);
   uintptr_t bp = (uintptr_t)g_cstr_buf[i];
   if (getenv("OBJC_BRIDGE_TRACE")) {
      fprintf(stderr, "[bp]   bounce_cstr in=%p -> 0x%lx \"%.32s\"\n",
              (void*)s, (unsigned long)bp, s);
      fflush(stderr);
   }
   return bp < 0x100000000UL ? (uint32_t)bp : 0;
}

/* 32-bit handle -> real 64-bit object. A value that is not an arena handle
 * is passed through zero-extended (nil, or an already-low raw value). */
uint64_t x64_objc_unwrap(uint32_t h) {
   if (h == 0) { return 0; }
   uintptr_t p = h;
   if (p >= g_arena_base && p < g_arena_end) {
      return *(uint64_t *)p;
   }
   return (uint64_t)h;
}

/*
 * Register layout handed back to the asm trampoline. reg[0..5] map to
 * rdi,rsi,rdx,rcx,r8,r9. Offsets are relied on by objc_msgSend.asm:
 *   reg      @ 0
 *   nreg     @ 48
 *   ret_is_obj @ 52
 *   super    @ 56  — temp objc_super for the Super* variants
 *   legacy_imp @ 72  — see below
 *
 * legacy_imp: if nonzero, the receiver is one of the translated binary's OWN
 * legacy ObjC-1.0 classes (defined in __OBJC, never registered with the modern
 * runtime), and this is the runtime address of the translated i386 IMP for the
 * sent class method. The asm trampoline then does NOT call objc_msgSend at all:
 * it restores the original i386 cdecl frame (which is still intact on the stack,
 * since the i386 caller pushed self/_cmd/args as 4-byte slots) and jmps straight
 * into the IMP, resuming i386 dispatch. No reverse ABI marshalling is needed
 * because we never left i386-cdecl land — every [self ...] inside the IMP just
 * re-enters this same forward bridge. See legacy_class_method_imp() below.
 */
struct objc_super_x64 { id receiver; Class super_class; };
/* Overflow args (7th SysV integer arg onward) live on the stack. objc varargs
 * (arrayWithObjects:, dictionaryWithObjectsAndKeys:, stringWithFormat:) can
 * exceed the 6 GP registers AND must carry a nil terminator past the last
 * object — when the explicit objects fill rdi..r9 the terminator itself lands
 * in stack[0]. The asm trampoline copies plan.stack[0..nstack) onto the real
 * stack before the call (see objc_msgSend.asm). */
#define PLAN_STACK_MAX 32
struct objc_call_plan {
   uint64_t reg[6];               /* +0  rdi,rsi,rdx,rcx,r8,r9            */
   int32_t  nreg;                 /* +48                                 */
   int32_t  ret_is_obj;           /* +52                                 */
   struct objc_super_x64 super;   /* +56 (16 bytes)                      */
   uint64_t legacy_imp;           /* +72                                 */
   uint32_t nstack;               /* +80 number of valid stack[] entries */
   uint32_t _pad;                 /* +84                                 */
   uint64_t stack[PLAN_STACK_MAX];/* +88 overflow args (7th onward)      */
};

/* Write SysV integer-arg position `pos` (0=rdi..5=r9, 6+=stack) into the plan. */
static inline void plan_put(struct objc_call_plan *plan, unsigned pos, uint64_t v) {
   if (pos < 6) {
      plan->reg[pos] = v;
   } else if (pos - 6 < PLAN_STACK_MAX) {
      plan->stack[pos - 6] = v;
   }
}

/* Defined after the legacy __OBJC struct decls at the bottom of this file.
 * Given an i386 class-name-string pointer (the value the translated __cls_refs
 * slot holds) and the sent selector name, returns the runtime address of the
 * translated class-method IMP, or 0 if the name is not one of our own legacy
 * classes / the selector is not a legacy class method. */
static uint64_t legacy_class_method_imp(uint32_t self32, const char *sel_name);
static uint64_t legacy_class_method_imp_byname(const char *clsname,
                                               const char *sel_name);
/* Reverse bridge (defined near end of file): if self32 is an i386 shadow of a
 * registered legacy-class instance, return the real modern object it stands
 * for, else 0. Lets a legacy IMP's `[self ...]` re-dispatch on the real obj. */
static id shadow_real(uint32_t s);

/* i386 layout of struct objc_super: two 4-byte pointers. The translated
 * code passes a pointer to this struct as the first arg of msgSendSuper. */
struct objc_super_i386 { uint32_t receiver; uint32_t super_class; };

/* Is [p, p+len) readable in this task? Uses mach_vm_read_overwrite so we can
 * safely test a translated-program pointer before treating it as a C-string
 * and handing it to objc_getClass / sel_registerName. Without this, a self32
 * or cmd32 that is an unmapped value (truncated real pointer or garbage)
 * crashes deep inside libobjc with KERN_INVALID_ADDRESS. Only ever hit on
 * the non-arena (class-message / selector) path, so the cost is off the
 * common instance-message hot path. */
static int mem_readable(uintptr_t p, size_t len) {
   if (p == 0) { return 0; }
   char buf[32];
   while (len) {
      size_t chunk = len < sizeof buf ? len : sizeof buf;
      mach_vm_size_t got = 0;
      kern_return_t kr = mach_vm_read_overwrite(
         mach_task_self(), (mach_vm_address_t)p, chunk,
         (mach_vm_address_t)(uintptr_t)buf, &got);
      if (kr != KERN_SUCCESS || got != chunk) { return 0; }
      p += chunk;
      len -= chunk;
   }
   return 1;
}

/* Resolve an i386 self32 to a real x86_64 id. Handles the three forms a
 * translated binary can pass: arena proxy handle, class-name cstring
 * pointer (for class messages), or nil. An unmapped value is none of these:
 * log it and fall back to nil rather than crashing objc_getClass. */
static id resolve_self(uint32_t self32) {
   uintptr_t sp = self32;
   /* A legacy instance method runs with self32 = its i386 shadow; a `[self ...]`
    * inside it must re-dispatch on the real modern object (so inherited/AppKit
    * methods hit real impls and our registered legacy methods route back
    * through the reverse bridge). */
   id sreal = shadow_real(self32);
   if (sreal) { return sreal; }
   if (sp >= g_arena_base && sp < g_arena_end) {
      return (id)(uintptr_t)*(uint64_t *)sp;
   }
   if (self32 != 0) {
      if (!mem_readable(sp, 1)) {
         if (getenv("OBJC_BRIDGE_TRACE")) {
            fprintf(stderr, "[bp] resolve_self: unmapped self32=0x%08x -> nil\n",
                    self32);
            fflush(stderr);
         }
         return (id)0;
      }
      id cls = (id)objc_getClass((const char *)(uintptr_t)self32);
      if (!cls && getenv("OBJC_BRIDGE_TRACE")) {
         fprintf(stderr, "[bp] resolve_self: no class named \"%s\" (self32=0x%08x)\n",
                 (const char *)(uintptr_t)self32, self32);
         fflush(stderr);
      }
      return cls;
   }
   return (id)0;
}

/* Resolve an i386 cmd32 selector-name pointer to a real SEL, guarding against
 * an unmapped pointer the same way resolve_self does. */
static SEL resolve_sel(uint32_t cmd32) {
   if (cmd32 != 0 && !mem_readable((uintptr_t)cmd32, 1)) {
      if (getenv("OBJC_BRIDGE_TRACE")) {
         fprintf(stderr, "[bp] resolve_sel: unmapped cmd32=0x%08x -> NULL\n",
                 cmd32);
         fflush(stderr);
      }
      return (SEL)0;
   }
   return sel_registerName((const char *)(uintptr_t)cmd32);
}

/* Per-method arg unwrap loop: iterates from method-arg index `arg_start`
 * (==2 for self+cmd already-placed methods) up through `cap_regs`,
 * pulling 4-byte slots out of args32 and writing 8-byte slots into plan.
 * Slot-i of args32 maps to plan->reg[reg_base + (i - arg_start)]. */
/* Strip ObjC type-encoding qualifier prefixes (const/in/out/byref/...) so we
 * see the underlying type letter. */
static char encoding_base_type(const char *t) {
   if (!t) { return 0; }
   while (*t && strchr("rnNoORV", *t)) { ++t; }
   return *t;
}

static unsigned fill_method_args(struct objc_call_plan *plan,
                                 const uint32_t *args32,
                                 unsigned arg_base_idx,
                                 unsigned reg_base,
                                 Method m,
                                 unsigned cap_regs) {
   unsigned nargs = m ? method_getNumberOfArguments(m) : 2;
   if (m && method_getNumberOfArguments(m) > cap_regs
       && getenv("OBJC_BRIDGE_TRACE")) {
      fprintf(stderr, "[bp] WARN: method takes %u args but only %u fit in the "
              "register plan; extra args dropped (stack spill not implemented)\n",
              method_getNumberOfArguments(m) - 2, cap_regs - 2);
      fflush(stderr);
   }
   if (nargs > cap_regs) { nargs = cap_regs; }
   const int trace = getenv("OBJC_BRIDGE_TRACE") != NULL;
   for (unsigned i = 2; i < nargs; ++i) {
      char *t = m ? method_copyArgumentType(m, i) : NULL;
      char bt = encoding_base_type(t);
      uint32_t a = args32[arg_base_idx + (i - 2)];
      if (bt == '@' || bt == '#') {
         plan->reg[reg_base + (i - 2)] = x64_objc_unwrap(a);
      } else {
         plan->reg[reg_base + (i - 2)] = (uint64_t)a;
         /* float/double/long-double/struct/union args use the XMM/MEMORY
          * SysV classes and/or differ in width and CGFloat (i386 float vs
          * x86_64 double) representation; the GP-only marshaller below does
          * not yet handle them. Surface it instead of corrupting silently —
          * see the "remaining ABI gap" note above the msgSend entry points. */
         if (trace && bt && strchr("fdD{(", bt)) {
            fprintf(stderr, "[bp] WARN: arg %u type '%c' (float/double/struct) "
                    "marshalled as integer GP reg — call may be incorrect\n",
                    i - 2, bt);
            fflush(stderr);
         }
      }
      free(t);
   }
   return nargs;
}

/* Trace dump: i386 stack arg slots [0..7] in hex. */
static void trace_args(const char *prefix, const char *cls_name,
                       SEL sel, const uint32_t *args32) {
   fprintf(stderr, "[bp] %s class=%s sel=%s args=[",
           prefix, cls_name, (const char *)sel);
   for (unsigned i = 0; i < 8; ++i) {
      fprintf(stderr, "%s%08x", i ? "," : "", args32[i]);
   }
   fprintf(stderr, "]\n");
   fflush(stderr);
}

/*
 * Variadic ObjC method detection.
 *
 * The ObjC runtime doesn't model varargs in the Method type encoding —
 * method_getNumberOfArguments returns only the fixed-arg count. For a
 * method like -[NSArray initWithObjects:] that signature is `(self,_cmd,
 * firstObj)` = 3, even though the real call passes a nil-terminated list.
 *
 * Without varargs handling, bridge_prep zero-fills plan->reg past the
 * fixed count. Foundation does `va_arg(ap, id)` on rcx (4th arg, first
 * past the fixed obj), reads 0, treats it as the nil terminator, and
 * builds an array with just `firstObj` instead of the caller's list.
 * Tessera's `__GLOBAL__I_TESTellus*` static init hits this for
 * `[NSArray arrayWithObjects:n1, n2, nil]` — the truncated array
 * cascades into wild-pointer dereferences a few calls downstream.
 *
 * Fix: maintain a known-varargs sel-name table; for those, walk past
 * the fixed args unwrapping arena handles as `id`-typed until we see
 * a nil sentinel or run out of register slots. Capacity is the same
 * 6-slot plan as fixed args — covers the typical 2-4 explicit varargs
 * we see in real binaries. Longer lists would need plan extension +
 * trampoline stack spill (sysv ABI > 6 GP args land above rsp).
 */
static int sel_has_suffix(const char *s, const char *suf) {
   size_t ls = strlen(s), lf = strlen(suf);
   return ls >= lf && strcmp(s + (ls - lf), suf) == 0;
}

static int is_varargs_sel(const char *sel_name) {
   if (!sel_name) { return 0; }

   /* Suffix families that are reliably variadic across Foundation/AppKit,
    * so we don't have to enumerate every concrete selector. These are
    * conservative (low false-positive): only the trailing keyword of a
    * nil-/format-terminated method matches. */
   static const char *const SUFFIXES[] = {
      "WithObjects:",          /* arrayWithObjects:, setWithObjects:, ... */
      "ObjectsAndKeys:",       /* dictionaryWithObjectsAndKeys:, init... */
      "WithFormat:",           /* stringWithFormat:, initWithFormat:, predicateWithFormat:, ... */
      "AppendingFormat:",      /* stringByAppendingFormat: */
      "appendFormat:",
      ":format:",              /* +[NSException raise:format:] etc. */
      NULL,
   };
   for (const char *const *p = SUFFIXES; *p; ++p) {
      if (sel_has_suffix(sel_name, *p)) { return 1; }
   }

   /* Allow a target-specific extension list without recompiling:
    * ABICONV_VARARGS_SELS="selA:,selB:" (comma/space separated, exact match). */
   const char *env = getenv("ABICONV_VARARGS_SELS");
   if (env && *env) {
      size_t ln = strlen(sel_name);
      const char *p = env;
      while (*p) {
         while (*p == ',' || *p == ' ') { ++p; }
         const char *start = p;
         while (*p && *p != ',' && *p != ' ') { ++p; }
         size_t len = (size_t)(p - start);
         if (len == ln && strncmp(start, sel_name, len) == 0) { return 1; }
      }
   }
   return 0;
}

/* Format-string-taking variadic selectors: stringWithFormat:, initWithFormat:,
 * predicateWithFormat:, stringByAppendingFormat:, appendFormat:, raise:format:,
 * etc. Distinguished from the nil-terminated id-list varargs (WithObjects:,
 * ObjectsAndKeys:) because their args are TYPED by the format string, not an
 * id list — each conversion tells us whether the arg is an object (resolve the
 * proxy handle to the real id), an integer (pass through), or a float. */
static int is_format_sel(const char *s) {
   return s && (sel_has_suffix(s, "WithFormat:") || sel_has_suffix(s, "AppendingFormat:")
                || sel_has_suffix(s, "appendFormat:") || sel_has_suffix(s, ":format:"));
}

/* Extract the C-string of a real Foundation format object (an NSString, which
 * may be a translated low-address constant @"..." or a real 64-bit NSString).
 * Returns 1 and fills buf on success; 0 if obj is not a usable CFString. */
static int format_cstr(uint64_t obj, char *buf, size_t buflen) {
   if (obj == 0 || !mem_readable((uintptr_t)obj, sizeof(void *))) { return 0; }
   CFTypeRef cf = (CFTypeRef)(uintptr_t)obj;
   if (CFGetTypeID(cf) != CFStringGetTypeID()) { return 0; }
   return CFStringGetCString((CFStringRef)cf, buf, (CFIndex)buflen,
                             kCFStringEncodingUTF8) ? 1 : 0;
}

/*
 * Place a format method's variadic args into the plan's GP registers, typed by
 * the format string. %@ args are resolved handle->real id (the whole point);
 * integers/strings/pointers pass through (NO unwrap — an int value that happens
 * to land in the arena range must not be "resolved"); floats would need XMM
 * (not modelled by the plan/asm) so they are skipped with a warning. Returns
 * the next free plan->reg index (so the caller can compute plan->nreg).
 *
 * gp/gp_cap index plan->reg directly; ai indexes args32. In x86_64 SysV
 * varargs, GP and SSE arg sequences are INDEPENDENT, so skipping a float does
 * not perturb the GP slot of a following %@.
 */
static unsigned fill_format_varargs(struct objc_call_plan *plan,
                                    const uint32_t *args32,
                                    unsigned ai, unsigned gp, unsigned gp_cap,
                                    const char *fmt) {
   const int trace = getenv("OBJC_BRIDGE_TRACE") != NULL;
   for (const char *p = fmt; *p; ++p) {
      if (*p != '%') { continue; }
      ++p;
      if (*p == '%' || *p == '\0') { continue; }     /* literal %% */
      /* flags */
      while (*p && strchr("-+ #0'", *p)) { ++p; }
      /* width: digits or '*' (consumes an int arg) */
      if (*p == '*') { if (gp < gp_cap) { plan_put(plan, gp++, (uint64_t)args32[ai]); } ai++; ++p; }
      else { while (*p >= '0' && *p <= '9') { ++p; } }
      /* precision */
      if (*p == '.') {
         ++p;
         if (*p == '*') { if (gp < gp_cap) { plan_put(plan, gp++, (uint64_t)args32[ai]); } ai++; ++p; }
         else { while (*p >= '0' && *p <= '9') { ++p; } }
      }
      /* length modifiers; 64-bit only for ll/q/j (i386 long/size_t are 4 bytes) */
      int len64 = 0;
      while (*p && strchr("hlLqjzt", *p)) {
         if ((*p == 'l' && p[1] == 'l') || *p == 'q' || *p == 'j') { len64 = 1; }
         ++p;
      }
      char c = *p;
      if (c == '\0') { break; }
      switch (c) {
      case '@':
         if (gp < gp_cap) { plan_put(plan, gp++, x64_objc_unwrap(args32[ai])); }
         ai++;
         break;
      case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': case 'c': case 'C':
         if (len64) {
            if (gp < gp_cap) {
               plan_put(plan, gp++, (uint64_t)args32[ai] | ((uint64_t)args32[ai + 1] << 32));
            }
            ai += 2;
         } else {
            if (gp < gp_cap) { plan_put(plan, gp++, (uint64_t)args32[ai]); }
            ai++;
         }
         break;
      case 's': case 'S': case 'p':
         if (gp < gp_cap) { plan_put(plan, gp++, (uint64_t)args32[ai]); }
         ai++;
         break;
      case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
         /* default-promoted to double: 8 bytes on the i386 stack, belongs in an
          * XMM reg on x86_64 — not modelled yet, so the value is dropped. */
         if (trace) {
            fprintf(stderr, "[bp] WARN: format %%%c (float/double) not bridged to "
                    "XMM; that conversion will print garbage\n", c);
            fflush(stderr);
         }
         ai += 2;
         break;
      default:
         if (gp < gp_cap) { plan_put(plan, gp++, (uint64_t)args32[ai]); }
         ai++;
         break;
      }
   }
   return gp;
}

/*
 * Common core for all msgSend variants. The shape:
 *   - real_self is already resolved (by variant-specific entry below);
 *   - sel is already registered;
 *   - args32 + arg_base_idx points at the first explicit-arg i386 slot;
 *   - reg_base is the first plan->reg slot to write args into.
 *
 *  variants:
 *    msgSend:        reg_base=2 (self@0, _cmd@1)
 *    msgSendSuper:   reg_base=2 (super@0, _cmd@1)
 *    msgSend_stret:  reg_base=3 (retbuf@0, self@1, _cmd@2)
 *    msgSendSuper_stret: reg_base=3 (retbuf@0, super@1, _cmd@2)
 *
 *  arg_base_idx is the args32 index of the first explicit arg
 *  (corresponds to method-arg index 2).
 */
static void fill_args_and_return(struct objc_call_plan *plan,
                                 const uint32_t *args32,
                                 unsigned arg_base_idx,
                                 unsigned reg_base,
                                 id real_self, SEL sel) {
   Method m = NULL;
   if (real_self) {
      Class lookup = object_getClass(real_self);
      m = class_getInstanceMethod(lookup, sel);
   }

   const unsigned cap = 6 - reg_base + 2;     /* method-arg index cap */
   unsigned nargs = fill_method_args(plan, args32, arg_base_idx,
                                     reg_base, m, cap);

   /* Varargs continuation. Foundation methods like initWithObjects: take
    * a nil-terminated id list past their fixed-arg count. The signature
    * doesn't model varargs so method_getNumberOfArguments stops at
    * firstObj; the next register would stay zero (== nil) and Foundation
    * would terminate the list early. Walk forward through args32 until
    * nil sentinel or plan-register capacity. */
   if (m && is_varargs_sel((const char *)sel)) {
      char fmtbuf[2048];
      if (is_format_sel((const char *)sel) && nargs >= 3
          && format_cstr(plan->reg[reg_base + (nargs - 3)], fmtbuf, sizeof fmtbuf)) {
         /* Format-typed varargs: the last fixed arg (reg_base + nargs - 3) is
          * the format NSString, already unwrapped by fill_method_args. Place
          * the following args by conversion type, resolving %@ handles, spilling
          * past the 6 GP regs onto the plan stack. */
         unsigned gp_end = fill_format_varargs(plan, args32,
                                               arg_base_idx + (nargs - 2),
                                               reg_base + (nargs - 2),
                                               /*gp_cap=*/6 + PLAN_STACK_MAX, fmtbuf);
         nargs = (gp_end - reg_base) + 2;
      } else {
         /* nil-terminated id list (arrayWithObjects:, dictionaryWith...Keys:).
          * Walk forward placing each object (registers then stack) up to and
          * INCLUDING the nil terminator — when the explicit objects fill all 6
          * GP registers the terminator itself must still go out, in stack[0],
          * or Foundation reads uninitialized stack and retains garbage. */
         unsigned i = nargs;
         const unsigned pos_cap = 6 + PLAN_STACK_MAX;
         for (;;) {
            unsigned pos = reg_base + (i - 2);
            if (pos >= pos_cap) { break; }
            uint32_t a = args32[arg_base_idx + (i - 2)];
            plan_put(plan, pos, x64_objc_unwrap(a));
            ++i;
            if (a == 0) { break; }        /* placed the nil terminator */
         }
         nargs = i;
      }
   }

   {
      unsigned total = reg_base + (nargs - 2);   /* SysV integer positions used */
      plan->nreg   = (int32_t)(total > 6 ? 6 : total);
      plan->nstack = (uint32_t)(total > 6 ? total - 6 : 0);
   }

   /* ret_is_obj is really a 3-way return-kind for the asm trampoline:
    *   0 = scalar/struct (pass rax straight back, truncated to eax by caller)
    *   1 = object/Class  (wrap the 64-bit id into a 32-bit arena handle)
    *   2 = C-string      (bounce the 64-bit char* into a low-4GB buffer so the
    *                      i386 caller gets a 32-bit pointer it can dereference;
    *                      UTF8String / fileSystemRepresentation / etc.) */
   char *rt = m ? method_copyReturnType(m) : NULL;
   if (rt && (rt[0] == '@' || rt[0] == '#')) {
      plan->ret_is_obj = 1;
   } else if (rt && (rt[0] == '*' || (rt[0] == 'r' && rt[1] == '*'))) {
      plan->ret_is_obj = 2;
   } else {
      plan->ret_is_obj = 0;
   }
   if (getenv("OBJC_BRIDGE_TRACE") && sel) {
      fprintf(stderr, "[bp]   ret_kind=%d rt=\"%s\" sel=%s m=%p\n",
              plan->ret_is_obj, rt ? rt : "(null)", sel_getName(sel), (void*)m);
      fflush(stderr);
   }
   free(rt);
}

/*
 * ============================ REMAINING ABI GAP ============================
 * The arg marshaller (fill_method_args) currently classifies every explicit
 * argument as a single 4-byte integer/pointer slot placed in a GP register.
 * That is correct for int/long/pointer/BOOL/char/short/SEL/id/Class args —
 * the overwhelming majority of startup traffic — but NOT for:
 *
 *   - float / double / long double: SysV x86_64 passes these in XMM0-7, not
 *     GP regs. The plan struct + objc_msgSend.asm would need an xmm[8] block
 *     and `movsd`/`mov al,nxmm` before the call.
 *   - CGFloat: i386 CGFloat is a 4-byte float; x86_64 CGFloat is an 8-byte
 *     double. The x86_64 method encoding reports 'd' for BOTH a real double
 *     and a CGFloat, so the true source width must come from the translated
 *     binary's *legacy i386* __OBJC method `types` (see legacy metadata walk
 *     in _86x64_objc_register_classes), not method_copyArgumentType.
 *   - struct/union by value (NSPoint/NSSize/NSRect, etc.): need full SysV
 *     field classification (the same logic abigen/typeconv.cc already does
 *     for C functions) plus i386-float→x86_64-double element conversion.
 *   - 64-bit scalars (long long): occupy two i386 slots; only one is read.
 *   - >4 explicit args: spill to the stack above rsp (asm change).
 *
 * fill_method_args emits an OBJC_BRIDGE_TRACE warning when it sees an arg it
 * can't faithfully marshal, so these cases are observable rather than silent.
 * Closing this gap is the main remaining work for fully universal ObjC.
 * ===========================================================================
 *
 * objc_msgSend (and objc_msgSend_fpret):
 *   args32[0] = self (handle / class-name ptr / 0)
 *   args32[1] = _cmd (cstring ptr)
 *   args32[2..] = explicit args
 */
void objc_bridge_prep(struct objc_call_plan *plan, const uint32_t *args32) {
   arena_init();

   if (getenv("OBJC_BRIDGE_TRACE")) {
      fprintf(stderr, "[bp] entry: self32=0x%08x cmd32=0x%08x args[2..5]=0x%08x 0x%08x 0x%08x 0x%08x arena=[0x%lx..0x%lx)\n",
              args32[0], args32[1], args32[2], args32[3], args32[4], args32[5],
              (unsigned long)g_arena_base, (unsigned long)g_arena_end);
      fflush(stderr);
   }

   id real_self = resolve_self(args32[0]);
   SEL sel = resolve_sel(args32[1]);

   if (getenv("OBJC_BRIDGE_TRACE")) {
      const char *cls_name = "(nil)";
      if (real_self) {
         Class c = object_getClass(real_self);
         if (c) cls_name = class_getName(c);
      }
      trace_args("send", cls_name, sel, args32);
   }

   /* Legacy class-method dispatch fallback. Two distinct cases need it, both
    * resolved by jumping straight into the translated i386 IMP:
    *   (a) real_self == nil: a class message to one of our binary's OWN legacy
    *       classes, which the modern runtime never registered (e.g.
    *       +[UpgradeChecker checkOSVersion:]).
    *   (b) real_self is a real (system) class that our binary extended with a
    *       legacy CATEGORY, and the selector is one of those category class
    *       methods (e.g. +[NSObject isLogEnabled]). The real class does NOT
    *       respond, so sending to it would raise "unrecognized selector sent
    *       to class" and abort; dispatch to the legacy IMP instead.
    * Only attempt this for CLASS receivers (nil, or object_isClass): legacy
    * category dispatch walks the metaclass + category class-method lists.
    * legacy_class_method_imp() keys off the i386 class-name pointer (args32[0])
    * and returns 0 on a miss, in which case we fall through to the normal path
    * (no worse than before). */
   if (sel) {
      uint64_t imp = 0;
      if (!real_self) {
         /* Own legacy class: args32[0] is the __cls_refs class-name pointer. */
         imp = legacy_class_method_imp(args32[0], sel_getName(sel));
      } else if (object_isClass(real_self) &&
                 class_getClassMethod((Class)real_self, sel) == NULL) {
         /* Real (system) class that lacks this selector — a legacy category
          * class method (e.g. +[NSObject isLogEnabled]/+boolForDefaultsKey:).
          * args32[0] may be a __cls_refs name ptr OR a proxy handle (when the
          * Class came from `+class`), so look up by the real class name. */
         imp = legacy_class_method_imp_byname(class_getName((Class)real_self),
                                              sel_getName(sel));
      }
      if (imp) {
         if (getenv("OBJC_BRIDGE_TRACE")) {
            fprintf(stderr, "[bp] legacy class-method dispatch \"%s\" -> imp 0x%llx\n",
                    sel_getName(sel), (unsigned long long)imp);
            fflush(stderr);
         }
         plan->legacy_imp = imp;
         return;
      }
   }

   plan->reg[0] = (uint64_t)real_self;
   plan->reg[1] = (uint64_t)sel;
   plan->reg[2] = plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;

   fill_args_and_return(plan, args32, /*arg_base_idx=*/2, /*reg_base=*/2,
                        real_self, sel);
}

/*
 * objc_msgSend_stret: i386 ABI puts the struct-return buffer as an
 * implicit first arg (caller pushes its address before self). x86_64
 * SysV passes it in rdi with self/cmd shifted into rsi/rdx.
 *
 *   args32[0] = retbuf (pass-through low-4GB ptr)
 *   args32[1] = self
 *   args32[2] = _cmd
 *   args32[3..] = explicit args
 */
void objc_bridge_prep_stret(struct objc_call_plan *plan, const uint32_t *args32) {
   arena_init();

   id real_self = resolve_self(args32[1]);
   SEL sel = resolve_sel(args32[2]);

   if (getenv("OBJC_BRIDGE_TRACE")) {
      const char *cls_name = "(nil)";
      if (real_self) {
         Class c = object_getClass(real_self);
         if (c) cls_name = class_getName(c);
      }
      trace_args("stret", cls_name, sel, args32);
   }

   plan->reg[0] = (uint64_t)args32[0];
   plan->reg[1] = (uint64_t)real_self;
   plan->reg[2] = (uint64_t)sel;
   plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;

   fill_args_and_return(plan, args32, /*arg_base_idx=*/3, /*reg_base=*/3,
                        real_self, sel);
   /* The return is a struct via retbuf — never an object handle. */
   plan->ret_is_obj = 0;
}

/* Defined with the reverse-bridge machinery below; lets a legacy [super sel]
 * tell reverse_prep to dispatch the SUPER's legacy method rather than the
 * receiver's derived override (prevents +initialize-style recursion). */
static void reverse_note_super(id recv, SEL sel, Class super_lookup);

/*
 * objc_msgSendSuper: args32[0] points at a struct objc_super_i386 in the
 * caller's stack frame. Resolve both halves, build an x86_64-layout
 * super struct inside the plan, and pass its address in reg[0].
 *
 *   args32[0] = &objc_super_i386 { receiver_handle, super_class_handle }
 *   args32[1] = _cmd
 *   args32[2..] = explicit args
 */
void objc_bridge_prep_super(struct objc_call_plan *plan, const uint32_t *args32) {
   arena_init();

   const struct objc_super_i386 *super_i386 =
      (const struct objc_super_i386 *)(uintptr_t)args32[0];
   id real_receiver = resolve_self(super_i386->receiver);
   /* super_class slot can be either a Class proxy handle (rare) or a
    * class-name cstring pointer in __OBJC,__cls_refs. resolve_self
    * handles both. */
   id super_class_id = resolve_self(super_i386->super_class);
   SEL sel = resolve_sel(args32[1]);

   /*
    * Class-method super-calls (`[super foo]` inside a `+method`). When the
    * receiver is itself a class object, the message is a CLASS method, which
    * lives on the METACLASS, not the regular class. objc_msgSendSuper starts
    * its method lookup in super.super_class's own method list, so for a class
    * method that field must be the super's METACLASS — otherwise the runtime
    * searches the super class's INSTANCE methods, fails to find the class
    * method, and raises "+[Class sel]: unrecognized selector" (iPhoto 10th
    * blocker: +[IPPublishPluginManager initialize]'s `[super initialize]`).
    * The instance-method super-call (e.g. ExtendedApplication -init's
    * [super init]) keeps super_class as the regular class — receiver is an
    * instance there, so object_isClass is false and this is skipped.
    */
   if (super_class_id && real_receiver && object_isClass(real_receiver) &&
       !class_isMetaClass((Class)super_class_id)) {
      super_class_id = (id)object_getClass(super_class_id);
   }

   plan->super.receiver    = real_receiver;
   plan->super.super_class = (Class)super_class_id;

   /* Tell reverse_prep to look this sel up on the SUPER class, so a legacy
    * [super sel] runs the super's method, not the receiver's override. */
   reverse_note_super(real_receiver, sel, (Class)super_class_id);

   if (getenv("OBJC_BRIDGE_TRACE")) {
      const char *cls_name = "(nil)";
      if (real_receiver) {
         Class c = object_getClass(real_receiver);
         if (c) cls_name = class_getName(c);
      }
      trace_args("super", cls_name, sel, args32);
   }

   plan->reg[0] = (uint64_t)(uintptr_t)&plan->super;
   plan->reg[1] = (uint64_t)sel;
   plan->reg[2] = plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;

   /* For arg-type introspection: look up the method on the SUPER class,
    * not the receiver's class (the call goes to super's IMP). */
   id type_lookup = real_receiver;
   if (plan->super.super_class) {
      type_lookup = (id)plan->super.super_class;
   }
   fill_args_and_return(plan, args32, /*arg_base_idx=*/2, /*reg_base=*/2,
                        type_lookup, sel);
}

/*
 * objc_msgSendSuper_stret: combination — retbuf is implicit-first, then
 * the super struct ptr, then _cmd.
 */
void objc_bridge_prep_super_stret(struct objc_call_plan *plan,
                                  const uint32_t *args32) {
   arena_init();

   const struct objc_super_i386 *super_i386 =
      (const struct objc_super_i386 *)(uintptr_t)args32[1];
   id real_receiver = resolve_self(super_i386->receiver);
   id super_class_id = resolve_self(super_i386->super_class);
   SEL sel = resolve_sel(args32[2]);

   /* Class-method super-call: search the super's metaclass (see
    * objc_bridge_prep_super for the full rationale). */
   if (super_class_id && real_receiver && object_isClass(real_receiver) &&
       !class_isMetaClass((Class)super_class_id)) {
      super_class_id = (id)object_getClass(super_class_id);
   }

   plan->super.receiver    = real_receiver;
   plan->super.super_class = (Class)super_class_id;

   reverse_note_super(real_receiver, sel, (Class)super_class_id);

   if (getenv("OBJC_BRIDGE_TRACE")) {
      const char *cls_name = "(nil)";
      if (real_receiver) {
         Class c = object_getClass(real_receiver);
         if (c) cls_name = class_getName(c);
      }
      trace_args("super_stret", cls_name, sel, args32);
   }

   plan->reg[0] = (uint64_t)args32[0];
   plan->reg[1] = (uint64_t)(uintptr_t)&plan->super;
   plan->reg[2] = (uint64_t)sel;
   plan->reg[3] = plan->reg[4] = plan->reg[5] = 0;

   id type_lookup = real_receiver;
   if (plan->super.super_class) {
      type_lookup = (id)plan->super.super_class;
   }
   fill_args_and_return(plan, args32, /*arg_base_idx=*/3, /*reg_base=*/3,
                        type_lookup, sel);
   plan->ret_is_obj = 0;
}

/* ---------------------------------------------------------------------
 * Legacy ObjC 1.0 ABI class registration (skeleton)
 * ---------------------------------------------------------------------
 *
 * iPhoto (and any pre-modern i386 Cocoa app) defines its OWN classes in
 * the legacy __OBJC segment. The modern x86_64 libobjc completely ignores
 * legacy metadata — it only processes the ObjC2 __objc_classlist /
 * __objc_data sections. So unless we walk the legacy metadata and
 * register each class with the modern runtime via
 * objc_allocateClassPair/class_addMethod/class_addIvar/objc_registerClassPair,
 * none of iPhoto's own classes exist at runtime — every `[MyController ...]`
 * fails because MyController is unknown to the runtime.
 *
 * What's here NOW: structure definitions + a metadata walker that just
 * counts and reports what would be registered. This is the foundation
 * for the real registration code, which is gated on:
 *   - a working IMP bridge (x86_64 objc_msgSend ABI -> i386 IMP ABI per
 *     method's type encoding — the reverse of the existing __objc_msgSend
 *     forward marshaller);
 *   - ivar size/alignment derivation from ObjC type encodings;
 *   - two-pass / topological ordering for in-binary superclass deps.
 *
 * Diagnostic walk is invoked via _86x64_objc_register_classes(mh, slide)
 * by the wrapper at load time (after slide-fixup so __OBJC pointers are
 * correct). Outputs to stderr; safe to call multiple times.
 */

#include <mach-o/loader.h>

struct legacy_objc_module {
   uint32_t version;       /* expected 7 */
   uint32_t size;          /* expected 16 (sizeof this struct) */
   uint32_t name;          /* cstring ptr (module name) */
   uint32_t symtab;        /* legacy_objc_symtab ptr */
};

struct legacy_objc_symtab {
   uint32_t sel_ref_cnt;
   uint32_t refs;          /* SEL ptr (selector refs) */
   uint16_t cls_def_cnt;
   uint16_t cat_def_cnt;
   /* defs[cls_def_cnt + cat_def_cnt]: classes then categories */
};

struct legacy_objc_class {
   uint32_t isa;              /* metaclass ptr (CLS_META variant of self) */
   uint32_t super_class;      /* before runtime fixup: cstring ptr to super name */
   uint32_t name;             /* cstring ptr */
   uint32_t version;
   uint32_t info;             /* CLS_CLASS (0x1) / CLS_META (0x2) etc. */
   uint32_t instance_size;
   uint32_t ivars;            /* legacy_objc_ivar_list ptr (or 0) */
   uint32_t methodLists;      /* ptr to NULL-terminated array of method_list ptrs */
   uint32_t cache;
   uint32_t protocols;
};

struct legacy_objc_method {
   uint32_t name;          /* cstring (selector name pre-fixup) */
   uint32_t types;         /* cstring (ObjC type encoding) */
   uint32_t imp;           /* function ptr (the translated x86_64 IMP) */
};

struct legacy_objc_method_list {
   uint32_t obsolete;      /* expected 0 */
   int32_t  method_count;
   /* methods[method_count] */
};

struct legacy_objc_ivar {
   uint32_t name;
   uint32_t type;
   int32_t  offset;
};

struct legacy_objc_ivar_list {
   int32_t ivar_count;
   /* ivars[ivar_count] */
};

struct legacy_objc_category {
   uint32_t category_name;
   uint32_t class_name;
   uint32_t instance_methods;
   uint32_t class_methods;
   uint32_t protocols;
};

/* Locate __OBJC,__module_info in the given Mach-O image. Returns the
 * runtime base address of the section (vmaddr + slide) and writes size. */
static const struct legacy_objc_module *
find_module_info(const struct mach_header_64 *mh, intptr_t slide, size_t *size_out) {
   const uint8_t *cmd_ptr = (const uint8_t *)(mh + 1);
   for (uint32_t c = 0; c < mh->ncmds; ++c) {
      const struct load_command *lc = (const struct load_command *)cmd_ptr;
      cmd_ptr += lc->cmdsize;
      if (lc->cmd != LC_SEGMENT_64) continue;
      const struct segment_command_64 *seg = (const struct segment_command_64 *)lc;
      if (strcmp(seg->segname, "__OBJC") != 0) continue;
      const struct section_64 *sects = (const struct section_64 *)(seg + 1);
      for (uint32_t s = 0; s < seg->nsects; ++s) {
         if (strcmp(sects[s].sectname, "__module_info") == 0) {
            *size_out = sects[s].size;
            return (const struct legacy_objc_module *)
               ((uintptr_t)sects[s].addr + slide);
         }
      }
   }
   return NULL;
}

/* Public entry: register this image's legacy __OBJC classes with the modern
 * runtime so AppKit/Foundation can message them (principal class, delegates,
 * NSClassFromString, …). The real work lives in reverse_register_image() at
 * the end of this file, where the registry/encoding/reverse-bridge helpers
 * are all in scope. */
static void reverse_register_image(const struct mach_header_64 *mh,
                                   intptr_t slide);
void _86x64_objc_register_classes(const struct mach_header_64 *mh,
                                  intptr_t slide) {
   reverse_register_image(mh, slide);
}

/* ---------------------------------------------------------------------
 * Legacy class-method direct dispatch (the actually-wired path)
 * ---------------------------------------------------------------------
 *
 * Rather than register our binary's legacy classes with the modern runtime
 * (which would force the unsolvable 4-byte-isa/ivar-layout impedance and a
 * reverse IMP marshaller — see the skeleton above), we keep dispatch entirely
 * inside the i386-cdecl world. We index every legacy class by name at image
 * load, and when objc_bridge_prep sees a class message whose receiver name is
 * one of ours, it finds the translated class-method IMP here and hands its
 * address back to the asm trampoline, which resumes the i386 call into it.
 *
 * This only handles CLASS methods (the receiver is a class-name string, so
 * `self` is a class — no instance ivars are touched). Instance dispatch on
 * legacy objects is a separate, harder problem (ivar layout) and is untouched.
 */

#define LEGACY_REG_CAP 8192u   /* power of two */
struct legacy_reg_ent {
   const char *name;                       /* class name cstring (runtime ptr) */
   const struct legacy_objc_class *cls;    /* runtime class struct ptr */
};
static struct legacy_reg_ent g_legacy_reg[LEGACY_REG_CAP];
static uint32_t g_legacy_reg_cnt = 0;   /* distinct classes indexed (for tracing) */

#define LEGACY_CAT_CAP 4096u
static const struct legacy_objc_category *g_legacy_cats[LEGACY_CAT_CAP];
static uint32_t g_legacy_cat_cnt = 0;

/* A legacy metadata pointer is trustworthy only if it is plausibly a real
 * address and actually mapped. The metadata in a 15-year-old i386 binary
 * carries sentinels (0, 1, -1) and the indexer runs inside dyld's add-image
 * callback, so a stray deref would crash the whole process before main(). */
static int ptr_ok(uint32_t p, size_t len) {
   return p >= 0x1000u && mem_readable((uintptr_t)p, len);
}

static uint32_t str_hash(const char *s) {
   uint32_t h = 2166136261u;
   for (; *s; ++s) { h = (h ^ (uint8_t)*s) * 16777619u; }
   return h;
}

static const struct legacy_objc_class *legacy_registry_lookup(const char *name) {
   if (!name) { return NULL; }
   uint32_t i = str_hash(name) & (LEGACY_REG_CAP - 1);
   for (uint32_t n = 0; n < LEGACY_REG_CAP; ++n) {
      const struct legacy_reg_ent *e = &g_legacy_reg[i];
      if (!e->name) { return NULL; }
      if (strcmp(e->name, name) == 0) { return e->cls; }
      i = (i + 1) & (LEGACY_REG_CAP - 1);
   }
   return NULL;
}

static void legacy_registry_insert(const struct legacy_objc_class *cls) {
   if (!ptr_ok(cls->name, 1)) { return; }
   const char *name = (const char *)(uintptr_t)cls->name;
   uint32_t i = str_hash(name) & (LEGACY_REG_CAP - 1);
   for (uint32_t n = 0; n < LEGACY_REG_CAP; ++n) {
      struct legacy_reg_ent *e = &g_legacy_reg[i];
      if (!e->name) { e->name = name; e->cls = cls; ++g_legacy_reg_cnt; return; }
      if (strcmp(e->name, name) == 0) { e->cls = cls; return; }  /* idempotent */
      i = (i + 1) & (LEGACY_REG_CAP - 1);
   }
   /* table full — silently drop (only happens with >8k legacy classes) */
}

/* Scan a single legacy objc_method_list {obsolete, count, methods[count]} for
 * a method whose selector name matches sel_name; return its (slid) IMP or 0. */
static uint64_t scan_one_list(uint32_t list, const char *sel_name) {
   if (!ptr_ok(list, sizeof(struct legacy_objc_method_list))) { return 0; }
   const struct legacy_objc_method_list *ml =
      (const struct legacy_objc_method_list *)(uintptr_t)list;
   if (ml->method_count <= 0 || ml->method_count >= 100000) { return 0; }
   const struct legacy_objc_method *methods =
      (const struct legacy_objc_method *)(ml + 1);
   if (!ptr_ok(list + (uint32_t)sizeof(*ml),
               (size_t)ml->method_count * sizeof(*methods))) { return 0; }
   for (int m = 0; m < ml->method_count; ++m) {
      if (!ptr_ok(methods[m].name, 1)) { continue; }
      const char *mname = (const char *)(uintptr_t)methods[m].name;
      if (getenv("OBJC_BRIDGE_TRACE_METH")) {
         fprintf(stderr, "[bp]     meth \"%s\" imp=0x%x\n", mname, methods[m].imp);
         fflush(stderr);
      }
      if (strcmp(mname, sel_name) == 0) {
         return (uint64_t)(uintptr_t)methods[m].imp;
      }
   }
   return 0;
}

/* The class `methodLists` field has two legacy layouts: the GCC i386 static
 * compiler emits a pointer to ONE objc_method_list directly (obsolete word is
 * 0); after runtime fixup (CLS_NO_METHOD_ARRAY clear) it can instead point to
 * a NULL/(-1)-terminated array of objc_method_list pointers. We never ran the
 * legacy runtime, so it's almost always the direct form, but handle both:
 * if the first word is 0 (an obsolete field, not a list pointer) and the
 * second is a plausible method count, treat it as a single direct list;
 * otherwise treat it as an array of list pointers. */
static uint64_t find_method_in_lists(uint32_t methodLists, const char *sel_name) {
   if (!ptr_ok(methodLists, 2 * sizeof(uint32_t))) { return 0; }
   const uint32_t *w = (const uint32_t *)(uintptr_t)methodLists;
   const int32_t  cnt = (int32_t)w[1];
   if (w[0] == 0 && cnt > 0 && cnt < 100000) {
      return scan_one_list(methodLists, sel_name);   /* direct single list */
   }
   for (size_t k = 0; k < 4096 && w[k] != 0 && w[k] != 0xFFFFFFFFu; ++k) {
      uint64_t imp = scan_one_list(w[k], sel_name);  /* array of list ptrs */
      if (imp) { return imp; }
   }
   return 0;
}

/* Class methods added to a class via a legacy category. */
static uint64_t find_category_class_method(const char *clsname, const char *sel_name) {
   if (!clsname) { return 0; }
   for (uint32_t i = 0; i < g_legacy_cat_cnt; ++i) {
      const struct legacy_objc_category *cat = g_legacy_cats[i];
      if (!ptr_ok(cat->class_name, 1)) { continue; }
      const char *cn = (const char *)(uintptr_t)cat->class_name;
      if (strcmp(cn, clsname) == 0) {
         uint64_t imp = find_method_in_lists(cat->class_methods, sel_name);
         if (imp) { return imp; }
      }
   }
   return 0;
}

static uint64_t legacy_class_method_imp_byname(const char *clsname,
                                               const char *sel_name) {
   if (!clsname || !sel_name) { return 0; }

   /* Walk the class and its legacy-superclass chain, checking each level's
    * metaclass method lists (class methods live in the metaclass) and any
    * categories that add class methods. Stop when the superclass is a
    * framework class (registry miss) — those class methods, if inherited,
    * are a known gap (we can't dispatch +alloc/+class etc. yet). */
   const struct legacy_objc_class *c = legacy_registry_lookup(clsname);
   if (getenv("OBJC_BRIDGE_TRACE")) {
      fprintf(stderr, "[bp]   legacy_lookup(\"%s\",\"%s\"): class=%p isa_ok=%d\n",
              clsname, sel_name, (void*)c,
              c ? ptr_ok(c->isa, sizeof(struct legacy_objc_class)) : 0);
      fflush(stderr);
   }
   const char *cur = clsname;
   for (int depth = 0; depth < 256; ++depth) {
      if (c && ptr_ok(c->isa, sizeof(struct legacy_objc_class))) {
         const struct legacy_objc_class *meta =
            (const struct legacy_objc_class *)(uintptr_t)c->isa;
         if (getenv("OBJC_BRIDGE_TRACE")) {
            const uint32_t *w = ptr_ok(meta->methodLists, 32)
               ? (const uint32_t*)(uintptr_t)meta->methodLists : NULL;
            fprintf(stderr, "[bp]   level %d cur=\"%s\" meta=%p mL=0x%x info=0x%x words=[%x %x %x %x %x %x]\n",
                    depth, cur, (void*)meta, meta->methodLists, meta->info,
                    w?w[0]:0, w?w[1]:0, w?w[2]:0, w?w[3]:0, w?w[4]:0, w?w[5]:0);
            fflush(stderr);
         }
         uint64_t imp = find_method_in_lists(meta->methodLists, sel_name);
         if (imp) { return imp; }
      }
      uint64_t cimp = find_category_class_method(cur, sel_name);
      if (cimp) { return cimp; }
      if (!c || !ptr_ok(c->super_class, 1)) { return 0; }
      cur = (const char *)(uintptr_t)c->super_class;
      c = legacy_registry_lookup(cur);
      if (!c) { return 0; }
   }
   return 0;
}

/* Thin wrapper: the i386 receiver slot (args32[0]) for a message to one of our
 * OWN legacy classes is a __cls_refs class-name cstring pointer. Validate it
 * and dispatch by name. (When the receiver is instead a proxy handle — e.g. a
 * Class obtained from `+class` — the caller derives the name via
 * class_getName() and calls legacy_class_method_imp_byname directly.) */
static uint64_t legacy_class_method_imp(uint32_t self32, const char *sel_name) {
   if (self32 == 0 || !sel_name) { return 0; }
   if (!mem_readable((uintptr_t)self32, 1)) { return 0; }
   return legacy_class_method_imp_byname((const char *)(uintptr_t)self32,
                                         sel_name);
}

/* Public entry: index every legacy class/category in the image into the
 * registry above. Called from objc_slide.c's add-image callback AFTER the
 * __OBJC slide-fixup, so every metadata pointer is a correct runtime address.
 * Idempotent: re-indexing the same image overwrites in place (the N co-located
 * libabiconv copies each run their own add-image callback). */
void _86x64_objc_index_legacy_classes(const struct mach_header_64 *mh,
                                      intptr_t slide) {
   if (!mh) { return; }
   size_t modinfo_size = 0;
   const struct legacy_objc_module *modules =
      find_module_info(mh, slide, &modinfo_size);
   if (!modules) { return; }
   const size_t nmodules = modinfo_size / sizeof(struct legacy_objc_module);
   const uint32_t before = g_legacy_reg_cnt;

   for (size_t i = 0; i < nmodules; ++i) {
      const struct legacy_objc_module *mod = &modules[i];
      if (mod->version != 7 || mod->size != sizeof(struct legacy_objc_module)) {
         continue;
      }
      if (!ptr_ok(mod->symtab, sizeof(struct legacy_objc_symtab))) { continue; }
      const struct legacy_objc_symtab *symtab =
         (const struct legacy_objc_symtab *)(uintptr_t)mod->symtab;
      const uint32_t ndefs =
         (uint32_t)symtab->cls_def_cnt + (uint32_t)symtab->cat_def_cnt;
      if (!ptr_ok(mod->symtab + (uint32_t)sizeof(*symtab),
                  (size_t)ndefs * sizeof(uint32_t))) { continue; }
      const uint32_t *defs = (const uint32_t *)
         ((const char *)symtab + sizeof(struct legacy_objc_symtab));

      for (uint16_t j = 0; j < symtab->cls_def_cnt; ++j) {
         if (!ptr_ok(defs[j], sizeof(struct legacy_objc_class))) { continue; }
         legacy_registry_insert(
            (const struct legacy_objc_class *)(uintptr_t)defs[j]);
      }
      for (uint16_t j = 0; j < symtab->cat_def_cnt; ++j) {
         uint32_t cref = defs[symtab->cls_def_cnt + j];
         if (!ptr_ok(cref, sizeof(struct legacy_objc_category))) { continue; }
         const struct legacy_objc_category *cat =
            (const struct legacy_objc_category *)(uintptr_t)cref;
         /* dedup by pointer (idempotent across repeated callbacks) */
         int seen = 0;
         for (uint32_t k = 0; k < g_legacy_cat_cnt; ++k) {
            if (g_legacy_cats[k] == cat) { seen = 1; break; }
         }
         if (!seen && g_legacy_cat_cnt < LEGACY_CAT_CAP) {
            g_legacy_cats[g_legacy_cat_cnt++] = cat;
         }
      }
   }

   if (getenv("ABICONV_OBJC_SLIDE_VERBOSE") && g_legacy_reg_cnt != before) {
      fprintf(stderr, "objc_shim: indexed %u legacy classes (+%u this image), "
              "%u categories total\n", g_legacy_reg_cnt,
              g_legacy_reg_cnt - before, g_legacy_cat_cnt);
      fflush(stderr);
   }
}

/*
 * External ObjC-object DATA-constant shadows.
 *
 * An i386 reference to a Foundation data constant such as
 * `NSString * const NSArgumentDomain` compiles to a non-lazy symbol pointer
 * the linker fills with the symbol's address, followed by a double-deref in
 * code: `movl slot,%reg` (-> &NSArgumentDomain) then `movl (%reg),%reg`
 * (-> the NSString*). On x86_64 both the variable's address AND the object
 * pointer are 64-bit, so the i386 4-byte `movl` truncates them -> bad deref.
 *
 * abigen emits, for each such constant in the consider set, a low-4GB shadow
 * variable ___SYM plus an entry in the table below {&___SYM, "SYM"}.
 * static-interpose rewrites the binary's non-lazy bind for _SYM to point at
 * ___SYM (a low libabiconv __DATA address). This ctor populates each shadow
 * with a 32-bit proxy HANDLE wrapping the real 64-bit object, so the i386
 * double-deref (slot -> &___SYM -> handle) yields a usable id that the objc
 * bridge unwraps on the next message send.
 *
 * The table/count symbols are always defined by abigen (possibly empty).
 * Runs once per libabiconv copy; x64_objc_wrap uses the process-shared arena,
 * so handles minted here are valid across all copies.
 */
extern void    *x64_data_shadows[];      /* flat: [&shadow0,name0, &shadow1,name1,...] */
extern uint64_t x64_data_shadows_count;  /* number of PAIRS */

__attribute__((constructor))
static void x64_init_data_shadows(void) {
   const uint64_t n = x64_data_shadows_count;
   for (uint64_t i = 0; i < n; ++i) {
      uint64_t *shadow = (uint64_t *)x64_data_shadows[2 * i];
      const char *name = (const char *)x64_data_shadows[2 * i + 1];
      if (!shadow || !name) { continue; }
      void *addr = dlsym(RTLD_DEFAULT, name);   /* &realvar */
      if (!addr) { continue; }
      uint64_t v = *(uint64_t *)addr;           /* the object pointer (or scalar) */
      if (v >= 0x100000000ULL) {
         *shadow = x64_objc_wrap(v);            /* 32-bit handle, zero-extended */
      } else {
         *shadow = (uint32_t)v;                 /* already low; pass through */
      }
   }
   if (getenv("ABICONV_OBJC_SLIDE_VERBOSE")) {
      fprintf(stderr, "objc_shim: populated %llu data-constant shadows\n",
              (unsigned long long)n);
      fflush(stderr);
   }
}

/* ======================================================================
 * Reverse bridge: modern x86_64 ObjC runtime -> translated i386 legacy IMP.
 *
 * Registration (reverse_register_image) creates a real modern class for each
 * legacy __OBJC class, with the SINGLE generic IMP _86x64_reverse_imp
 * (objc_reverse.asm) for every method. The legacy class's instance ivars are
 * NOT added to the modern class — they live in a low-4GB i386-layout SHADOW
 * buffer per instance, so the translated IMPs (which access ivars at hardcoded
 * i386 offsets) read/write the shadow, while the real modern instance carries
 * the superclass (e.g. NSApplication) state for AppKit/[super ...].
 * ====================================================================== */

#include <objc/message.h>

struct reverse_plan {
   uint64_t legacy_imp;
   uint64_t lowstack_top;
   uint64_t lowstack_base;
   int32_t  ret_kind;        /* 0 scalar, 1 object(unwrap), 2 void */
   uint32_t frame_words;
   uint32_t frame[64];
};

extern void _86x64_reverse_imp(void);   /* objc_reverse.asm */

/* ---- (lookup_class, sel) -> legacy method map. lookup_class is
 * object_getClass(self): the registered class for instance methods, its
 * metaclass for class methods. ---- */
#define RMETH_CAP 32768u   /* power of two */
struct rmeth_ent { Class cls; SEL sel; uint64_t imp; const char *types; };
static struct rmeth_ent g_rmeth[RMETH_CAP];
static uint32_t rmeth_hash(Class c, SEL s) {
   uint64_t h = (uint64_t)(uintptr_t)c * 1099511628211ULL ^ (uint64_t)(uintptr_t)s;
   return (uint32_t)((h ^ (h >> 32)) & (RMETH_CAP - 1));
}
static void rmeth_insert(Class c, SEL s, uint64_t imp, const char *types) {
   uint32_t i = rmeth_hash(c, s);
   for (uint32_t n = 0; n < RMETH_CAP; ++n) {
      if (g_rmeth[i].imp == 0 || (g_rmeth[i].cls == c && g_rmeth[i].sel == s)) {
         g_rmeth[i].cls = c; g_rmeth[i].sel = s;
         g_rmeth[i].imp = imp; g_rmeth[i].types = types; return;
      }
      i = (i + 1) & (RMETH_CAP - 1);
   }
}
static struct rmeth_ent *rmeth_lookup(Class c, SEL s) {
   uint32_t i = rmeth_hash(c, s);
   for (uint32_t n = 0; n < RMETH_CAP; ++n) {
      if (g_rmeth[i].imp == 0) { return NULL; }
      if (g_rmeth[i].cls == c && g_rmeth[i].sel == s) { return &g_rmeth[i]; }
      i = (i + 1) & (RMETH_CAP - 1);
   }
   return NULL;
}

/* ---- thread-local super-dispatch hint (see objc_bridge_prep_super) ----
 * A legacy `[super sel]` resolves (via objc_msgSendSuper) to the SUPER's
 * reverse_imp method, but reverse_prep would otherwise re-derive the method
 * from object_getClass(self) = the RECEIVER's most-derived class and run the
 * subclass override again — infinite recursion when sub and super both define
 * sel (the iPhoto 10th blocker: +[IPPublishPluginManager initialize] calling
 * [super initialize] where IP_IPHPluginManager also defines +initialize).
 * objc_bridge_prep_super records the resolved super lookup-class here ONLY
 * when that super method is itself a legacy method; the very next reverse_prep
 * for the same (self,sel) consumes it. Thread-local + consume-once makes it
 * exact: the super-send synchronously dispatches straight into reverse_imp on
 * the same thread, with no intervening reverse_prep. */
static uint64_t super_hint_tid(void) {
   uint64_t t = 0;
   pthread_threadid_np(NULL, &t);
   return t ? t : 1;
}

/* Record the resolved super lookup-class for the calling thread. No local
 * rmeth gating here: this runs in the caller's copy whose g_rmeth may be empty
 * (cross-copy). reverse_prep (registering copy) validates against the real map
 * and falls back if the hint doesn't name a legacy method. */
static void reverse_note_super(id recv, SEL sel, Class super_lookup) {
   arena_init();
   if (!g_ctrl || !super_lookup) { return; }
   uint64_t tid = super_hint_tid();
   unsigned i = (unsigned)(tid % SUPER_HINT_SLOTS);
   g_ctrl->super_hints[i].recv = (uint64_t)(uintptr_t)recv;
   g_ctrl->super_hints[i].sel = (uint64_t)(uintptr_t)sel;
   g_ctrl->super_hints[i].lookup = (uint64_t)(uintptr_t)super_lookup;
   g_ctrl->super_hints[i].tid = tid;
   g_ctrl->super_hints[i].valid = 1;
   if (getenv("OBJC_SUPER_TRACE")) {
      fprintf(stderr, "[note_super] tid=%llu recv=%p sel=%s super_lookup=%s\n",
              (unsigned long long)tid, (void*)recv, sel_getName(sel),
              class_getName(super_lookup));
   }
}

/* Consume this thread's super hint if it matches (self,sel). Returns the
 * super lookup-class (validated to name a legacy method) or NULL. */
static Class reverse_take_super(id self_, SEL sel) {
   if (!g_ctrl) { return NULL; }
   uint64_t tid = super_hint_tid();
   unsigned i = (unsigned)(tid % SUPER_HINT_SLOTS);
   if (!g_ctrl->super_hints[i].valid ||
       g_ctrl->super_hints[i].tid != tid ||
       g_ctrl->super_hints[i].recv != (uint64_t)(uintptr_t)self_ ||
       g_ctrl->super_hints[i].sel != (uint64_t)(uintptr_t)sel) {
      return NULL;
   }
   g_ctrl->super_hints[i].valid = 0;   /* consume-once */
   return (Class)(uintptr_t)g_ctrl->super_hints[i].lookup;
}

/* ---- registered Class -> legacy class metadata addr + instance_size ---- */
#define RCLS_CAP 8192u
struct rcls_ent { Class cls; uint32_t legacy_addr; uint32_t instance_size; };
static struct rcls_ent g_rcls[RCLS_CAP];
static uint32_t rcls_hash(Class c) {
   uint64_t h = (uint64_t)(uintptr_t)c * 2654435761ULL;
   return (uint32_t)((h ^ (h >> 32)) & (RCLS_CAP - 1));
}
static void rcls_insert(Class c, uint32_t addr, uint32_t isz) {
   uint32_t i = rcls_hash(c);
   for (uint32_t n = 0; n < RCLS_CAP; ++n) {
      if (!g_rcls[i].cls || g_rcls[i].cls == c) {
         g_rcls[i].cls = c; g_rcls[i].legacy_addr = addr;
         g_rcls[i].instance_size = isz; return;
      }
      i = (i + 1) & (RCLS_CAP - 1);
   }
}
static struct rcls_ent *rcls_lookup(Class c) {
   uint32_t i = rcls_hash(c);
   for (uint32_t n = 0; n < RCLS_CAP; ++n) {
      if (!g_rcls[i].cls) { return NULL; }
      if (g_rcls[i].cls == c) { return &g_rcls[i]; }
      i = (i + 1) & (RCLS_CAP - 1);
   }
   return NULL;
}

/* ---- i386 shadow arena: per legacy-class instance, a low-4GB i386-layout
 * buffer preceded by an 8-byte header holding the real object pointer, so the
 * forward bridge can map a shadow address back to its real object. ---- */
/* The shadow arena lives in the shared objc_shared_ctrl (see arena_init) so
 * every libabiconv copy agrees on its extent. */
static void shadow_arena_init(void) {
   arena_init();                        /* ensure g_ctrl is attached/created */
   if (!g_ctrl || g_ctrl->shadow_base) { return; }
   const size_t SZ = 64u * 1024u * 1024u;   /* 64MB i386 instance space */
   for (uintptr_t a = 0x90000000UL; a + SZ <= 0xF0000000UL; a += SZ) {
      mach_vm_address_t addr = a;
      if (mach_vm_allocate(mach_task_self(), &addr, SZ, VM_FLAGS_FIXED)
          == KERN_SUCCESS) {
         g_ctrl->shadow_base = (uint64_t)addr;
         g_ctrl->shadow_cur  = (uint64_t)addr;
         g_ctrl->shadow_end  = (uint64_t)addr + SZ;
         return;
      }
   }
   fprintf(stderr, "objc_shim: no low-4GB region for shadow arena\n");
}
static int is_shadow(uint32_t s) {
   if (!g_ctrl || !s) { return 0; }
   return (uint64_t)s >= g_ctrl->shadow_base + 8 && (uint64_t)s < g_ctrl->shadow_cur;
}
static id shadow_real(uint32_t s) {
   return is_shadow(s) ? *(id *)((uintptr_t)s - 8) : (id)0;
}
static uint32_t get_or_create_shadow(id real, Class cls) {
   static char assoc_key;
   uint32_t s = (uint32_t)(uintptr_t)objc_getAssociatedObject(real, &assoc_key);
   if (s) { return s; }
   shadow_arena_init();
   if (!g_ctrl || !g_ctrl->shadow_base) { return 0; }
   struct rcls_ent *ce = rcls_lookup(cls);
   uint32_t isz = ce ? ce->instance_size : 64;
   uint32_t isa = ce ? ce->legacy_addr : 0;
   if (isz < 4) { isz = 4; }
   size_t need = 8 + ((isz + 15) & ~(size_t)15);
   if (g_ctrl->shadow_cur + need > g_ctrl->shadow_end) { return 0; }
   uintptr_t hdr = (uintptr_t)g_ctrl->shadow_cur;
   g_ctrl->shadow_cur += need;
   *(uint64_t *)hdr = (uint64_t)real;
   uintptr_t s2 = hdr + 8;
   memset((void *)s2, 0, isz);
   *(uint32_t *)s2 = isa;          /* i386 object's isa slot */
   s = (uint32_t)s2;
   objc_setAssociatedObject(real, &assoc_key, (id)(uintptr_t)s,
                            OBJC_ASSOCIATION_ASSIGN);
   return s;
}

/* Public: map a shadow self32 back to its real object (forward bridge). 0 if
 * not a shadow. */
id _86x64_shadow_real(uint32_t s) { return shadow_real(s); }

/* ---- ObjC type-encoding scanners ---- */
static const char *enc_skip_quals(const char *t) {
   while (*t && strchr("rnNoORV", *t)) { ++t; }
   return t;
}
/* Advance past ONE complete type (handles ^ptr, {struct}, [array], (union),
 * bN bitfield). Does not skip trailing offset digits. */
static const char *enc_skip_type(const char *t) {
   t = enc_skip_quals(t);
   if (!*t) { return t; }
   char c = *t++;
   switch (c) {
   case '^': return enc_skip_type(t);        /* pointer-to */
   case 'b': while (*t >= '0' && *t <= '9') { ++t; } return t;  /* bitfield */
   case '{': case '[': case '(': {
      char close = (c == '{') ? '}' : (c == '[') ? ']' : ')';
      int depth = 1;
      while (*t && depth) { if (*t == c) { ++depth; } else if (*t == close) { --depth; } ++t; }
      return t;
   }
   default: return t;
   }
}
static const char *enc_skip_digits(const char *t) {
   while (*t >= '0' && *t <= '9') { ++t; }
   return t;
}

/* ---- the C prep called by _86x64_reverse_imp ---- */
#define REV_STACK_SZ (512u * 1024u)

void _86x64_reverse_prep(struct reverse_plan *plan, const uint64_t *regs) {
   id self_ = (id)regs[0];
   SEL sel  = (SEL)regs[1];

   plan->ret_kind = 2;          /* default void */
   plan->frame_words = 2;
   plan->frame[0] = 0;
   plan->frame[1] = (uint32_t)(uintptr_t)sel;
   plan->lowstack_base = (uint64_t)(uintptr_t)malloc(REV_STACK_SZ);  /* low-4GB */
   plan->lowstack_top  = (plan->lowstack_base + REV_STACK_SZ) & ~(uint64_t)0xf;

   /* A pending super-dispatch hint for exactly this (self,sel) overrides the
    * derived-class lookup so [super sel] runs the SUPER's legacy method, not
    * the receiver's override again (consume-once). */
   /* A pending super-dispatch hint for this (self,sel) routes the lookup to the
    * SUPER's legacy method instead of the receiver's derived override (prevents
    * +initialize-style recursion). Validate it against the authoritative map;
    * fall back to the derived class if it doesn't name a legacy method. */
   Class lookup = object_getClass(self_);
   struct rmeth_ent *m = NULL;
   Class hint = reverse_take_super(self_, sel);
   if (hint) {
      struct rmeth_ent *hm = rmeth_lookup(hint, sel);
      if (hm) {
         lookup = hint;
         m = hm;
         if (getenv("OBJC_SUPER_TRACE"))
            fprintf(stderr, "[prep] HINT HIT self=%p sel=%s -> %s\n",
                    (void*)self_, sel_getName(sel), class_getName(lookup));
      }
   }
   if (!m) { m = rmeth_lookup(lookup, sel); }
   if (!m) {
      /* No legacy method — should not happen (we only register methods we map).
       * Leave legacy_imp=0; asm will jmp to 0. Guard by returning a benign nil.
       * Better: abort loudly so the gap is visible during bring-up. */
      fprintf(stderr, "objc_shim: reverse_prep: no legacy method for %s[%s]\n",
              class_getName(lookup), sel_getName(sel));
      plan->legacy_imp = 0;
      return;
   }
   plan->legacy_imp = m->imp;

   /* self32: a class message wraps the modern Class as a proxy handle (so the
    * IMP's `[self ...]` resolves back through resolve_self -> unwrap -> Class ->
    * reverse bridge); an instance message uses the i386 shadow (so the IMP's
    * ivar accesses at hardcoded i386 offsets land in the shadow buffer). */
   if (object_isClass(self_)) {
      plan->frame[0] = x64_objc_wrap((uint64_t)(uintptr_t)self_);
   } else {
      plan->frame[0] = get_or_create_shadow(self_, lookup);
   }

   /* Return kind from the encoding's leading type. */
   char rb = encoding_base_type(m->types);
   if (rb == 'v') { plan->ret_kind = 2; }
   else if (rb == '@' || rb == '#') { plan->ret_kind = 1; }
   else { plan->ret_kind = 0; }

   /* Walk arg types: skip ret+offset, self(@)+offset, cmd(:)+offset, then map
    * each explicit arg from the x86_64 GP regs to a 4-byte i386 slot. */
   const char *t = m->types;
   t = enc_skip_digits(enc_skip_type(t));    /* return type */
   t = enc_skip_digits(enc_skip_type(t));    /* self @ */
   t = enc_skip_digits(enc_skip_type(t));    /* _cmd : */
   unsigned w = 2;        /* next frame word */
   unsigned gp = 2;       /* next GP reg index (regs[2]=rdx ... regs[5]=r9) */
   while (*t && w < 60) {
      char b = encoding_base_type(t);
      uint64_t v = (gp < 6) ? regs[gp]
                   : *(const uint64_t *)((uintptr_t)regs[6] + 8 * (gp - 6));
      if (b == '@' || b == '#') {
         plan->frame[w++] = x64_objc_wrap(v);
      } else if (b == 'q' || b == 'Q') {
         plan->frame[w++] = (uint32_t)v;
         plan->frame[w++] = (uint32_t)(v >> 32);
      } else if (b == 'f' || b == 'd') {
         /* float/double live in XMM regs we don't capture — pass 0 + warn. */
         if (getenv("OBJC_BRIDGE_TRACE")) {
            fprintf(stderr, "objc_shim: reverse_prep: FP arg unsupported in %s\n",
                    sel_getName(sel));
         }
         plan->frame[w++] = 0;
      } else {
         plan->frame[w++] = (uint32_t)v;
      }
      gp++;
      t = enc_skip_digits(enc_skip_type(t));
   }
   plan->frame_words = w;

   if (getenv("OBJC_BRIDGE_TRACE")) {
      fprintf(stderr, "[rev] %s[%s] imp=0x%llx self32=0x%x words=%u kind=%d\n",
              class_getName(lookup), sel_getName(sel),
              (unsigned long long)m->imp, plan->frame[0], plan->frame_words,
              plan->ret_kind);
      fflush(stderr);
   }
}

uint64_t _86x64_reverse_ret(struct reverse_plan *plan, uint32_t eax) {
   uint64_t r;
   if (plan->ret_kind == 1) { r = x64_objc_unwrap(eax); }   /* object */
   else if (plan->ret_kind == 2) { r = 0; }                 /* void */
   else { r = (uint64_t)eax; }                              /* scalar */
   if (plan->lowstack_base) { free((void *)(uintptr_t)plan->lowstack_base); }
   return r;
}

/* ---- registration ---- */

/* Enumerate one legacy class's method lists (handles both the single-direct-
 * list and array-of-list-ptrs layouts, like find_method_in_lists) and add each
 * to target_class via class_addMethod(_86x64_reverse_imp), recording the legacy
 * IMP in the (target_class,sel) map. */
static void reverse_add_methods(Class target, uint32_t methodLists) {
   if (!ptr_ok(methodLists, 8)) { return; }
   const uint32_t *word = (const uint32_t *)(uintptr_t)methodLists;
   /* single direct list iff word[0]==0 (obsolete) && word[1] plausible count */
   int single = (word[0] == 0 && (int32_t)word[1] > 0 && word[1] < 100000);
   const uint32_t *lists; uint32_t nlists; uint32_t single_arr[1];
   if (single) {
      single_arr[0] = methodLists; lists = single_arr; nlists = 1;
   } else {
      /* NULL/-1 terminated array of method_list ptrs */
      lists = word;
      nlists = 0;
      while (nlists < 4096 && ptr_ok((uint32_t)(uintptr_t)&lists[nlists], 4)
             && lists[nlists] != 0 && lists[nlists] != 0xFFFFFFFFu) { ++nlists; }
   }
   for (uint32_t li = 0; li < nlists; ++li) {
      uint32_t mlp = lists[li];
      if (!ptr_ok(mlp, sizeof(struct legacy_objc_method_list))) { continue; }
      const struct legacy_objc_method_list *ml =
         (const struct legacy_objc_method_list *)(uintptr_t)mlp;
      int32_t cnt = ml->method_count;
      if (cnt <= 0 || cnt > 100000) { continue; }
      const struct legacy_objc_method *meth = (const struct legacy_objc_method *)
         ((const char *)ml + sizeof(struct legacy_objc_method_list));
      for (int32_t k = 0; k < cnt; ++k) {
         if (!ptr_ok(meth[k].name, 1) || meth[k].imp == 0) { continue; }
         const char *sname = (const char *)(uintptr_t)meth[k].name;
         const char *types = ptr_ok(meth[k].types, 1)
            ? (const char *)(uintptr_t)meth[k].types : "v8@0:4";
         SEL sel = sel_registerName(sname);
         if (class_addMethod(target, sel, (IMP)_86x64_reverse_imp, types)) {
            rmeth_insert(target, sel, meth[k].imp, types);
         } else {
            /* already present (e.g. an override added by a later list); still
             * record the legacy imp so the reverse bridge can find it */
            rmeth_insert(target, sel, meth[k].imp, types);
         }
      }
   }
}

/* Register one legacy class. Returns 1 if registered, 0 if deferred (super not
 * ready), -1 if skipped (collision / bad). */
static int reverse_register_one(const struct legacy_objc_class *cls) {
   if (!ptr_ok(cls->name, 1) || !ptr_ok(cls->super_class, 1)) { return -1; }
   const char *name = (const char *)(uintptr_t)cls->name;
   if (objc_getClass(name)) { return -1; }   /* name already exists — skip */
   const char *supername = (const char *)(uintptr_t)cls->super_class;
   Class super = objc_getClass(supername);
   if (!super) { return 0; }                  /* super not registered yet */
   Class newcls = objc_allocateClassPair(super, name, 0);
   if (!newcls) { return -1; }
   /* instance methods */
   reverse_add_methods(newcls, cls->methodLists);
   /* class methods live on the metaclass; legacy metaclass = cls->isa */
   if (ptr_ok(cls->isa, sizeof(struct legacy_objc_class))) {
      const struct legacy_objc_class *meta =
         (const struct legacy_objc_class *)(uintptr_t)cls->isa;
      Class newmeta = object_getClass((id)newcls);
      reverse_add_methods(newmeta, meta->methodLists);
   }
   objc_registerClassPair(newcls);
   rcls_insert(newcls, (uint32_t)(uintptr_t)cls, cls->instance_size);
   return 1;
}

static void reverse_register_image(const struct mach_header_64 *mh,
                                   intptr_t slide) {
   if (!mh) { return; }
   size_t modinfo_size = 0;
   const struct legacy_objc_module *modules =
      find_module_info(mh, slide, &modinfo_size);
   if (!modules) { return; }
   const size_t nmodules = modinfo_size / sizeof(struct legacy_objc_module);

   /* collect class defs */
   enum { CAP = 4096 };
   const struct legacy_objc_class *defs[CAP];
   size_t ndefs = 0;
   for (size_t i = 0; i < nmodules; ++i) {
      const struct legacy_objc_module *mod = &modules[i];
      if (mod->version != 7 || mod->size != sizeof(*mod)) { continue; }
      if (!ptr_ok(mod->symtab, sizeof(struct legacy_objc_symtab))) { continue; }
      const struct legacy_objc_symtab *st =
         (const struct legacy_objc_symtab *)(uintptr_t)mod->symtab;
      const uint32_t *cdefs = (const uint32_t *)
         ((const char *)st + sizeof(struct legacy_objc_symtab));
      for (uint16_t j = 0; j < st->cls_def_cnt && ndefs < CAP; ++j) {
         if (!ptr_ok(cdefs[j], sizeof(struct legacy_objc_class))) { continue; }
         defs[ndefs++] = (const struct legacy_objc_class *)(uintptr_t)cdefs[j];
      }
   }

   /* iterate to fixpoint so superclasses register before subclasses */
   char done[CAP]; memset(done, 0, ndefs);
   uint32_t registered = 0;
   for (int pass = 0; pass < 64; ++pass) {
      int progress = 0;
      for (size_t i = 0; i < ndefs; ++i) {
         if (done[i]) { continue; }
         int r = reverse_register_one(defs[i]);
         if (r != 0) { done[i] = 1; progress = 1; if (r == 1) { ++registered; } }
      }
      if (!progress) { break; }
   }

   if (getenv("ABICONV_OBJC_SLIDE_VERBOSE")) {
      fprintf(stderr, "objc_shim: registered %u/%zu legacy classes\n",
              registered, ndefs);
      fflush(stderr);
   }
}
