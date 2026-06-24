#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "CoreServices", s); }

long FSMakeFSSpec(long a, long b, long c_, long d, long e, long f) { shim_note("FSMakeFSSpec called (auto-stub)"); return 0; }

long FSMatchAliasNoUI(long a, long b, long c_, long d, long e, long f) { shim_note("FSMatchAliasNoUI called (auto-stub)"); return 0; }

long FSpMakeFSRef(long a, long b, long c_, long d, long e, long f) { shim_note("FSpMakeFSRef called (auto-stub)"); return 0; }

long PBCatSearchSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBCatSearchSync called (auto-stub)"); return 0; }

long PBGetCatInfoSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBGetCatInfoSync called (auto-stub)"); return 0; }

long PBHGetFInfoSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBHGetFInfoSync called (auto-stub)"); return 0; }

long PBHGetVolParmsSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBHGetVolParmsSync called (auto-stub)"); return 0; }

long PBHGetVolSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBHGetVolSync called (auto-stub)"); return 0; }

long ResolveAlias(long a, long b, long c_, long d, long e, long f) { shim_note("ResolveAlias called (auto-stub)"); return 0; }
