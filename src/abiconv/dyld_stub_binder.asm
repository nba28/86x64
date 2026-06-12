;; dyld_stub_binder.asm — thread-safe lazy-bind entry for translated images.
;;
;; A translated image's stub helper jumps here (static-interpose rewires its
;; dyld_stub_binder import). On entry:
;;   [rsp+0]  = &image's __dyld_private (pushed by the helper head)
;;   [rsp+8]  = lazy bind info offset   (pushed by the per-stub `push imm32`)
;;   [rsp+16] = the i386 caller frame: 4-byte return address, then 4-byte args
;;
;; The OLD protocol parked rsp in the process-wide __dyld_stub_binder_flag,
;; tail-jumped into the real dyld_stub_binder (which jumps straight to the
;; bound target), and relied on the target shim's preamble to repair rsp from
;; the global. That global raced across threads: a second thread binding (or
;; merely entering any shim) adopted the first thread's parked stack — the
;; 23rd-blocker crash family (wild pcs assembled from string/code bytes,
;; proxy-arena handles leaking into native msgSend receivers).
;;
;; NOW everything stays on the calling thread's own stack: resolve the symbol
;; via the C helper (which also writes the lazy pointer), restore the i386
;; frame ourselves, and jump to the target. The target's preamble sees a
;; perfectly ordinary i386 call frame (and a flag that is always 0).
;;
;; Register discipline: the interrupted operation is an i386 `call` into the
;; stub, so eax/ecx/edx are dead (i386 caller-saved) and ebx/esi/edi/ebp must
;; survive. rsi/rdi are SysV caller-saved, so save them around the C call;
;; rbx/rbp are callee-saved both ways. r12-r15 survive the SysV call; r8-r11
;; are clobbered, which every shim call already does (prep runs C code).

   segment .text
   global __dyld_stub_binder
   global __dyld_stub_binder_flag
   extern _x64_lazy_bind_helper

__dyld_stub_binder:
   push rbp
   mov rbp, rsp                 ; rbp = entry_rsp - 8 (anchor)
   push rbx
   push rsi                     ; i386 callee-saved, SysV caller-saved
   push rdi
   mov rdi, [rbp + 8]           ; &__dyld_private   (entry [rsp+0])
   mov rsi, [rbp + 16]          ; lazy bind offset  (entry [rsp+8])
   and rsp, ~0xf                ; align for the call
   call _x64_lazy_bind_helper   ; rax = target (aborts loudly on failure)
   lea rsp, [rbp - 24]          ; -> saved rdi
   pop rdi
   pop rsi
   pop rbx
   pop rbp                      ; rsp = entry rsp
   add rsp, 16                  ; drop the two helper qwords -> i386 ret32
   jmp rax

   segment .data
;; Always 0 now. Every shim preamble (abigen output, objc_msgSend.asm,
;; getopt.asm, exec.asm, sigaction.asm, vararg-conv-t.asm, maptable_tramp.asm)
;; still checks it on entry, which is a harmless no-op; kept so those externs
;; keep linking. Do NOT revive the old store-rsp-here protocol: it is
;; process-global and therefore racy by construction.
__dyld_stub_binder_flag: dq 0
