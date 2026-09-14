/* Native x86_64 helper for the 99_low_native_thunk guard.
 *
 * The point of this file is WHERE it gets mapped, not what it computes. It is
 * linked with `-image_base 0x30000000` so dyld maps genuinely NATIVE code BELOW
 * 4GB -- the situation `fnptr_lookup_result` used to misread as "low, therefore
 * translated, therefore i386-callable as it stands". Modern steamclient.dylib
 * (24 MB, x86_64 + arm64, no i386 slice) lands at such an address in Portal 2
 * for exactly the same reason: dyld put it where there was room.
 *
 * It carries NO LC_LOAD_DYLIB naming libabiconv, so image_is_translated() must
 * classify it NATIVE however low it sits.
 *
 * The argument is what makes the guard bite: a 0-arg function called with the
 * wrong convention returns correctly by accident (nothing to marshal, the result
 * comes back in eax either way). Reading a parameter is what separates an i386
 * cdecl frame from SysV registers.
 */
int m64_lownative_triple(int n) { return n * 3; }
