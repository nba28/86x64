/* 99_rebase_exact_const — integer constants that alias a slidable image's own
 * address span stay integers: a dylib's rebase stream is the exact list of its
 * internal pointers (ParseEnv::have_rebase_stream). minimp3's sample-rate table,
 * padded so 32000/44100/48000 land inside this dylib; rate_name keeps one real
 * (rebased) pointer next to them. Checked by content (rebase_exact_const_test.sh). */
static const unsigned pad[0x3000] = { 1 };
const unsigned g_rates[3] = { 44100, 48000, 32000 };
const char *const rate_name = "rates";
unsigned rate(int i) { return g_rates[i] + pad[i]; }
