/*
 * Hand-written i386->x86_64 shim for the Carbon Process Manager's
 * GetProcessInformation() (iPhoto 21st blocker: -[AppController alreadyRunning]
 * walks GetNextProcess/GetProcessInformation to detect a second instance).
 *
 * abigen can't generate this one: ProcessInfoRec embeds POINTER fields
 * (processName, processLocation, processAppSpec) so the i386 and x86_64
 * layouts differ in both field offsets and total size (60 vs 80 bytes), and
 * the struct is in/out — the caller pre-sets processInfoLength/processName/
 * processAppSpec and the callee fills the rest. The generated shim handed the
 * x86_64 callee an i386-layout record; HIServices then read the high half of
 * a repacked pointer as its own pointer (crash deref 0x7ff8).
 *
 * Conversion: PSN is two UInt32s (layout-identical) — pass through. Build a
 * native ProcessInfoRec; processName passes through (the caller's Str255
 * buffer is low-4GB and layout-identical). processAppSpec is requested as
 * NULL: the LP64 field is an FSRefPtr, the i386 field an FSSpecPtr, and
 * modern macOS cannot produce an FSSpec at all — the caller's field is left
 * exactly as it set it. Scalar results are copied back narrow.
 *
 * Reached via the ___GetProcessInformation trampoline in maptable_tramp.asm
 * (same export name abigen used, so already-interposed binaries pick this up
 * without re-running static-interpose).
 */

#include <stdint.h>
#include <ApplicationServices/ApplicationServices.h>

/* i386 ProcessInfoRec: 4-byte pointers, no padding (every field 4-aligned). */
struct proc_info_rec_i386 {
   uint32_t processInfoLength;
   uint32_t processName;          /* StringPtr */
   uint32_t processNumber[2];     /* ProcessSerialNumber */
   uint32_t processType;
   uint32_t processSignature;
   uint32_t processMode;
   uint32_t processLocation;      /* Ptr */
   uint32_t processSize;
   uint32_t processFreeMem;
   uint32_t processLauncher[2];   /* ProcessSerialNumber */
   uint32_t processLaunchDate;
   uint32_t processActiveTime;
   uint32_t processAppSpec;       /* FSSpecPtr */
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/* OSErr GetProcessInformation(const ProcessSerialNumber *PSN,
 *                             ProcessInfoRec *info);
 * i386 frame: PSN[0] info[1]. */
int shim_GetProcessInformation(uint32_t *a) {
   const ProcessSerialNumber *psn =
      (const ProcessSerialNumber *)(uintptr_t)a[0];
   struct proc_info_rec_i386 *r32 =
      (struct proc_info_rec_i386 *)(uintptr_t)a[1];
   if (!r32) { return paramErr; }

   ProcessInfoRec r;
   memset(&r, 0, sizeof r);
   r.processInfoLength = sizeof r;
   r.processName       = (StringPtr)(uintptr_t)r32->processName;
   r.processAppRef     = NULL;

   OSErr rc = GetProcessInformation(psn, &r);
   if (rc != noErr) { return rc; }

   r32->processNumber[0]   = r.processNumber.highLongOfPSN;
   r32->processNumber[1]   = r.processNumber.lowLongOfPSN;
   r32->processType        = r.processType;
   r32->processSignature   = r.processSignature;
   r32->processMode        = r.processMode;
   r32->processLocation    = 0;     /* heap zone ptr: meaningless since Carbon */
   r32->processSize        = r.processSize;
   r32->processFreeMem     = r.processFreeMem;
   r32->processLauncher[0] = r.processLauncher.highLongOfPSN;
   r32->processLauncher[1] = r.processLauncher.lowLongOfPSN;
   r32->processLaunchDate  = r.processLaunchDate;
   r32->processActiveTime  = r.processActiveTime;
   /* processInfoLength, processName, processAppSpec: caller's values kept. */
   return rc;
}

#pragma clang diagnostic pop
