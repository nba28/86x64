#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "libSystem.B.dylib", s); }

long APP_SANDBOX_READ(long a, long b, long c_, long d, long e, long f) { shim_note("APP_SANDBOX_READ called (auto-stub)"); return 0; }

long APP_SANDBOX_READ_WRITE(long a, long b, long c_, long d, long e, long f) { shim_note("APP_SANDBOX_READ_WRITE called (auto-stub)"); return 0; }

long CC_MD5(long a, long b, long c_, long d, long e, long f) { shim_note("CC_MD5 called (auto-stub)"); return 0; }

long CC_MD5_Final(long a, long b, long c_, long d, long e, long f) { shim_note("CC_MD5_Final called (auto-stub)"); return 0; }

long CC_MD5_Init(long a, long b, long c_, long d, long e, long f) { shim_note("CC_MD5_Init called (auto-stub)"); return 0; }

long CC_MD5_Update(long a, long b, long c_, long d, long e, long f) { shim_note("CC_MD5_Update called (auto-stub)"); return 0; }

long CC_SHA1(long a, long b, long c_, long d, long e, long f) { shim_note("CC_SHA1 called (auto-stub)"); return 0; }

long NDR_record(long a, long b, long c_, long d, long e, long f) { shim_note("NDR_record called (auto-stub)"); return 0; }

long NSAddImage(long a, long b, long c_, long d, long e, long f) { shim_note("NSAddImage called (auto-stub)"); return 0; }

long NSAddressOfSymbol(long a, long b, long c_, long d, long e, long f) { shim_note("NSAddressOfSymbol called (auto-stub)"); return 0; }

long NSLookupSymbolInImage(long a, long b, long c_, long d, long e, long f) { shim_note("NSLookupSymbolInImage called (auto-stub)"); return 0; }

long NSVersionOfLinkTimeLibrary(long a, long b, long c_, long d, long e, long f) { shim_note("NSVersionOfLinkTimeLibrary called (auto-stub)"); return 0; }

long NSVersionOfRunTimeLibrary(long a, long b, long c_, long d, long e, long f) { shim_note("NSVersionOfRunTimeLibrary called (auto-stub)"); return 0; }

long OSAtomicAdd32(long a, long b, long c_, long d, long e, long f) { shim_note("OSAtomicAdd32 called (auto-stub)"); return 0; }

long OSAtomicAdd32Barrier(long a, long b, long c_, long d, long e, long f) { shim_note("OSAtomicAdd32Barrier called (auto-stub)"); return 0; }

long OSAtomicAdd64Barrier(long a, long b, long c_, long d, long e, long f) { shim_note("OSAtomicAdd64Barrier called (auto-stub)"); return 0; }

long OSAtomicCompareAndSwap32Barrier(long a, long b, long c_, long d, long e, long f) { shim_note("OSAtomicCompareAndSwap32Barrier called (auto-stub)"); return 0; }

long OSAtomicCompareAndSwap64(long a, long b, long c_, long d, long e, long f) { shim_note("OSAtomicCompareAndSwap64 called (auto-stub)"); return 0; }

long OSAtomicCompareAndSwap64Barrier(long a, long b, long c_, long d, long e, long f) { shim_note("OSAtomicCompareAndSwap64Barrier called (auto-stub)"); return 0; }

long OSAtomicCompareAndSwapPtrBarrier(long a, long b, long c_, long d, long e, long f) { shim_note("OSAtomicCompareAndSwapPtrBarrier called (auto-stub)"); return 0; }

long OSMemoryBarrier(long a, long b, long c_, long d, long e, long f) { shim_note("OSMemoryBarrier called (auto-stub)"); return 0; }

long OSSpinLockLock(long a, long b, long c_, long d, long e, long f) { shim_note("OSSpinLockLock called (auto-stub)"); return 0; }

long OSSpinLockUnlock(long a, long b, long c_, long d, long e, long f) { shim_note("OSSpinLockUnlock called (auto-stub)"); return 0; }

long SANDBOX_EXTENSION_CANONICAL(long a, long b, long c_, long d, long e, long f) { shim_note("SANDBOX_EXTENSION_CANONICAL called (auto-stub)"); return 0; }

long Block_object_assign(long a, long b, long c_, long d, long e, long f) { shim_note("Block_object_assign called (auto-stub)"); return 0; }

long Block_object_dispose(long a, long b, long c_, long d, long e, long f) { shim_note("Block_object_dispose called (auto-stub)"); return 0; }

long DefaultRuneLocale(long a, long b, long c_, long d, long e, long f) { shim_note("DefaultRuneLocale called (auto-stub)"); return 0; }

long NSConcreteGlobalBlock(long a, long b, long c_, long d, long e, long f) { shim_note("NSConcreteGlobalBlock called (auto-stub)"); return 0; }

long NSConcreteStackBlock(long a, long b, long c_, long d, long e, long f) { shim_note("NSConcreteStackBlock called (auto-stub)"); return 0; }

long Unwind_Resume(long a, long b, long c_, long d, long e, long f) { shim_note("Unwind_Resume called (auto-stub)"); return 0; }

long assert_rtn(long a, long b, long c_, long d, long e, long f) { shim_note("assert_rtn called (auto-stub)"); return 0; }

long bzero(long a, long b, long c_, long d, long e, long f) { shim_note("bzero called (auto-stub)"); return 0; }

long cxa_atexit(long a, long b, long c_, long d, long e, long f) { shim_note("cxa_atexit called (auto-stub)"); return 0; }

long error(long a, long b, long c_, long d, long e, long f) { shim_note("error called (auto-stub)"); return 0; }

long exp10(long a, long b, long c_, long d, long e, long f) { shim_note("exp10 called (auto-stub)"); return 0; }

long maskrune(long a, long b, long c_, long d, long e, long f) { shim_note("maskrune called (auto-stub)"); return 0; }

long memcpy_chk(long a, long b, long c_, long d, long e, long f) { shim_note("memcpy_chk called (auto-stub)"); return 0; }

long memmove_chk(long a, long b, long c_, long d, long e, long f) { shim_note("memmove_chk called (auto-stub)"); return 0; }

long snprintf_chk(long a, long b, long c_, long d, long e, long f) { shim_note("snprintf_chk called (auto-stub)"); return 0; }

long sprintf_chk(long a, long b, long c_, long d, long e, long f) { shim_note("sprintf_chk called (auto-stub)"); return 0; }

long stack_chk_fail(long a, long b, long c_, long d, long e, long f) { shim_note("stack_chk_fail called (auto-stub)"); return 0; }

long stack_chk_guard(long a, long b, long c_, long d, long e, long f) { shim_note("stack_chk_guard called (auto-stub)"); return 0; }

long stderrp(long a, long b, long c_, long d, long e, long f) { shim_note("stderrp called (auto-stub)"); return 0; }

long stdoutp(long a, long b, long c_, long d, long e, long f) { shim_note("stdoutp called (auto-stub)"); return 0; }

long strcpy_chk(long a, long b, long c_, long d, long e, long f) { shim_note("strcpy_chk called (auto-stub)"); return 0; }

long strncat_chk(long a, long b, long c_, long d, long e, long f) { shim_note("strncat_chk called (auto-stub)"); return 0; }

long strncpy_chk(long a, long b, long c_, long d, long e, long f) { shim_note("strncpy_chk called (auto-stub)"); return 0; }

long toupper(long a, long b, long c_, long d, long e, long f) { shim_note("toupper called (auto-stub)"); return 0; }

long c_locale(long a, long b, long c_, long d, long e, long f) { shim_note("c_locale called (auto-stub)"); return 0; }

long dispatch_main_q(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_main_q called (auto-stub)"); return 0; }

long dispatch_queue_attr_concurrent(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_queue_attr_concurrent called (auto-stub)"); return 0; }

long dispatch_source_type_timer(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_source_type_timer called (auto-stub)"); return 0; }

long os_log_pack_fill(long a, long b, long c_, long d, long e, long f) { shim_note("os_log_pack_fill called (auto-stub)"); return 0; }

long os_log_pack_size(long a, long b, long c_, long d, long e, long f) { shim_note("os_log_pack_size called (auto-stub)"); return 0; }

long os_nospin_lock_lock(long a, long b, long c_, long d, long e, long f) { shim_note("os_nospin_lock_lock called (auto-stub)"); return 0; }

long os_nospin_lock_unlock(long a, long b, long c_, long d, long e, long f) { shim_note("os_nospin_lock_unlock called (auto-stub)"); return 0; }

long setjmp(long a, long b, long c_, long d, long e, long f) { shim_note("setjmp called (auto-stub)"); return 0; }

long xpc_error_key_description(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_error_key_description called (auto-stub)"); return 0; }

long xpc_runtime_is_app_sandboxed(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_runtime_is_app_sandboxed called (auto-stub)"); return 0; }

long xpc_type_error(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_type_error called (auto-stub)"); return 0; }

long abort(long a, long b, long c_, long d, long e, long f) { shim_note("abort called (auto-stub)"); return 0; }

long accept(long a, long b, long c_, long d, long e, long f) { shim_note("accept called (auto-stub)"); return 0; }

long access(long a, long b, long c_, long d, long e, long f) { shim_note("access called (auto-stub)"); return 0; }

long acos(long a, long b, long c_, long d, long e, long f) { shim_note("acos called (auto-stub)"); return 0; }

long acosf(long a, long b, long c_, long d, long e, long f) { shim_note("acosf called (auto-stub)"); return 0; }

long arc4random(long a, long b, long c_, long d, long e, long f) { shim_note("arc4random called (auto-stub)"); return 0; }

long asin(long a, long b, long c_, long d, long e, long f) { shim_note("asin called (auto-stub)"); return 0; }

long asl_free(long a, long b, long c_, long d, long e, long f) { shim_note("asl_free called (auto-stub)"); return 0; }

long asl_new(long a, long b, long c_, long d, long e, long f) { shim_note("asl_new called (auto-stub)"); return 0; }

long asl_send(long a, long b, long c_, long d, long e, long f) { shim_note("asl_send called (auto-stub)"); return 0; }

long asl_set(long a, long b, long c_, long d, long e, long f) { shim_note("asl_set called (auto-stub)"); return 0; }

long atan(long a, long b, long c_, long d, long e, long f) { shim_note("atan called (auto-stub)"); return 0; }

long atan2(long a, long b, long c_, long d, long e, long f) { shim_note("atan2 called (auto-stub)"); return 0; }

long atan2f(long a, long b, long c_, long d, long e, long f) { shim_note("atan2f called (auto-stub)"); return 0; }

long atanf(long a, long b, long c_, long d, long e, long f) { shim_note("atanf called (auto-stub)"); return 0; }

long atoi(long a, long b, long c_, long d, long e, long f) { shim_note("atoi called (auto-stub)"); return 0; }

long backtrace(long a, long b, long c_, long d, long e, long f) { shim_note("backtrace called (auto-stub)"); return 0; }

long backtrace_symbols(long a, long b, long c_, long d, long e, long f) { shim_note("backtrace_symbols called (auto-stub)"); return 0; }

long bcmp(long a, long b, long c_, long d, long e, long f) { shim_note("bcmp called (auto-stub)"); return 0; }

long bcopy(long a, long b, long c_, long d, long e, long f) { shim_note("bcopy called (auto-stub)"); return 0; }

long bind(long a, long b, long c_, long d, long e, long f) { shim_note("bind called (auto-stub)"); return 0; }

long calloc(long a, long b, long c_, long d, long e, long f) { shim_note("calloc called (auto-stub)"); return 0; }

long ceil(long a, long b, long c_, long d, long e, long f) { shim_note("ceil called (auto-stub)"); return 0; }

long ceilf(long a, long b, long c_, long d, long e, long f) { shim_note("ceilf called (auto-stub)"); return 0; }

long cfmakeraw(long a, long b, long c_, long d, long e, long f) { shim_note("cfmakeraw called (auto-stub)"); return 0; }

long cfsetspeed(long a, long b, long c_, long d, long e, long f) { shim_note("cfsetspeed called (auto-stub)"); return 0; }

long chmod(long a, long b, long c_, long d, long e, long f) { shim_note("chmod called (auto-stub)"); return 0; }

long close(long a, long b, long c_, long d, long e, long f) { shim_note("close called (auto-stub)"); return 0; }

long confstr(long a, long b, long c_, long d, long e, long f) { shim_note("confstr called (auto-stub)"); return 0; }

long connect(long a, long b, long c_, long d, long e, long f) { shim_note("connect called (auto-stub)"); return 0; }

long cos(long a, long b, long c_, long d, long e, long f) { shim_note("cos called (auto-stub)"); return 0; }

long cosf(long a, long b, long c_, long d, long e, long f) { shim_note("cosf called (auto-stub)"); return 0; }

long cosh(long a, long b, long c_, long d, long e, long f) { shim_note("cosh called (auto-stub)"); return 0; }

long coshf(long a, long b, long c_, long d, long e, long f) { shim_note("coshf called (auto-stub)"); return 0; }

long difftime(long a, long b, long c_, long d, long e, long f) { shim_note("difftime called (auto-stub)"); return 0; }

long dispatch_after(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_after called (auto-stub)"); return 0; }

long dispatch_async(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_async called (auto-stub)"); return 0; }

long dispatch_barrier_async(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_barrier_async called (auto-stub)"); return 0; }

long dispatch_get_global_queue(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_get_global_queue called (auto-stub)"); return 0; }

long dispatch_group_create(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_group_create called (auto-stub)"); return 0; }

long dispatch_group_enter(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_group_enter called (auto-stub)"); return 0; }

long dispatch_group_leave(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_group_leave called (auto-stub)"); return 0; }

long dispatch_group_wait(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_group_wait called (auto-stub)"); return 0; }

long dispatch_once(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_once called (auto-stub)"); return 0; }

long dispatch_queue_create(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_queue_create called (auto-stub)"); return 0; }

long dispatch_release(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_release called (auto-stub)"); return 0; }

long dispatch_resume(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_resume called (auto-stub)"); return 0; }

long dispatch_retain(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_retain called (auto-stub)"); return 0; }

long dispatch_set_target_queue(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_set_target_queue called (auto-stub)"); return 0; }

long dispatch_source_create(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_source_create called (auto-stub)"); return 0; }

long dispatch_source_set_event_handler(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_source_set_event_handler called (auto-stub)"); return 0; }

long dispatch_sync(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_sync called (auto-stub)"); return 0; }

long dispatch_time(long a, long b, long c_, long d, long e, long f) { shim_note("dispatch_time called (auto-stub)"); return 0; }

long dlopen(long a, long b, long c_, long d, long e, long f) { shim_note("dlopen called (auto-stub)"); return 0; }

long dlsym(long a, long b, long c_, long d, long e, long f) { shim_note("dlsym called (auto-stub)"); return 0; }

long dyld_stub_binder(long a, long b, long c_, long d, long e, long f) { shim_note("dyld_stub_binder called (auto-stub)"); return 0; }

long exit(long a, long b, long c_, long d, long e, long f) { shim_note("exit called (auto-stub)"); return 0; }

long exp(long a, long b, long c_, long d, long e, long f) { shim_note("exp called (auto-stub)"); return 0; }

long exp2(long a, long b, long c_, long d, long e, long f) { shim_note("exp2 called (auto-stub)"); return 0; }

long expf(long a, long b, long c_, long d, long e, long f) { shim_note("expf called (auto-stub)"); return 0; }

long fclose(long a, long b, long c_, long d, long e, long f) { shim_note("fclose called (auto-stub)"); return 0; }

long fcntl(long a, long b, long c_, long d, long e, long f) { shim_note("fcntl called (auto-stub)"); return 0; }

long feof(long a, long b, long c_, long d, long e, long f) { shim_note("feof called (auto-stub)"); return 0; }

long fflush(long a, long b, long c_, long d, long e, long f) { shim_note("fflush called (auto-stub)"); return 0; }

long fgets(long a, long b, long c_, long d, long e, long f) { shim_note("fgets called (auto-stub)"); return 0; }

long fileno(long a, long b, long c_, long d, long e, long f) { shim_note("fileno called (auto-stub)"); return 0; }

long floor(long a, long b, long c_, long d, long e, long f) { shim_note("floor called (auto-stub)"); return 0; }

long floorf(long a, long b, long c_, long d, long e, long f) { shim_note("floorf called (auto-stub)"); return 0; }

long fmax(long a, long b, long c_, long d, long e, long f) { shim_note("fmax called (auto-stub)"); return 0; }

long fmaxf(long a, long b, long c_, long d, long e, long f) { shim_note("fmaxf called (auto-stub)"); return 0; }

long fmin(long a, long b, long c_, long d, long e, long f) { shim_note("fmin called (auto-stub)"); return 0; }

long fminf(long a, long b, long c_, long d, long e, long f) { shim_note("fminf called (auto-stub)"); return 0; }

long fmod(long a, long b, long c_, long d, long e, long f) { shim_note("fmod called (auto-stub)"); return 0; }

long fmodf(long a, long b, long c_, long d, long e, long f) { shim_note("fmodf called (auto-stub)"); return 0; }

long fopen(long a, long b, long c_, long d, long e, long f) { shim_note("fopen called (auto-stub)"); return 0; }

long fprintf(long a, long b, long c_, long d, long e, long f) { shim_note("fprintf called (auto-stub)"); return 0; }

long fputc(long a, long b, long c_, long d, long e, long f) { shim_note("fputc called (auto-stub)"); return 0; }

long fputs(long a, long b, long c_, long d, long e, long f) { shim_note("fputs called (auto-stub)"); return 0; }

long fread(long a, long b, long c_, long d, long e, long f) { shim_note("fread called (auto-stub)"); return 0; }

long free(long a, long b, long c_, long d, long e, long f) { shim_note("free called (auto-stub)"); return 0; }

long freeifaddrs(long a, long b, long c_, long d, long e, long f) { shim_note("freeifaddrs called (auto-stub)"); return 0; }

long frexp(long a, long b, long c_, long d, long e, long f) { shim_note("frexp called (auto-stub)"); return 0; }

long fseek(long a, long b, long c_, long d, long e, long f) { shim_note("fseek called (auto-stub)"); return 0; }

long fstat(long a, long b, long c_, long d, long e, long f) { shim_note("fstat called (auto-stub)"); return 0; }

long fstat$INODE64(long a, long b, long c_, long d, long e, long f) { shim_note("fstat$INODE64 called (auto-stub)"); return 0; }

long ftell(long a, long b, long c_, long d, long e, long f) { shim_note("ftell called (auto-stub)"); return 0; }

long ftruncate(long a, long b, long c_, long d, long e, long f) { shim_note("ftruncate called (auto-stub)"); return 0; }

long fts_children(long a, long b, long c_, long d, long e, long f) { shim_note("fts_children called (auto-stub)"); return 0; }

long fts_close(long a, long b, long c_, long d, long e, long f) { shim_note("fts_close called (auto-stub)"); return 0; }

long fts_open(long a, long b, long c_, long d, long e, long f) { shim_note("fts_open called (auto-stub)"); return 0; }

long fts_read(long a, long b, long c_, long d, long e, long f) { shim_note("fts_read called (auto-stub)"); return 0; }

long fwrite(long a, long b, long c_, long d, long e, long f) { shim_note("fwrite called (auto-stub)"); return 0; }

long fwrite$UNIX2003(long a, long b, long c_, long d, long e, long f) { shim_note("fwrite$UNIX2003 called (auto-stub)"); return 0; }

long getenv(long a, long b, long c_, long d, long e, long f) { shim_note("getenv called (auto-stub)"); return 0; }

long gethostbyname(long a, long b, long c_, long d, long e, long f) { shim_note("gethostbyname called (auto-stub)"); return 0; }

long getifaddrs(long a, long b, long c_, long d, long e, long f) { shim_note("getifaddrs called (auto-stub)"); return 0; }

long getpagesize(long a, long b, long c_, long d, long e, long f) { shim_note("getpagesize called (auto-stub)"); return 0; }

long getpid(long a, long b, long c_, long d, long e, long f) { shim_note("getpid called (auto-stub)"); return 0; }

long gettimeofday(long a, long b, long c_, long d, long e, long f) { shim_note("gettimeofday called (auto-stub)"); return 0; }

long getuid(long a, long b, long c_, long d, long e, long f) { shim_note("getuid called (auto-stub)"); return 0; }

long gmtime_r(long a, long b, long c_, long d, long e, long f) { shim_note("gmtime_r called (auto-stub)"); return 0; }

long host_info(long a, long b, long c_, long d, long e, long f) { shim_note("host_info called (auto-stub)"); return 0; }

long hypot(long a, long b, long c_, long d, long e, long f) { shim_note("hypot called (auto-stub)"); return 0; }

long ilogbf(long a, long b, long c_, long d, long e, long f) { shim_note("ilogbf called (auto-stub)"); return 0; }

long index(long a, long b, long c_, long d, long e, long f) { shim_note("index called (auto-stub)"); return 0; }

long inet_addr(long a, long b, long c_, long d, long e, long f) { shim_note("inet_addr called (auto-stub)"); return 0; }

long inet_ntoa(long a, long b, long c_, long d, long e, long f) { shim_note("inet_ntoa called (auto-stub)"); return 0; }

long ioctl(long a, long b, long c_, long d, long e, long f) { shim_note("ioctl called (auto-stub)"); return 0; }

long kevent(long a, long b, long c_, long d, long e, long f) { shim_note("kevent called (auto-stub)"); return 0; }

long listen(long a, long b, long c_, long d, long e, long f) { shim_note("listen called (auto-stub)"); return 0; }

long localtime_r(long a, long b, long c_, long d, long e, long f) { shim_note("localtime_r called (auto-stub)"); return 0; }

long log(long a, long b, long c_, long d, long e, long f) { shim_note("log called (auto-stub)"); return 0; }

long log10(long a, long b, long c_, long d, long e, long f) { shim_note("log10 called (auto-stub)"); return 0; }

long log10f(long a, long b, long c_, long d, long e, long f) { shim_note("log10f called (auto-stub)"); return 0; }

long log2(long a, long b, long c_, long d, long e, long f) { shim_note("log2 called (auto-stub)"); return 0; }

long log2f(long a, long b, long c_, long d, long e, long f) { shim_note("log2f called (auto-stub)"); return 0; }

long logf(long a, long b, long c_, long d, long e, long f) { shim_note("logf called (auto-stub)"); return 0; }

long lrint(long a, long b, long c_, long d, long e, long f) { shim_note("lrint called (auto-stub)"); return 0; }

long lrintf(long a, long b, long c_, long d, long e, long f) { shim_note("lrintf called (auto-stub)"); return 0; }

long lround(long a, long b, long c_, long d, long e, long f) { shim_note("lround called (auto-stub)"); return 0; }

long lroundf(long a, long b, long c_, long d, long e, long f) { shim_note("lroundf called (auto-stub)"); return 0; }

long lseek(long a, long b, long c_, long d, long e, long f) { shim_note("lseek called (auto-stub)"); return 0; }

long lstat$INODE64(long a, long b, long c_, long d, long e, long f) { shim_note("lstat$INODE64 called (auto-stub)"); return 0; }

long mach_absolute_time(long a, long b, long c_, long d, long e, long f) { shim_note("mach_absolute_time called (auto-stub)"); return 0; }

long mach_error_string(long a, long b, long c_, long d, long e, long f) { shim_note("mach_error_string called (auto-stub)"); return 0; }

long mach_host_self(long a, long b, long c_, long d, long e, long f) { shim_note("mach_host_self called (auto-stub)"); return 0; }

long mach_msg(long a, long b, long c_, long d, long e, long f) { shim_note("mach_msg called (auto-stub)"); return 0; }

long mach_msg_destroy(long a, long b, long c_, long d, long e, long f) { shim_note("mach_msg_destroy called (auto-stub)"); return 0; }

long mach_port_allocate(long a, long b, long c_, long d, long e, long f) { shim_note("mach_port_allocate called (auto-stub)"); return 0; }

long mach_port_deallocate(long a, long b, long c_, long d, long e, long f) { shim_note("mach_port_deallocate called (auto-stub)"); return 0; }

long mach_port_destroy(long a, long b, long c_, long d, long e, long f) { shim_note("mach_port_destroy called (auto-stub)"); return 0; }

long mach_port_insert_right(long a, long b, long c_, long d, long e, long f) { shim_note("mach_port_insert_right called (auto-stub)"); return 0; }

long mach_port_mod_refs(long a, long b, long c_, long d, long e, long f) { shim_note("mach_port_mod_refs called (auto-stub)"); return 0; }

long mach_task_self_(long a, long b, long c_, long d, long e, long f) { shim_note("mach_task_self_ called (auto-stub)"); return 0; }

long mach_thread_self(long a, long b, long c_, long d, long e, long f) { shim_note("mach_thread_self called (auto-stub)"); return 0; }

long mach_timebase_info(long a, long b, long c_, long d, long e, long f) { shim_note("mach_timebase_info called (auto-stub)"); return 0; }

long malloc(long a, long b, long c_, long d, long e, long f) { shim_note("malloc called (auto-stub)"); return 0; }

long memcmp(long a, long b, long c_, long d, long e, long f) { shim_note("memcmp called (auto-stub)"); return 0; }

long memcpy(long a, long b, long c_, long d, long e, long f) { shim_note("memcpy called (auto-stub)"); return 0; }

long memmove(long a, long b, long c_, long d, long e, long f) { shim_note("memmove called (auto-stub)"); return 0; }

long memset(long a, long b, long c_, long d, long e, long f) { shim_note("memset called (auto-stub)"); return 0; }

long memset_pattern16(long a, long b, long c_, long d, long e, long f) { shim_note("memset_pattern16 called (auto-stub)"); return 0; }

long mig_allocate(long a, long b, long c_, long d, long e, long f) { shim_note("mig_allocate called (auto-stub)"); return 0; }

long mig_dealloc_reply_port(long a, long b, long c_, long d, long e, long f) { shim_note("mig_dealloc_reply_port called (auto-stub)"); return 0; }

long mig_deallocate(long a, long b, long c_, long d, long e, long f) { shim_note("mig_deallocate called (auto-stub)"); return 0; }

long mig_get_reply_port(long a, long b, long c_, long d, long e, long f) { shim_note("mig_get_reply_port called (auto-stub)"); return 0; }

long mig_put_reply_port(long a, long b, long c_, long d, long e, long f) { shim_note("mig_put_reply_port called (auto-stub)"); return 0; }

long mk_timer_arm(long a, long b, long c_, long d, long e, long f) { shim_note("mk_timer_arm called (auto-stub)"); return 0; }

long mk_timer_cancel(long a, long b, long c_, long d, long e, long f) { shim_note("mk_timer_cancel called (auto-stub)"); return 0; }

long mk_timer_create(long a, long b, long c_, long d, long e, long f) { shim_note("mk_timer_create called (auto-stub)"); return 0; }

long mk_timer_destroy(long a, long b, long c_, long d, long e, long f) { shim_note("mk_timer_destroy called (auto-stub)"); return 0; }

long mkdir(long a, long b, long c_, long d, long e, long f) { shim_note("mkdir called (auto-stub)"); return 0; }

long mkstemp(long a, long b, long c_, long d, long e, long f) { shim_note("mkstemp called (auto-stub)"); return 0; }

long mktime(long a, long b, long c_, long d, long e, long f) { shim_note("mktime called (auto-stub)"); return 0; }

long mmap(long a, long b, long c_, long d, long e, long f) { shim_note("mmap called (auto-stub)"); return 0; }

long modf(long a, long b, long c_, long d, long e, long f) { shim_note("modf called (auto-stub)"); return 0; }

long mprotect(long a, long b, long c_, long d, long e, long f) { shim_note("mprotect called (auto-stub)"); return 0; }

long msync(long a, long b, long c_, long d, long e, long f) { shim_note("msync called (auto-stub)"); return 0; }

long munmap(long a, long b, long c_, long d, long e, long f) { shim_note("munmap called (auto-stub)"); return 0; }

long open(long a, long b, long c_, long d, long e, long f) { shim_note("open called (auto-stub)"); return 0; }

long perror(long a, long b, long c_, long d, long e, long f) { shim_note("perror called (auto-stub)"); return 0; }

long pow(long a, long b, long c_, long d, long e, long f) { shim_note("pow called (auto-stub)"); return 0; }

long powf(long a, long b, long c_, long d, long e, long f) { shim_note("powf called (auto-stub)"); return 0; }

long pread(long a, long b, long c_, long d, long e, long f) { shim_note("pread called (auto-stub)"); return 0; }

long printf(long a, long b, long c_, long d, long e, long f) { shim_note("printf called (auto-stub)"); return 0; }

long proc_name(long a, long b, long c_, long d, long e, long f) { shim_note("proc_name called (auto-stub)"); return 0; }

long pthread_attr_destroy(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_attr_destroy called (auto-stub)"); return 0; }

long pthread_attr_init(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_attr_init called (auto-stub)"); return 0; }

long pthread_attr_setdetachstate(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_attr_setdetachstate called (auto-stub)"); return 0; }

long pthread_attr_setscope(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_attr_setscope called (auto-stub)"); return 0; }

long pthread_cond_broadcast(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_broadcast called (auto-stub)"); return 0; }

long pthread_cond_destroy(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_destroy called (auto-stub)"); return 0; }

long pthread_cond_init(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_init called (auto-stub)"); return 0; }

long pthread_cond_signal(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_signal called (auto-stub)"); return 0; }

long pthread_cond_timedwait(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_timedwait called (auto-stub)"); return 0; }

long pthread_cond_wait(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_cond_wait called (auto-stub)"); return 0; }

long pthread_create(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_create called (auto-stub)"); return 0; }

long pthread_detach(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_detach called (auto-stub)"); return 0; }

long pthread_main_np(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_main_np called (auto-stub)"); return 0; }

long pthread_mutex_destroy(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutex_destroy called (auto-stub)"); return 0; }

long pthread_mutex_init(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutex_init called (auto-stub)"); return 0; }

long pthread_mutex_lock(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutex_lock called (auto-stub)"); return 0; }

long pthread_mutex_unlock(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutex_unlock called (auto-stub)"); return 0; }

long pthread_mutexattr_destroy(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutexattr_destroy called (auto-stub)"); return 0; }

long pthread_mutexattr_init(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutexattr_init called (auto-stub)"); return 0; }

long pthread_mutexattr_settype(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_mutexattr_settype called (auto-stub)"); return 0; }

long pthread_once(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_once called (auto-stub)"); return 0; }

long pthread_self(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_self called (auto-stub)"); return 0; }

long pthread_setcanceltype(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_setcanceltype called (auto-stub)"); return 0; }

long pthread_setname_np(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_setname_np called (auto-stub)"); return 0; }

long pthread_setschedparam(long a, long b, long c_, long d, long e, long f) { shim_note("pthread_setschedparam called (auto-stub)"); return 0; }

long putchar(long a, long b, long c_, long d, long e, long f) { shim_note("putchar called (auto-stub)"); return 0; }

long puts(long a, long b, long c_, long d, long e, long f) { shim_note("puts called (auto-stub)"); return 0; }

long pwrite(long a, long b, long c_, long d, long e, long f) { shim_note("pwrite called (auto-stub)"); return 0; }

long qsort(long a, long b, long c_, long d, long e, long f) { shim_note("qsort called (auto-stub)"); return 0; }

long qsort_r(long a, long b, long c_, long d, long e, long f) { shim_note("qsort_r called (auto-stub)"); return 0; }

long rand(long a, long b, long c_, long d, long e, long f) { shim_note("rand called (auto-stub)"); return 0; }

long random(long a, long b, long c_, long d, long e, long f) { shim_note("random called (auto-stub)"); return 0; }

long read(long a, long b, long c_, long d, long e, long f) { shim_note("read called (auto-stub)"); return 0; }

long readv(long a, long b, long c_, long d, long e, long f) { shim_note("readv called (auto-stub)"); return 0; }

long realloc(long a, long b, long c_, long d, long e, long f) { shim_note("realloc called (auto-stub)"); return 0; }

long realpath$DARWIN_EXTSN(long a, long b, long c_, long d, long e, long f) { shim_note("realpath$DARWIN_EXTSN called (auto-stub)"); return 0; }

long recv(long a, long b, long c_, long d, long e, long f) { shim_note("recv called (auto-stub)"); return 0; }

long remainder(long a, long b, long c_, long d, long e, long f) { shim_note("remainder called (auto-stub)"); return 0; }

long rint(long a, long b, long c_, long d, long e, long f) { shim_note("rint called (auto-stub)"); return 0; }

long rintf(long a, long b, long c_, long d, long e, long f) { shim_note("rintf called (auto-stub)"); return 0; }

long round(long a, long b, long c_, long d, long e, long f) { shim_note("round called (auto-stub)"); return 0; }

long roundf(long a, long b, long c_, long d, long e, long f) { shim_note("roundf called (auto-stub)"); return 0; }

long sandbox_check(long a, long b, long c_, long d, long e, long f) { shim_note("sandbox_check called (auto-stub)"); return 0; }

long sandbox_extension_issue_file(long a, long b, long c_, long d, long e, long f) { shim_note("sandbox_extension_issue_file called (auto-stub)"); return 0; }

long scalbnf(long a, long b, long c_, long d, long e, long f) { shim_note("scalbnf called (auto-stub)"); return 0; }

long sched_get_priority_max(long a, long b, long c_, long d, long e, long f) { shim_note("sched_get_priority_max called (auto-stub)"); return 0; }

long sched_get_priority_min(long a, long b, long c_, long d, long e, long f) { shim_note("sched_get_priority_min called (auto-stub)"); return 0; }

long select$1050(long a, long b, long c_, long d, long e, long f) { shim_note("select$1050 called (auto-stub)"); return 0; }

long setlocale(long a, long b, long c_, long d, long e, long f) { shim_note("setlocale called (auto-stub)"); return 0; }

long setsockopt(long a, long b, long c_, long d, long e, long f) { shim_note("setsockopt called (auto-stub)"); return 0; }

long sin(long a, long b, long c_, long d, long e, long f) { shim_note("sin called (auto-stub)"); return 0; }

long sinf(long a, long b, long c_, long d, long e, long f) { shim_note("sinf called (auto-stub)"); return 0; }

long sinh(long a, long b, long c_, long d, long e, long f) { shim_note("sinh called (auto-stub)"); return 0; }

long sleep(long a, long b, long c_, long d, long e, long f) { shim_note("sleep called (auto-stub)"); return 0; }

long snprintf(long a, long b, long c_, long d, long e, long f) { shim_note("snprintf called (auto-stub)"); return 0; }

long socket(long a, long b, long c_, long d, long e, long f) { shim_note("socket called (auto-stub)"); return 0; }

long sprintf(long a, long b, long c_, long d, long e, long f) { shim_note("sprintf called (auto-stub)"); return 0; }

long srand(long a, long b, long c_, long d, long e, long f) { shim_note("srand called (auto-stub)"); return 0; }

long srandom(long a, long b, long c_, long d, long e, long f) { shim_note("srandom called (auto-stub)"); return 0; }

long sscanf(long a, long b, long c_, long d, long e, long f) { shim_note("sscanf called (auto-stub)"); return 0; }

long stat(long a, long b, long c_, long d, long e, long f) { shim_note("stat called (auto-stub)"); return 0; }

long stat$INODE64(long a, long b, long c_, long d, long e, long f) { shim_note("stat$INODE64 called (auto-stub)"); return 0; }

long statfs(long a, long b, long c_, long d, long e, long f) { shim_note("statfs called (auto-stub)"); return 0; }

long statfs$INODE64(long a, long b, long c_, long d, long e, long f) { shim_note("statfs$INODE64 called (auto-stub)"); return 0; }

long strchr(long a, long b, long c_, long d, long e, long f) { shim_note("strchr called (auto-stub)"); return 0; }

long strcmp(long a, long b, long c_, long d, long e, long f) { shim_note("strcmp called (auto-stub)"); return 0; }

long strcpy(long a, long b, long c_, long d, long e, long f) { shim_note("strcpy called (auto-stub)"); return 0; }

long strdup(long a, long b, long c_, long d, long e, long f) { shim_note("strdup called (auto-stub)"); return 0; }

long strerror(long a, long b, long c_, long d, long e, long f) { shim_note("strerror called (auto-stub)"); return 0; }

long strlcat(long a, long b, long c_, long d, long e, long f) { shim_note("strlcat called (auto-stub)"); return 0; }

long strlen(long a, long b, long c_, long d, long e, long f) { shim_note("strlen called (auto-stub)"); return 0; }

long strncat(long a, long b, long c_, long d, long e, long f) { shim_note("strncat called (auto-stub)"); return 0; }

long strncmp(long a, long b, long c_, long d, long e, long f) { shim_note("strncmp called (auto-stub)"); return 0; }

long strncpy(long a, long b, long c_, long d, long e, long f) { shim_note("strncpy called (auto-stub)"); return 0; }

long strnstr(long a, long b, long c_, long d, long e, long f) { shim_note("strnstr called (auto-stub)"); return 0; }

long strrchr(long a, long b, long c_, long d, long e, long f) { shim_note("strrchr called (auto-stub)"); return 0; }

long strsep(long a, long b, long c_, long d, long e, long f) { shim_note("strsep called (auto-stub)"); return 0; }

long strtod(long a, long b, long c_, long d, long e, long f) { shim_note("strtod called (auto-stub)"); return 0; }

long strtod$UNIX2003(long a, long b, long c_, long d, long e, long f) { shim_note("strtod$UNIX2003 called (auto-stub)"); return 0; }

long strtod_l(long a, long b, long c_, long d, long e, long f) { shim_note("strtod_l called (auto-stub)"); return 0; }

long strtof(long a, long b, long c_, long d, long e, long f) { shim_note("strtof called (auto-stub)"); return 0; }

long strtol(long a, long b, long c_, long d, long e, long f) { shim_note("strtol called (auto-stub)"); return 0; }

long strtoll(long a, long b, long c_, long d, long e, long f) { shim_note("strtoll called (auto-stub)"); return 0; }

long strtoul(long a, long b, long c_, long d, long e, long f) { shim_note("strtoul called (auto-stub)"); return 0; }

long syscall(long a, long b, long c_, long d, long e, long f) { shim_note("syscall called (auto-stub)"); return 0; }

long sysctl(long a, long b, long c_, long d, long e, long f) { shim_note("sysctl called (auto-stub)"); return 0; }

long sysctlbyname(long a, long b, long c_, long d, long e, long f) { shim_note("sysctlbyname called (auto-stub)"); return 0; }

long tan(long a, long b, long c_, long d, long e, long f) { shim_note("tan called (auto-stub)"); return 0; }

long tanf(long a, long b, long c_, long d, long e, long f) { shim_note("tanf called (auto-stub)"); return 0; }

long tanh(long a, long b, long c_, long d, long e, long f) { shim_note("tanh called (auto-stub)"); return 0; }

long task_info(long a, long b, long c_, long d, long e, long f) { shim_note("task_info called (auto-stub)"); return 0; }

long task_threads(long a, long b, long c_, long d, long e, long f) { shim_note("task_threads called (auto-stub)"); return 0; }

long tcgetattr(long a, long b, long c_, long d, long e, long f) { shim_note("tcgetattr called (auto-stub)"); return 0; }

long tcsetattr(long a, long b, long c_, long d, long e, long f) { shim_note("tcsetattr called (auto-stub)"); return 0; }

long thread_info(long a, long b, long c_, long d, long e, long f) { shim_note("thread_info called (auto-stub)"); return 0; }

long time(long a, long b, long c_, long d, long e, long f) { shim_note("time called (auto-stub)"); return 0; }

long trunc(long a, long b, long c_, long d, long e, long f) { shim_note("trunc called (auto-stub)"); return 0; }

long tzset(long a, long b, long c_, long d, long e, long f) { shim_note("tzset called (auto-stub)"); return 0; }

long unlink(long a, long b, long c_, long d, long e, long f) { shim_note("unlink called (auto-stub)"); return 0; }

long usleep(long a, long b, long c_, long d, long e, long f) { shim_note("usleep called (auto-stub)"); return 0; }

long valloc(long a, long b, long c_, long d, long e, long f) { shim_note("valloc called (auto-stub)"); return 0; }

long vfprintf(long a, long b, long c_, long d, long e, long f) { shim_note("vfprintf called (auto-stub)"); return 0; }

long vm_deallocate(long a, long b, long c_, long d, long e, long f) { shim_note("vm_deallocate called (auto-stub)"); return 0; }

long vm_map(long a, long b, long c_, long d, long e, long f) { shim_note("vm_map called (auto-stub)"); return 0; }

long vm_region_64(long a, long b, long c_, long d, long e, long f) { shim_note("vm_region_64 called (auto-stub)"); return 0; }

long voucher_mach_msg_set(long a, long b, long c_, long d, long e, long f) { shim_note("voucher_mach_msg_set called (auto-stub)"); return 0; }

long write(long a, long b, long c_, long d, long e, long f) { shim_note("write called (auto-stub)"); return 0; }

long writev(long a, long b, long c_, long d, long e, long f) { shim_note("writev called (auto-stub)"); return 0; }

long xpc_connection_cancel(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_connection_cancel called (auto-stub)"); return 0; }

long xpc_connection_create(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_connection_create called (auto-stub)"); return 0; }

long xpc_connection_get_pid(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_connection_get_pid called (auto-stub)"); return 0; }

long xpc_connection_resume(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_connection_resume called (auto-stub)"); return 0; }

long xpc_connection_send_message(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_connection_send_message called (auto-stub)"); return 0; }

long xpc_connection_send_message_with_reply_sync(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_connection_send_message_with_reply_sync called (auto-stub)"); return 0; }

long xpc_connection_set_event_handler(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_connection_set_event_handler called (auto-stub)"); return 0; }

long xpc_connection_set_target_queue(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_connection_set_target_queue called (auto-stub)"); return 0; }

long xpc_dictionary_create(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_dictionary_create called (auto-stub)"); return 0; }

long xpc_dictionary_get_string(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_dictionary_get_string called (auto-stub)"); return 0; }

long xpc_dictionary_get_value(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_dictionary_get_value called (auto-stub)"); return 0; }

long xpc_dictionary_set_bool(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_dictionary_set_bool called (auto-stub)"); return 0; }

long xpc_dictionary_set_value(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_dictionary_set_value called (auto-stub)"); return 0; }

long xpc_get_type(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_get_type called (auto-stub)"); return 0; }

long xpc_mach_send_create(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_mach_send_create called (auto-stub)"); return 0; }

long xpc_mach_send_get_right(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_mach_send_get_right called (auto-stub)"); return 0; }

long xpc_release(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_release called (auto-stub)"); return 0; }

long xpc_retain(long a, long b, long c_, long d, long e, long f) { shim_note("xpc_retain called (auto-stub)"); return 0; }

long dyld_stub_binder(long a, long b, long c_, long d, long e, long f) { shim_note("dyld_stub_binder called (auto-stub)"); return 0; }
