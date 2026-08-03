/* 97_dynamic_cast_crosscast — does dynamic_cast still work under translation for
 * the shapes a 2006 C++ game actually uses?
 *
 * WHY THIS EXISTS. After the display/fullscreen bridge (`c56bc2a`) Halo's next
 * wall is `Halo.dylib+0x3ac37c`: it virtual-calls a NULL `IDirect3DTexture9_Mac`
 * global that is assigned ONLY from a `dynamic_cast` (and assigned NULL when the
 * source is NULL). So either the texture was legitimately NULL upstream, or
 * **our dynamic_cast returns NULL when it should not** — and the second would be
 * a translator defect with reach far beyond Halo, since every C++ target that
 * downcasts through an interface hits it.
 *
 * The existing C++ coverage does NOT settle this. `96_rtti_const_vtable_bind`
 * guards the __DATA,__const typeinfo BIND (that the __name field is not clobbered
 * by an 8-byte write); `68_cpp_typeinfo_name` guards typeinfo NAME reads. Neither
 * ever performs a dynamic_cast, so the RUNTIME cast path — __dynamic_cast walking
 * the __class_type_info hierarchy — is untested. This guard tests exactly that,
 * and deliberately covers the several distinct shapes, because they take
 * DIFFERENT paths inside the ABI runtime and a defect can hit one and spare
 * another:
 *
 *   [1] plain downcast  base -> derived         (__si_class_type_info walk)
 *   [2] CROSS-CAST      base A -> sibling base B under multiple inheritance
 *                                               (__vmi_class_type_info walk, and
 *                                                the one that requires a nonzero
 *                                                THIS-POINTER ADJUSTMENT — the
 *                                                classic place a translated cast
 *                                                silently returns NULL)
 *   [3] downcast through a VIRTUAL base         (vbase offset in the vtable)
 *   [4] a cast that MUST fail                   (guards against a "fix" that
 *                                                makes everything succeed)
 *   [5] dynamic_cast<void*>                     (complete-object address)
 *
 * ★[2] is the shape that matters most here: `IDirect3DTexture9_Mac` deriving from
 * both a D3D interface and a platform base is exactly the 2006 COM-style idiom,
 * and a cross-cast is the case where the returned pointer is NOT the same address
 * as the source. If the translator loses that adjustment the result is either
 * NULL or a wild pointer — and a wild `this` that is then virtual-called is
 * precisely `Halo.dylib+0x3ac37c`.
 *
 * Needs `make sysroot-cpp` (RTTI + the C++ runtime). Output is diffed against
 * expected/97_dynamic_cast_crosscast.txt.
 */
#include <cstdio>
#include <cstdlib>

/* ---- [1] simple single-inheritance chain ------------------------------- */
struct Base            { virtual ~Base() {} virtual int who() { return 1; } };
struct Derived : Base  { int extra = 0x11; int who() override { return 2; } };

/* ---- [2] the COM-style shape: two unrelated bases, one concrete class ---
 * IFace is the "interface" the engine hands around; Plat is the platform half.
 * A cross-cast IFace* -> Plat* must adjust the pointer by a nonzero offset. */
struct IFace           { virtual ~IFace() {} virtual int iface_tag() { return 10; } };
struct Plat            { virtual ~Plat()  {} virtual int plat_tag()  { return 20; }
                         int payload = 0x22; };
struct Concrete : IFace, Plat { int plat_tag() override { return 21; } };

/* ---- [3] virtual base --------------------------------------------------- */
struct VBase           { virtual ~VBase() {} int vb = 0x33; };
struct VLeft  : virtual VBase {};
struct VRight : virtual VBase {};
struct VJoin  : VLeft, VRight {};

/* ---- [4] an unrelated polymorphic type for the must-fail cast ----------- */
struct Stranger        { virtual ~Stranger() {} };

static void say(const char *tag, bool ok) { printf("%-16s %s\n", tag, ok ? "ok" : "FAIL"); }

int main() {
   /* [1] plain downcast */
   Base *b = new Derived();
   Derived *d = dynamic_cast<Derived *>(b);
   say("downcast", d != nullptr && d->who() == 2 && d->extra == 0x11);

   /* [2] CROSS-CAST — the pointer MUST change, and the object must still work.
    * Checked three ways: non-NULL, the adjustment actually happened, and a
    * virtual call through the result dispatches to the right override (a wild
    * `this` would return garbage or crash rather than 21). */
   Concrete *cc = new Concrete();
   IFace *ifp = cc;                       /* implicit upcast to the interface   */
   Plat  *pp  = dynamic_cast<Plat *>(ifp);   /* the cross-cast under test       */
   const bool cross_nonnull = (pp != nullptr);
   const bool cross_adjusted =
       cross_nonnull && ((void *)pp != (void *)ifp);   /* nonzero this-adjust   */
   const bool cross_usable =
       cross_nonnull && pp->plat_tag() == 21 && pp->payload == 0x22;
   say("crosscast", cross_nonnull);
   say("cross-adjust", cross_adjusted);
   say("cross-usable", cross_usable);

   /* [3] downcast through a virtual base */
   VBase *vb = new VJoin();
   VJoin *vj = dynamic_cast<VJoin *>(vb);
   say("virtual-base", vj != nullptr && vj->vb == 0x33);

   /* [4] a cast that must FAIL — a runtime that returns non-NULL for everything
    * would pass every other check here while being catastrophically wrong. */
   Base *plain = new Base();
   say("must-fail", dynamic_cast<Derived *>(plain) == nullptr);
   say("must-fail-2", dynamic_cast<Stranger *>(b) == nullptr);

   /* [5] dynamic_cast<void*> yields the COMPLETE object address, which for an
    * interface sub-object is NOT the interface pointer. */
   void *complete = dynamic_cast<void *>(ifp);
   say("void-complete", complete == (void *)cc);

   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
