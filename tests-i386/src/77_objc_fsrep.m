/* 77_objc_fsrep.m — -[NSString fileSystemRepresentation] /
 * getFileSystemRepresentation:maxLength: bridged from translated i386 code.
 *
 * fileSystemRepresentation returns a `const char *` into an internal buffer; on
 * x86_64 that is a >4GB native pointer the i386 caller would truncate, AND
 * modern Foundation's fsrep path builds a VM-backed NSData whose
 * -[NSConcreteData bytes] pthread_mutex_lock()s a NULL-based mutex (SIGSEGV
 * addr=0x8) — the iPhoto account-config background-thread crash. bp_fsrep
 * intercepts both selectors and satisfies them via getCString:...:NSUTF8String-
 * Encoding into a low-4GB per-thread ring buffer, sidestepping the VM-NSData
 * path and returning an i386-dereferenceable pointer.
 *
 * Verifies: the returned C string matches the path (UTF-8), the caller-buffer
 * form fills correctly, and the returned pointer stays valid across a few more
 * fileSystemRepresentation calls (ring-buffer lifetime contract).
 * puts/exit only on success (no vararg/fp printf on the success path).
 */
#include <Foundation/Foundation.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    int failures = 0;
    NSString *p = @"/Users/test/Some Path/file.txt";
    const char *want = "/Users/test/Some Path/file.txt";

    const char *fsr = [p fileSystemRepresentation];
    if (fsr && strcmp(fsr, want) == 0) puts("ok fsr");
    else { printf("FAIL fsr got=%s\n", fsr ? fsr : "(null)"); failures++; }

    char buf[256];
    memset(buf, 0, sizeof(buf));
    if ([p getFileSystemRepresentation:buf maxLength:sizeof(buf)]
        && strcmp(buf, want) == 0) puts("ok getfsr");
    else { printf("FAIL getfsr got=%s\n", buf); failures++; }

    /* ring lifetime: two live fsrep pointers from different strings must both
     * remain valid (the ring is 16 deep, so 2 back-to-back never collide) */
    NSString *q = @"/another/path";
    const char *fp = [p fileSystemRepresentation];
    const char *fq = [q fileSystemRepresentation];
    if (fp && fq && strcmp(fp, want) == 0 && strcmp(fq, "/another/path") == 0)
        puts("ok lifetime");
    else { puts("FAIL lifetime"); failures++; }

    exit(failures);
}
