# Civ IV modal-hang harness

Reusable on-target measurement tools built while diagnosing the Civilization IV
"modal loop, no window" wall. Nothing here is Civ-specific except the nib
coordinates in `hangrepro.sh` / `wakeup.sh`.

Build the three native helpers first (they are arm64 host tools, not translated):

    clang -arch arm64 -framework Foundation -framework CoreGraphics -o winlist winlist.m
    clang -arch arm64 -framework CoreGraphics -framework ApplicationServices -o clicker clicker.m
    clang -arch arm64 -framework CoreGraphics -framework ApplicationServices -o mover   mover.m

All the shell scripts hardcode a `SCR=` scratch path at the top — point it at
wherever you put the built helpers.

| file | what it does |
|---|---|
| `winlist.m` | lists every WindowServer window for an owner-name substring (pid, layer, onscreen, alpha, bounds). The only reliable way to answer "did a window materialise". |
| `clicker.m` | posts a real `CGEventPost` left click at CG-global coords. |
| `mover.m` | posts a bare `kCGEventMouseMoved`. |
| `probe.py` | lldb script (`command script import probe.py; probe`) that dumps `[NSApp modalWindow]`, its visibility / windowNumber / level, `[NSApp windows]`, and the main-thread frames of a PARKED process. |
| `sync.sh` | copies a worktree's `build/src/abiconv/libabiconv.dylib` over **all 9** co-located copies in the test bundle and re-signs. Use this, **not** `m64 resync` — `m64 resync` has a hardcoded path to the MAIN repo build and will silently install master's dylib over yours. |
| `openrun.sh` | launches via `open` (LaunchServices) and captures windows + CPU% + a full `sample`. |
| `hangrepro.sh` | full hang repro: open, wait for the panel, compute a nib control's CG-global centre from the LIVE window bounds, click it, then capture CPU% and `sample`. |
| `wakeup.sh` | the wake-up discriminator: park the app, then deliver `none` / `move` / `click` events and report parked-vs-returned, with a CPU-time-advance delivery proof. |
| `wakeN.sh` | runs `wakeup.sh` N times per arm — the hang is a RACE, so only frequencies mean anything. |
| `catchhang.sh` | retries until it actually catches a hang, then attaches and runs `probe.py` on the parked process. |
| `axdump.m` | dumps a pid's Accessibility tree (role/title/value + CG-global centre of every element), so a click can target a NAMED button instead of a guessed coordinate. Returns nothing useful for a SELF-DRAWN Carbon window (our HIView widgets expose no AX tree) — that is itself the signal to fall back to computing coordinates from the nib/xib `bounds`. |
| `activate.m` | forces an app frontmost via `NSRunningApplication activateWithOptions:` and reports `isActive` + `activationPolicy` + who is actually frontmost. **Run this before any synthetic-input run**: if it reports `activate=1` but `isActive=0`, input will silently go nowhere. |

Build the two extra helpers with:

    clang -arch arm64 -fno-objc-arc -framework Foundation -framework ApplicationServices -o axdump   axdump.m
    clang -arch arm64 -fno-objc-arc -framework AppKit                                     -o activate activate.m

## Hard-won gotchas (each cost real time)

0. **CHECK THE SCREEN LOCK FIRST — before blaming anything on the app.**
   With the screen locked, `loginwindow` owns frontmost, NOTHING can be
   activated, and every synthetic `CGEventPost` is silently discarded. It looks
   exactly like "the app ignores clicks" / "the modal is inert", and it makes
   the `classic-alert` and `classic-dialog` guards go red for no code reason.
   One command, run it before every GUI run:

       python3 -c "import Quartz; d=Quartz.CGSessionCopyCurrentDictionary(); \
         print('locked =', d.get('CGSSessionScreenIsLocked'), \
               'onconsole =', d.get('kCGSSessionOnConsoleKey'))"

   `locked = True` means STOP: no GUI measurement in this session is meaningful.
   (Cost an agent a full Halo verification pass on 2026-08-02.)

1. **Never measure window state from a bare `exec` of `Contents/MacOS/<binary>`.**
   With no LaunchServices activation the app never shows a window, which
   manufactures a completely fictitious "modal loop, no window" bug. Use `open`.
2. **`pkill -9 -f "<app name>"` matches the full command line** and will SIGKILL
   a running `m64 translate` pipeline whose argv contains the app name. Always
   `pkill -9 -x`.
3. **Prove the bytes under test are yours before measuring.** For copies inside
   signed nested bundles a raw `sha256` mismatch is expected after
   `codesign -f --deep` — compare **size + `dwarfdump --uuid`** instead, and
   cross-check the UUID against the `Binary Images` section of your `sample`
   output to prove the running process loaded your build.
4. **A `kCGEventMouseMoved` is only delivered if the window has
   `acceptsMouseMovedEvents`.** "Nothing happened after mouse-moves" may mean no
   event ever arrived. Use a click, and prove delivery with a CPU-time advance.
5. **lldb cannot unwind past an abigen bridge frame** — the caller frame comes
   back as garbage, so neither `sample` nor `bt` will show the i386 caller of a
   bridged call. Symbolise the bridge itself with `nm -n` plus the image load
   address from `sample`'s `Binary Images`.
6. **Attaching lldb can mask the bug.** Its SIGSTOP/resume and breakpoint traps
   generate mach traffic that wakes a parked run loop, so a bug that reproduces
   bare may vanish under the debugger. Confirm bare first.
