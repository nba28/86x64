// 99_libstdcxx_redirect.cc — guard for the libstdc++-redirect fix (Portal 2,
// 2026-09-29): a translated i386 caller into libstdc++'s iostream/ios_base
// surface (basic_ios/ostream/istream/filebuf/sentry, operator<<, ...) is a raw
// cross-ABI call against modern macOS's NATIVE x86_64 libstdc++.6.dylib — the
// i386 4-byte-return convention vs. the native 8-byte `ret` fuses the return
// address (see the unbridged native-call bug). abi-hazard-vendor.py redirects a
// translated consumer's bind onto our own TRANSLATED i386 libstdc++ (a sibling
// translated module, same 4-byte-return convention, correct object layout).
//
// Exercises the surface the maintainer asked the guard to cover: ostringstream +
// operator<<, istringstream + operator>>, and ifstream (real file I/O
// through a filebuf). Reports PASS/FAIL through the exit code only — a
// fused-PC crash loses buffered stdout (see the silently-inert-guard gotcha).
//
// NOT exercised here, deliberately, because each hits its OWN separate,
// still-open bug (verified 2026-09-29, logged as a known gap) that is not what
// this guard exists to prove -- conflating them would make this guard fail
// for a reason that has nothing to do with the redirect:
//   - operator<<(double): basic_ostream::_M_insert<double> -> ostream::sentry
//     reads a null vtable pointer (isolated with a 2-line repro).
//   - a C++ throw/catch entirely within the fixture's own image, once
//     __cxa_throw/the personality routine resolve to the translated
//     libstdc++ instead of native libc++abi: "no matching handler in any
//     frame" (isolated the same way).
#include <sstream>
#include <fstream>
#include <string>
#include <cstdlib>

int main() {
    std::ostringstream out;
    out << "n=" << 7;
    if (out.str() != "n=7") { exit(1); }

    std::istringstream in("10 20");
    int a = 0, b = 0;
    in >> a >> b;
    if (a != 10 || b != 20) { exit(2); }

    std::ifstream f("/etc/hosts");
    if (!f.good()) { exit(4); }
    std::string line;
    std::getline(f, line);
    if (line.empty()) { exit(5); }

    exit(42);
}
