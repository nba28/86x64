// 29_weak_new_delete — guard the static-interpose weak_bind REDIRECT.
//
// C++ operator new/delete (__Znwm/__Znam/__ZdlPv/__ZdaPv) are EXTERNAL
// weak-coalesced symbols: an i386 C++ binary imports them from libstdc++ as
// "weak external [lazy bound]", so they appear in BOTH the regular bind table
// AND the weak_bind table, targeting the SAME __la_symbol_ptr slot.
// static-interpose rewrites the regular bind to libabiconv's ____Znwm shim, but
// dyld processes weak binds AFTER regular binds: a weak_bind entry left under
// its ORIGINAL name re-resolves the slot by flat weak coalescing and OVERWRITES
// the shim pointer with the NATIVE x86_64 operator new — which is then entered
// with the i386 cdecl frame (size pushed on the stack, NOT in %rdi) so it reads
// a garbage size, and its native 8-byte `ret` over-pops the i386 4-byte return
// push -> fused PC crash. The fix renames the weak_bind entry to the shim name
// too (no new_dylib — a weak bind carries no ordinal), so the coalesced lookup
// keeps resolving to libabiconv's ____Znwm (its only definition).
//
// Unlike 28_weak_bind (self-defined weak vtables, NO external binds) this
// exercises operator new/new[]/delete/delete[] — forcing those weak EXTERNALS
// into the weak_bind table — and verifies allocation works and values survive a
// round-trip through the heap. A mis-redirected weak bind crashes or corrupts.
//
// Needs `make sysroot-cpp` (SL-era i386 libstdc++) once. The libstdc++ dep is
// kept (operator new is used), so __Znwm is a genuine weak-undef external.
// Verify the table is non-empty:
//   xcrun llvm-objdump --macho --arch=i386 --weak-bind build/29_weak_new_delete.i386

#include <cstdio>
#include <cstdlib>

struct Node {
   int v;
   Node *next;
   Node(int x, Node *n) : v(x), next(n) {}
};

int main() {
   // operator new (__Znwm) for each Node.
   Node *head = nullptr;
   for (int i = 1; i <= 5; i++) {
      head = new Node(i * i, head);
   }
   int sum = 0, count = 0;
   for (Node *p = head; p; p = p->next) {
      sum += p->v;
      count++;
   }
   printf("count=%d sum=%d\n", count, sum);

   // operator new[] (__Znam) + operator delete[] (__ZdaPv).
   int *arr = new int[4];
   for (int i = 0; i < 4; i++) {
      arr[i] = (i + 1) * 10;
   }
   int asum = 0;
   for (int i = 0; i < 4; i++) {
      asum += arr[i];
   }
   printf("arr sum=%d\n", asum);
   delete[] arr;

   // operator delete (__ZdlPv) for each Node.
   while (head) {
      Node *n = head->next;
      delete head;
      head = n;
   }
   printf("ok\n");

   /* exit() (not _exit()) so stdio buffers flush; the wrapper enters _main via
      jmp so a bare `return` has no return address to unwind to. */
   exit(0);
}
