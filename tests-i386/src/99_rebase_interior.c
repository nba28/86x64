/* 99_rebase_interior — rebased pointers that enter a read-only byte table
 * mid-word keep their exact byte. __TEXT,__const is parsed in 4-byte words, so
 * a target at word+1..3 has no blob of its own; its placeholder used to land
 * before the NEXT word and the pointer moved up to 3 bytes (Python 2.6's
 * grammar d_first bitsets: every parse a SyntaxError). Checked by content
 * (rebase_interior_test.sh). */
const unsigned char g_tab[8] __attribute__((aligned(4))) = { 0x84, 0x28, 0x30, 0x47, 0x11, 0x22, 0x33, 0x44 };
const unsigned char *const g_ptrs[3] = { g_tab + 1, g_tab + 2, g_tab + 3 };
int tab_at(int i) { return *g_ptrs[i]; }
