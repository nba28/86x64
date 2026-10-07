# Probes

Small standalone programs and scripts, each written to answer **one question**
during an investigation. None of them is part of the translator or the
runtime, and nothing here is built by CMake. Diagnostics for a single
investigation belong here, never inside libabiconv, libinterpose or the core.

There are three kinds:

- **Native capability probes**: plain x86_64 programs that ask "what does this
  modern macOS actually provide?" without involving the translator at all.
  Several long-standing assumptions turned out to be wrong because a
  framework's *on-disk* stub was mistaken for the framework being absent. The
  real code lives in the dyld shared cache, so **`dlopen` + `dlsym` is the only
  authoritative test**.
- **Spies**: native dylibs injected with `DYLD_INSERT_LIBRARIES` (or a
  `__DATA,__interpose` section) that log what a running translated app passes
  to a system API. They change nothing in the runtime.
- **Harnesses**: scripts and i386 programs that drive one translated library
  or app in a controlled way and compare the result with a reference.

Most C and Objective-C probes build with one line; the header comment of each
file gives the exact command and environment variables. For example:

    clang -arch x86_64 -Wno-deprecated-declarations \
        -framework OpenGL -framework ApplicationServices -o /tmp/p cglprobe.c

Scripts read the shared `M64_*` paths (`m64 paths` shows them) and take the
app location from the environment where they need one, e.g. `HALO_APP`.

## Index

### Graphics, windows and display

| File | Question it answers |
|---|---|
| `cglprobe.c` | What does a 2006-era GPU capability check see on this machine? |
| `wpprobe2.c` | Which Carbon/AGL entry points still exist (with the frameworks `dlopen`ed first)? |
| `aglattach.c` | Can an AGL context still attach to a Carbon window, and how? |
| `aglpresent.m` | Can an AGL context on a shown Carbon window be re-presented in a native NSWindow? |
| `aglrenderer.c` | What survives of the classic AGL renderer-enumeration API? |
| `classagl.m` | Can a window class that survives a post-show resize also host AGL? |
| `attrresize.m`, `resizeways.m` | Is there any way to resize a shown Carbon window without `NSCGSPanic`? |
| `displayswitch.m` | Does a classic `CGDisplaySwitchToMode` panic modern AppKit? |
| `windowspy.m` | Which window still owns the display when an app switches display mode? |
| `snapshot.m` | What do the legacy view-snapshot APIs actually capture? |
| `buttonstate.m` | Why is a button invisible? Logs every button-cell draw |
| `tablecolwidth.m` | What width does an `NSTableColumn` actually receive? |
| `gl-call-probe.c` | Which GL call caused an asynchronous GPU-driver crash? |

### Input and events

| File | Question it answers |
|---|---|
| `carboncmdspy.c` | Why does a translated Carbon dialog ignore its buttons? |
| `appdownspy.m` | Where do mouse-down events go when windowed clicks are ignored? |
| `warpspy.m` | What happens on each `CGWarpMouseCursorPosition` call? |
| `keylayout.m` | Which characters does a translated app actually get per key? |

### Files, preferences and memory

| File | Question it answers |
|---|---|
| `aliasprobe2.c`, `aliasprobe4.c` | Does native Alias Manager resolution still work, and is a custom tag after the terminator safe? |
| `file-trace-probe.c` | Which files does a translated app actually try to open? |
| `prefswrite.m` | Does the app's save actually reach `NSUserDefaults`? |
| `badfree.m` | Which pointer is behind a malloc abort, and whose is it? |
| `cfstrrange.m` | Which CFString and range are behind an uncaught `NSRangeException`? |
| `mptask_probe.c` | Does native Carbon Multiprocessing Services still spawn tasks under Rosetta? |

### Loader and runtime internals

| File | Question it answers |
|---|---|
| `dyld-imagewalk-probe.c` | Which image makes dyld abort a walk of its loaded-image array? |
| `dyld-image-list-verify.c` | Does `src/abiconv/dyld_image_list.c` agree with dyld for every image? |
| `kvs-probe.c` | What does a translated singleton's runtime vtable look like over time (Portal 2 `KeyValuesSystem`)? |

### Bink video (Portal 2)

| File | Question it answers |
|---|---|
| `bink-diff.sh` + `binkdec.c` | Does a translated `libbinkmachox86` decode frames identically to ffmpeg's Bink decoder? |
| `bink-emu.py` | Run the original i386 and the translated library under Unicorn and diff them state for state |

### `halo/`: Halo: Combat Evolved investigations

| File | Question it answers |
|---|---|
| `halo-asset-probe.sh` | Does Halo ever open and read its sound asset? |
| `halo-event-probe.sh` + `.c` | Where is a main-menu click lost on the Carbon event path? |
| `halo-input-probe.sh` | Why do menu items highlight but not respond to a click? |
| `seed_halo_eula.sh` | Pre-seeds Halo's first-launch EULA acceptance for test runs |

## Findings worth keeping

- `cglprobe.c`: renderer 0 is accelerated, with `kCGLRPVideoMemory` clamped
  to `INT_MAX`. **The GPU check passes**, so "the renderer check fails" is not
  why Carbon games bail out.
- `wpprobe2.c`: `GetWindowPort`/`GetWindowRef` are genuinely **gone**;
  `HIViewGetRoot`, `HIWindowGetCGWindowID` and **all of AGL** are
  **present**. Without `dlopen`ing the frameworks first, `RTLD_DEFAULT` finds
  nothing and everything looks absent, which is an easy false negative.
- `aglattach.c`: the classic idiom `aglSetDrawable(ctx, GetWindowPort(win))`
  cannot work (the port is gone, so the drawable is NULL), while
  `aglSetWindowRef(ctx, win)` succeeds and gives a live accelerated context
  (`GL_VERSION = 2.1 Metal`). That is the replacement path for every
  translated Carbon + AGL app.
- `aglpresent.m`: an AGLContext *is* its CGLContextObj, so it can be
  re-presented in a native NSView (`NSOpenGLContext initWithCGLContextObj:` +
  `setView:`). Every later `aglSwapBuffers` lands in the view, and
  `aglUpdateContext` does not pull it back (only an explicit
  `aglSetWindowRef` would). This is the basis of the windowed-context
  fullscreen presentation in `cgl_fullscreen_shim.m`.
