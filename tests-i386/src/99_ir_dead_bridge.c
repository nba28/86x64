/* 99_ir_dead_bridge — import_repair must not wire a weak-NULL slot to a
 * libabiconv bridge that can only jump to NULL (abigen bridge, native removed).
 * Call of Duty 4: NewControlKeyFilterUPP (32-bit-only HIToolbox) -> rip=0; that
 * one is a real shim now (upp_shim.c), so this probes CopyPixPat, another
 * removed-native abigen bridge nothing shims.
 * ON exits 42: the slot stays a loud `missing` gap, the call returns 0.
 * OFF (M64_NO_IR_DEAD_BRIDGE=1, run time): wired to the bridge -> crash. */
extern void exit(int);
extern int printf(const char *, ...);
void *CopyPixPat(void *srcPP);
int main(void) {
   void *upp = CopyPixPat((void *)0);
   printf("upp=%p\n", upp);
   exit(upp == 0 ? 42 : 1);
}
