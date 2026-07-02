/* 60_objc_nsduring.m — classic Foundation NS_DURING / NSHandler2 exception
 * bridge (the Quinn play-crash family, crashlog Quinn-2026-07-02-160137.ips:
 * fused PC in -[ATAnimation runFrom:to:], r11=_NSAddHandler2).
 *
 * Pre-@try SDKs expanded NS_DURING/NS_HANDLER to the OLDER NSHandler2 spelling:
 *
 *     NSHandler2 h;
 *     _NSAddHandler2(&h);
 *     if (_setjmp(&h) == 0) {            // jmp_buf at offset 0 of NSHandler2
 *         <body>
 *         _NSRemoveHandler2(&h);
 *     } else {
 *         id e = _NSExceptionObjectFromHandler2(&h);
 *         <handler reads e name/reason>
 *     }
 *
 * The 10.6+ host SDK redefines NS_DURING to @try (the objc_exception_* path),
 * so we hand-write the older calls to exercise the NSHandler2 path that Quinn
 * actually binds (__NSAddHandler2 / __setjmp / __NSRemoveHandler2 /
 * __NSExceptionObjectFromHandler2), mirroring the compiled i386 shape exactly:
 * the SAME pointer &h is fed to both _NSAddHandler2 and _setjmp, so jmp_buf
 * sits at offset 0 of the >=88-byte handler.
 *
 * Pre-fix, __NSAddHandler2 bound to NATIVE modern Foundation: its 8-byte `ret`
 * over-pops the i386 4-byte return frame -> fused-PC SIGSEGV, fired even with
 * NO exception thrown (case a). Post-fix, ____NSAddHandler2 routes into the
 * per-thread exc_chain shared with the @try machinery, so a translated raise
 * (objc_exception_throw) longjmps back through ____setjmp into the handler
 * (cases b, c).
 */
#include <Foundation/Foundation.h>
#include <setjmp.h>
#include <stdio.h>

/* Private classic-Foundation runtime functions (no public header). */
extern void _NSAddHandler2(void *handler);
extern void _NSRemoveHandler2(void *handler);
extern id   _NSExceptionObjectFromHandler2(void *handler);
extern void objc_exception_throw(id exception);

/* NSHandler2 is opaque and >= 88 bytes on i386 (jmp_buf[18]=72 bytes + fields).
 * jmp_buf must be at offset 0 (the app passes &h to _setjmp). 112 bytes matches
 * Quinn's `subl $0xcc; leal -0x70(%ebp)` stack reservation. */
typedef struct { jmp_buf jb; void *rest[10]; } NSHandler2;

int main(void) {
    volatile int failures = 0;   /* volatile: survives longjmp register clobber */

    /* (a) NS_DURING body runs, NO exception — the over-pop fast path that
     *     crashes pre-fix even though nothing is ever thrown. */
    {
        NSHandler2 h;
        _NSAddHandler2(&h);
        if (_setjmp(h.jb) == 0) {
            puts("a: body ran");
            _NSRemoveHandler2(&h);
            puts("a: normal exit");
        } else {
            puts("a: FAIL took handler with no throw");
            failures++;
        }
    }

    /* (b) A translated raise inside the body lands in the handler, and the
     *     caught exception's name/reason are readable across the bridge. */
    {
        NSHandler2 h;
        _NSAddHandler2(&h);
        if (_setjmp(h.jb) == 0) {
            NSException *e = [NSException exceptionWithName:@"BoomException"
                                                    reason:@"kaboom 42"
                                                  userInfo:nil];
            objc_exception_throw(e);
            puts("b: FAIL fell through throw");
            failures++;
        } else {
            id caught = _NSExceptionObjectFromHandler2(&h);
            printf("b: caught name=%s reason=%s\n",
                   [[caught name] UTF8String], [[caught reason] UTF8String]);
        }
    }

    /* (c) Nested handlers: the inner frame catches Inner, then a second raise
     *     unwinds to the outer frame which catches Outer. Both share the one
     *     per-thread chain; the throw pops the top frame before longjmp. */
    {
        NSHandler2 ho;
        _NSAddHandler2(&ho);
        if (_setjmp(ho.jb) == 0) {
            NSHandler2 hi;
            _NSAddHandler2(&hi);
            if (_setjmp(hi.jb) == 0) {
                objc_exception_throw([NSException exceptionWithName:@"Inner"
                                                            reason:@"i" userInfo:nil]);
                puts("c: FAIL inner fell through");
                failures++;
            } else {
                id ie = _NSExceptionObjectFromHandler2(&hi);
                printf("c: inner caught %s\n", [[ie name] UTF8String]);
            }
            /* inner frame already popped by its throw; now raise to the outer */
            objc_exception_throw([NSException exceptionWithName:@"Outer"
                                                        reason:@"o" userInfo:nil]);
            puts("c: FAIL after-inner fell through");
            failures++;
            _NSRemoveHandler2(&ho);
        } else {
            id oe = _NSExceptionObjectFromHandler2(&ho);
            printf("c: outer caught %s\n", [[oe name] UTF8String]);
        }
    }

    printf("done failures=%d\n", failures);
    exit(failures);
}
