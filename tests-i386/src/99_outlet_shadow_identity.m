/* 99_outlet_shadow_identity.m — a nib-connected outlet to a LEGACY object must
 * compare equal to that object when it arrives as a message argument.
 *
 * Quinn's appearance prefs: -imageRepositoryDidSelectImage:(id)repo tests
 * `repo == myPieceImageRepository` to tell the piece-style picker from the
 * board-background picker. The outlet is connected by object_setInstanceVariable
 * (stores the target's i386 shadow), but the R->S outlet mirror on every reverse
 * dispatch re-stored it as a fresh proxy handle, so the compare always failed and
 * a piece style was applied as the board background.
 *
 * ON (fixed) exits 42; OFF (ABICONV_NO_OUTLET_SHADOW_ID=1) exits 1.
 */
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <stdlib.h>

@interface Repository : NSObject
@end
@implementation Repository
@end

@interface PrefsController : NSObject {
    Repository *myPieceRepository;   /* no setter: connected via the raw runtime */
}
- (int)isPieceRepository:(id)repo;
@end
@implementation PrefsController
- (int)isPieceRepository:(id)repo { return repo == myPieceRepository; }
@end

int main(void) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    PrefsController *owner = [[PrefsController alloc] init];
    Repository *piece = [[Repository alloc] init];
    object_setInstanceVariable(owner, "myPieceRepository", piece);
    int same = [owner isPieceRepository:piece];
    [pool drain];
    exit(same ? 42 : 1);
}
