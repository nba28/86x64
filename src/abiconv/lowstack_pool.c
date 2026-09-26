/*
 * lowstack_pool.c — per-thread cache of the low-4GB stacks that native->i386
 * entries (C callbacks, Apple Event handlers, CGFunction callbacks, blocks,
 * reverse-bridge IMPs) run the translated code on.
 *
 * Each entry used to malloc a fresh stack (up to 4 MB) from the shim heap and
 * free it on return — once per socket packet, timer tick or drawRect:. That is
 * heap churn on the hottest bridge paths, and before 06b862e it leaked 4 MB per
 * callback (Quinn's server exhausted the low-4GB heap in minutes).
 *
 * Stacks are kept per size, per thread, LIFO, and only ever reused as a stack of
 * the same size: a just-returned frame can still be referenced for an instant
 * (an out-param written back into it, a pointer into it returned), so a retired
 * stack must never go back to the general heap while that window is open.
 * At most LS_CACHE_BYTES per size are cached per thread; the rest are freed.
 * A thread-exit destructor frees what the thread still holds.
 * Kill switch ABICONV_NO_LOWSTACK_CACHE=1: plain malloc/free per entry.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#define LS_CLASSES     4
#define LS_DEPTH_MAX   128
#define LS_CACHE_BYTES (32u << 20)

struct ls_class { size_t sz; unsigned n; void *stk[LS_DEPTH_MAX]; };
struct ls_tls   { struct ls_class c[LS_CLASSES]; };

static pthread_key_t  g_ls_key;
static pthread_once_t g_ls_once = PTHREAD_ONCE_INIT;
static int            g_ls_off = -1;

static void ls_dtor(void *v) {
   struct ls_tls *t = v;
   for (unsigned i = 0; i < LS_CLASSES; ++i) {
      while (t->c[i].n) { free(t->c[i].stk[--t->c[i].n]); }
   }
   free(t);
}
static void ls_init(void) {
   pthread_key_create(&g_ls_key, ls_dtor);
   g_ls_off = getenv("ABICONV_NO_LOWSTACK_CACHE") != NULL;
}

/* The thread's pool class for `sz`, or NULL (cache off, or all classes taken). */
static struct ls_class *ls_class_for(size_t sz) {
   pthread_once(&g_ls_once, ls_init);
   if (g_ls_off) { return NULL; }
   struct ls_tls *t = pthread_getspecific(g_ls_key);
   if (!t) {
      t = calloc(1, sizeof *t);
      if (!t) { return NULL; }
      pthread_setspecific(g_ls_key, t);
   }
   for (unsigned i = 0; i < LS_CLASSES; ++i) {
      if (t->c[i].sz == sz) { return &t->c[i]; }
      if (t->c[i].sz == 0) { t->c[i].sz = sz; return &t->c[i]; }
   }
   return NULL;
}

void *x64_lowstack_get(size_t sz) {
   struct ls_class *c = ls_class_for(sz);
   if (c && c->n) { return c->stk[--c->n]; }
   return malloc(sz);                        /* shim malloc -> low-4GB */
}

void x64_lowstack_put(void *p, size_t sz) {
   if (!p) { return; }
   struct ls_class *c = ls_class_for(sz);
   unsigned cap = LS_CACHE_BYTES / sz;
   if (cap < 2) { cap = 2; }
   if (cap > LS_DEPTH_MAX) { cap = LS_DEPTH_MAX; }
   if (c && c->n < cap) { c->stk[c->n++] = p; return; }
   free(p);
}
