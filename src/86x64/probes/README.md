# Native capability probes (x86_64, run under Rosetta)

Small standalone programs that answer "what does this modern macOS actually
provide?" without involving the translator at all. Written because several
long-standing beliefs in memory turned out to be wrong, each time because a
framework's *on-disk* stub was mistaken for the framework being absent — the
real code lives in the dyld shared cache, so **`dlopen` + `dlsym` is the only
authoritative test**.

Build any of them:

    clang -arch x86_64 -Wno-deprecated-declarations \
        -framework OpenGL -framework ApplicationServices -o /tmp/p cglprobe.c

- `cglprobe.c`  — what Halo's GPU capability check sees: enumerates CGL
  renderers and dumps `kCGLRPAccelerated` (0x49) and the deprecated
  `kCGLRPVideoMemory` (0x78), the two properties 2006-era code tests.
  Result on M4 Pro: renderer 0 is accelerated with VideoMemory clamped to
  INT_MAX (18186 MB) — **the GPU check passes**, so "the renderer check fails"
  is NOT why Carbon games bail.

- `wpprobe2.c`  — presence of the Carbon/AGL entry points, with the frameworks
  explicitly `dlopen`ed first (⚠ without that, `RTLD_DEFAULT` finds nothing and
  everything looks absent — an easy false negative).
  Result: `GetWindowPort`/`GetWindowRef` are genuinely **gone**; `HIViewGetRoot`,
  `HIWindowGetCGWindowID` and **all of AGL** are **present**.

- `aglattach.c` — the decisive one. Creates a real compositing window and shows
  that the classic idiom `aglSetDrawable(ctx, GetWindowPort(win))` cannot work
  (the port is gone, so the drawable is NULL and the call returns 0), while
  `aglSetWindowRef(ctx, win)` succeeds and gives a live accelerated context
  (`GL_RENDERER = Apple M4 Pro`, `GL_VERSION = 2.1 Metal`). That is the
  replacement path for every translated Carbon+AGL app.

See the Halo target notes for how this reframed the Halo blocker.
