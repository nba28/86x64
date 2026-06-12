#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "libobjc.A.dylib", s); }

@interface NSObject : NSObject
@end
@implementation NSObject

@end

long OBJC_EHTYPE_id(long a, long b, long c_, long d, long e, long f) { shim_note("OBJC_EHTYPE_id called (auto-stub)"); return 0; }

long objc_personality_v0(long a, long b, long c_, long d, long e, long f) { shim_note("objc_personality_v0 called (auto-stub)"); return 0; }

long objc_empty_cache(long a, long b, long c_, long d, long e, long f) { shim_note("objc_empty_cache called (auto-stub)"); return 0; }

long objc_empty_vtable(long a, long b, long c_, long d, long e, long f) { shim_note("objc_empty_vtable called (auto-stub)"); return 0; }

long class_addMethod(long a, long b, long c_, long d, long e, long f) { shim_note("class_addMethod called (auto-stub)"); return 0; }

long class_copyMethodList(long a, long b, long c_, long d, long e, long f) { shim_note("class_copyMethodList called (auto-stub)"); return 0; }

long class_getClassMethod(long a, long b, long c_, long d, long e, long f) { shim_note("class_getClassMethod called (auto-stub)"); return 0; }

long class_getInstanceMethod(long a, long b, long c_, long d, long e, long f) { shim_note("class_getInstanceMethod called (auto-stub)"); return 0; }

long class_getInstanceSize(long a, long b, long c_, long d, long e, long f) { shim_note("class_getInstanceSize called (auto-stub)"); return 0; }

long class_getMethodImplementation(long a, long b, long c_, long d, long e, long f) { shim_note("class_getMethodImplementation called (auto-stub)"); return 0; }

long class_getSuperclass(long a, long b, long c_, long d, long e, long f) { shim_note("class_getSuperclass called (auto-stub)"); return 0; }

long class_setSuperclass(long a, long b, long c_, long d, long e, long f) { shim_note("class_setSuperclass called (auto-stub)"); return 0; }

long method_exchangeImplementations(long a, long b, long c_, long d, long e, long f) { shim_note("method_exchangeImplementations called (auto-stub)"); return 0; }

long method_getDescription(long a, long b, long c_, long d, long e, long f) { shim_note("method_getDescription called (auto-stub)"); return 0; }

long method_getImplementation(long a, long b, long c_, long d, long e, long f) { shim_note("method_getImplementation called (auto-stub)"); return 0; }

long method_getTypeEncoding(long a, long b, long c_, long d, long e, long f) { shim_note("method_getTypeEncoding called (auto-stub)"); return 0; }

long method_invoke(long a, long b, long c_, long d, long e, long f) { shim_note("method_invoke called (auto-stub)"); return 0; }

long method_setImplementation(long a, long b, long c_, long d, long e, long f) { shim_note("method_setImplementation called (auto-stub)"); return 0; }

long objc_assign_global(long a, long b, long c_, long d, long e, long f) { shim_note("objc_assign_global called (auto-stub)"); return 0; }

long objc_assign_ivar(long a, long b, long c_, long d, long e, long f) { shim_note("objc_assign_ivar called (auto-stub)"); return 0; }

long objc_assign_strongCast(long a, long b, long c_, long d, long e, long f) { shim_note("objc_assign_strongCast called (auto-stub)"); return 0; }

long objc_begin_catch(long a, long b, long c_, long d, long e, long f) { shim_note("objc_begin_catch called (auto-stub)"); return 0; }

long objc_collecting_enabled(long a, long b, long c_, long d, long e, long f) { shim_note("objc_collecting_enabled called (auto-stub)"); return 0; }

long objc_copyStruct(long a, long b, long c_, long d, long e, long f) { shim_note("objc_copyStruct called (auto-stub)"); return 0; }

long objc_ehtype_vtable(long a, long b, long c_, long d, long e, long f) { shim_note("objc_ehtype_vtable called (auto-stub)"); return 0; }

long objc_empty_vtable(long a, long b, long c_, long d, long e, long f) { shim_note("objc_empty_vtable called (auto-stub)"); return 0; }

long objc_end_catch(long a, long b, long c_, long d, long e, long f) { shim_note("objc_end_catch called (auto-stub)"); return 0; }

long objc_enumerationMutation(long a, long b, long c_, long d, long e, long f) { shim_note("objc_enumerationMutation called (auto-stub)"); return 0; }

long objc_exception_extract(long a, long b, long c_, long d, long e, long f) { shim_note("objc_exception_extract called (auto-stub)"); return 0; }

long objc_exception_match(long a, long b, long c_, long d, long e, long f) { shim_note("objc_exception_match called (auto-stub)"); return 0; }

long objc_exception_rethrow(long a, long b, long c_, long d, long e, long f) { shim_note("objc_exception_rethrow called (auto-stub)"); return 0; }

long objc_exception_throw(long a, long b, long c_, long d, long e, long f) { shim_note("objc_exception_throw called (auto-stub)"); return 0; }

long objc_exception_try_enter(long a, long b, long c_, long d, long e, long f) { shim_note("objc_exception_try_enter called (auto-stub)"); return 0; }

long objc_exception_try_exit(long a, long b, long c_, long d, long e, long f) { shim_note("objc_exception_try_exit called (auto-stub)"); return 0; }

long objc_getAssociatedObject(long a, long b, long c_, long d, long e, long f) { shim_note("objc_getAssociatedObject called (auto-stub)"); return 0; }

long objc_getClass(long a, long b, long c_, long d, long e, long f) { shim_note("objc_getClass called (auto-stub)"); return 0; }

long objc_getMetaClass(long a, long b, long c_, long d, long e, long f) { shim_note("objc_getMetaClass called (auto-stub)"); return 0; }

long objc_getProperty(long a, long b, long c_, long d, long e, long f) { shim_note("objc_getProperty called (auto-stub)"); return 0; }

long objc_loadWeak(long a, long b, long c_, long d, long e, long f) { shim_note("objc_loadWeak called (auto-stub)"); return 0; }

long objc_msgSend(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSend called (auto-stub)"); return 0; }

long objc_msgSendSuper(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSendSuper called (auto-stub)"); return 0; }

long objc_msgSendSuper2(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSendSuper2 called (auto-stub)"); return 0; }

long objc_msgSendSuper2_fixup(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSendSuper2_fixup called (auto-stub)"); return 0; }

long objc_msgSendSuper2_stret(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSendSuper2_stret called (auto-stub)"); return 0; }

long objc_msgSendSuper2_stret_fixup(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSendSuper2_stret_fixup called (auto-stub)"); return 0; }

long objc_msgSend_fixup(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSend_fixup called (auto-stub)"); return 0; }

long objc_msgSend_fpret(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSend_fpret called (auto-stub)"); return 0; }

long objc_msgSend_stret(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSend_stret called (auto-stub)"); return 0; }

long objc_msgSend_stret_fixup(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSend_stret_fixup called (auto-stub)"); return 0; }

long objc_setAssociatedObject(long a, long b, long c_, long d, long e, long f) { shim_note("objc_setAssociatedObject called (auto-stub)"); return 0; }

long objc_setProperty(long a, long b, long c_, long d, long e, long f) { shim_note("objc_setProperty called (auto-stub)"); return 0; }

long objc_setProperty_atomic_copy(long a, long b, long c_, long d, long e, long f) { shim_note("objc_setProperty_atomic_copy called (auto-stub)"); return 0; }

long objc_setProperty_nonatomic(long a, long b, long c_, long d, long e, long f) { shim_note("objc_setProperty_nonatomic called (auto-stub)"); return 0; }

long objc_setProperty_nonatomic_copy(long a, long b, long c_, long d, long e, long f) { shim_note("objc_setProperty_nonatomic_copy called (auto-stub)"); return 0; }

long objc_storeWeak(long a, long b, long c_, long d, long e, long f) { shim_note("objc_storeWeak called (auto-stub)"); return 0; }

long objc_sync_enter(long a, long b, long c_, long d, long e, long f) { shim_note("objc_sync_enter called (auto-stub)"); return 0; }

long objc_sync_exit(long a, long b, long c_, long d, long e, long f) { shim_note("objc_sync_exit called (auto-stub)"); return 0; }

long objc_terminate(long a, long b, long c_, long d, long e, long f) { shim_note("objc_terminate called (auto-stub)"); return 0; }

long object_getInstanceVariable(long a, long b, long c_, long d, long e, long f) { shim_note("object_getInstanceVariable called (auto-stub)"); return 0; }

long sel_isEqual(long a, long b, long c_, long d, long e, long f) { shim_note("sel_isEqual called (auto-stub)"); return 0; }

long sel_registerName(long a, long b, long c_, long d, long e, long f) { shim_note("sel_registerName called (auto-stub)"); return 0; }
