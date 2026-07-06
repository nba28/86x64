/*
 * 81_static_init_default — translated __DATA,__mod_init_func static
 * initializers must run BY DEFAULT (no ABICONV_RUN_INITS in the environment).
 *
 * libabiconv's wrap_mod_init_funcs has two strategies. The legacy stub path
 * rewrites each __mod_init_func slot to an out-of-image JIT stub for dyld to
 * call — but modern dyld4 VALIDATES that initializer pointers land inside the
 * image and SILENTLY SKIPS our stubs, so on every current macOS that path runs
 * ZERO initializers. The run-now path (collect + NULL the slot + run at the
 * end of slide_objc on the low-4GB init stack) works everywhere but was gated
 * behind an opt-in ABICONV_RUN_INITS env var that only hand-written launch
 * scripts set.
 *
 * Civ IV s26 (2026-07-06): launched via `m64 run` (no env), ALL 1063 of the
 * game's static ctors were silently skipped; the first hard use of a
 * never-constructed static (GameRanger MSG_Mac's std::set sCallbackList,
 * main→CheckPreferences→InitGameRanger→AddPeriodicCallback→insert) crashed in
 * _Rb_tree_decrement reading header.parent(=NULL)->parent at address 0x4.
 *
 * This test is the missing default-path coverage (the only other mod-init
 * fixture, dyld_multicopy, sets ABICONV_RUN_INITS=1 explicitly): two
 * constructor-attribute initializers — exactly the GCC __GLOBAL__I shape, an
 * S_MOD_INIT_FUNC_POINTERS section — must have run, in order, before main.
 * The harness intentionally sets NO abiconv env vars. Exits 42 on success;
 * 1 = neither ran (the dyld4-skip bug), 2/3 = partial/misordered.
 */

extern void exit(int status);

static int g_a = 0;
static int g_b = 0;

/* Mirror the Civ shape: the ctor CONSTRUCTS state main depends on (not just a
 * flag) — a tiny linked chain main must walk. */
struct node { int v; struct node *next; };
static struct node n2 = { 2, 0 };
static struct node n1 = { 1, 0 };
static struct node *g_head = 0;

__attribute__((constructor)) static void init_a(void) {
   g_a = 1;
   n1.next = &n2;
   g_head = &n1;
}

__attribute__((constructor)) static void init_b(void) {
   /* runs after init_a (mod_init order) */
   g_b = (g_a == 1) ? 1 : -1;
}

int main(void) {
   if (g_a == 0 && g_b == 0) { exit(1); }   /* no initializer ran at all */
   if (g_a != 1) { exit(2); }
   if (g_b != 1) { exit(3); }               /* -1 => wrong order */

   /* walk the ctor-built chain: 1 + 2 + 39 = 42 */
   int sum = 39;
   for (const struct node *p = g_head; p; p = p->next) { sum += p->v; }
   exit(sum);
}
