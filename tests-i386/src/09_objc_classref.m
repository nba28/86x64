/*
 * 09_objc_classref — exercises the ObjC bridge end to end: class messages
 * (compiler-emitted __OBJC,__cls_refs / __message_refs), the nil-terminated
 * varargs path (arrayWithObjects:), instance messages, and object
 * wrap/unwrap through the proxy arena. Uses only Foundation classes that
 * exist in the modern x86_64 runtime, so no legacy class registration is
 * needed — the bridge resolves them by name.
 *
 * Validates via EXIT CODE (0 = all results correct), not printf: a 4-arg
 * printf would hit the separate, pre-existing libabiconv varargs marshalling
 * bug that 04_pic_data_read also documents and dodges. Keeping output off the
 * stdio path isolates this test to the ObjC bridge + PIC translation.
 *
 * Exercises: class messages via compiler-emitted __OBJC,__cls_refs /
 * __message_refs loaded through the i386 get_pc_thunk PIC anchor (including
 * an anchor spilled to the frame and reloaded into another register), the
 * nil-terminated varargs path (arrayWithObjects:), instance messages, and
 * object wrap/unwrap through the proxy arena.
 */
#import <Foundation/Foundation.h>

extern void _exit(int);

int main(void) {
    NSNumber *n = [NSNumber numberWithInt:7];
    int v = [n intValue];

    NSArray *a = [NSArray arrayWithObjects:
        [NSNumber numberWithInt:10],
        [NSNumber numberWithInt:20],
        [NSNumber numberWithInt:30], nil];
    int c = (int)[a count];

    int sum = 0;
    for (int i = 0; i < c; i++)
        sum += [[a objectAtIndex:i] intValue];

    /* 0 only if every class/selector resolved and arithmetic is right. */
    _exit((v == 7 && c == 3 && sum == 60) ? 0 : 1);
}
