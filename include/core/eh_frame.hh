#pragma once

/* DWARF EH decoding over raw __eh_frame / __gcc_except_tab bytes (i386
 * source images): CIE/FDE walk and the LSDA call-site table. */

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace MachO {

   struct CieInfo { bool has_aug=false, has_L=false, has_R=false; uint8_t L_enc=0, R_enc=0; };

   inline uint32_t ehbuf_le32(const std::vector<uint8_t>& b, size_t o) {
      return (uint32_t)b[o] | ((uint32_t)b[o+1]<<8) |
             ((uint32_t)b[o+2]<<16) | ((uint32_t)b[o+3]<<24);
   }
   inline uint16_t ehbuf_le16(const std::vector<uint8_t>& b, size_t o) {
      return (uint16_t)(b[o] | (b[o+1]<<8));
   }
   inline uint64_t eh_uleb(const std::vector<uint8_t>& b, size_t& o) {
      uint64_t r=0; int s=0; uint8_t c;
      do { c=b[o++]; r |= (uint64_t)(c&0x7f)<<s; s+=7; } while ((c&0x80) && o<b.size());
      return r;
   }
   inline int64_t eh_sleb(const std::vector<uint8_t>& b, size_t& o) {
      int64_t r=0; int s=0; uint8_t c;
      do { c=b[o++]; r |= (int64_t)(c&0x7f)<<s; s+=7; } while ((c&0x80) && o<b.size());
      if (s<64 && (c&0x40)) r |= -((int64_t)1<<s);
      return r;
   }
   inline int eh_enc_size(uint8_t enc) {       /* fixed-size encodings (i386 absptr=4) */
      switch (enc & 0x0f) {
      case 0x00: case 0x03: case 0x0b: return 4;   /* absptr/udata4/sdata4 */
      case 0x02: case 0x0a: return 2;              /* udata2/sdata2 */
      case 0x04: case 0x0c: return 8;              /* udata8/sdata8 */
      default: return -1;                          /* uleb/sleb -> variable */
      }
   }
   /* Decode an EH-encoded value; field_vmaddr = vmaddr of its first byte.
    * Returns false (skip) for omit / indirect / unsupported relativity. */
   inline bool eh_read_encoded(const std::vector<uint8_t>& b, size_t& o, uint8_t enc,
                               uint32_t field_vmaddr, uint32_t& out) {
      if (enc == 0xff) return false;               /* DW_EH_PE_omit */
      uint32_t raw;
      switch (enc & 0x0f) {
      case 0x00: case 0x03: raw = ehbuf_le32(b,o); o+=4; break;          /* absptr/udata4 */
      case 0x0b: raw = ehbuf_le32(b,o); o+=4; break;                     /* sdata4 (2's-comp) */
      case 0x02: raw = ehbuf_le16(b,o); o+=2; break;                     /* udata2 */
      case 0x0a: raw = (uint32_t)(int16_t)ehbuf_le16(b,o); o+=2; break;  /* sdata2 */
      case 0x01: raw = (uint32_t)eh_uleb(b,o); break;                    /* uleb */
      case 0x09: raw = (uint32_t)eh_sleb(b,o); break;                    /* sleb */
      default: return false;
      }
      if (enc & 0x80) return false;                /* indirect: can't deref here */
      switch (enc & 0x70) {
      case 0x10: out = field_vmaddr + raw; return true;   /* pcrel (raw 2's-comp) */
      case 0x00: out = raw; return true;                  /* absptr */
      default: return false;                              /* datarel/funcrel/etc. */
      }
   }
   /* Does a well-formed GCC LSDA header start at gbuf[off]? */
   inline bool eh_lsda_valid_at(const std::vector<uint8_t>& gbuf, size_t off) {
      if (off >= gbuf.size()) return false;
      size_t p = off;
      uint8_t lpenc = gbuf[p++];
      if (lpenc != 0xff && (lpenc & 0x0f) > 0x0c) return false;
      if (lpenc != 0xff) { int s = eh_enc_size(lpenc); if (s < 0) eh_uleb(gbuf, p); else p += s; }
      if (p >= gbuf.size()) return false;
      uint8_t ttenc = gbuf[p++];
      if (ttenc != 0xff) { eh_uleb(gbuf, p); }            /* @TType base offset */
      if (p >= gbuf.size()) return false;
      uint8_t csenc = gbuf[p++];
      if ((csenc & 0x0f) > 0x0c) return false;
      if (p >= gbuf.size()) return false;
      uint64_t cslen = eh_uleb(gbuf, p);
      if (p + cslen > gbuf.size()) return false;          /* table must fit */
      return cslen > 0;
   }

   /* Every FDE of an __eh_frame image whose CIE has a pointer encoding:
    * f(pc_begin, has_lsda, lsda). buf[0] is at vmaddr `base`. */
   template <typename F>
   void eh_for_each_fde(const std::vector<uint8_t>& buf, uint32_t base, F f) {
      std::map<size_t, CieInfo> cies;
      size_t i = 0;
      while (i + 4 <= buf.size()) {
         const uint32_t len = ehbuf_le32(buf, i);
         if (len == 0) { break; }                  /* terminator */
         const size_t after_len = i + 4;
         const size_t rec_end = after_len + len;
         if (rec_end > buf.size()) { break; }
         const uint32_t id = ehbuf_le32(buf, after_len);
         if (id == 0) {                             /* CIE */
            CieInfo ci;
            size_t p = after_len + 4;
            const uint8_t version = buf[p++];
            std::string aug;
            while (p < rec_end && buf[p] != 0) { aug.push_back((char)buf[p++]); }
            if (p < rec_end) { p++; }               /* skip NUL */
            ci.has_aug = (!aug.empty() && aug[0] == 'z');
            eh_uleb(buf, p);                        /* code alignment factor */
            eh_sleb(buf, p);                        /* data alignment factor */
            if (version == 1) { p++; } else { eh_uleb(buf, p); } /* return reg */
            if (ci.has_aug) {
               eh_uleb(buf, p);                     /* augmentation data length */
               for (size_t k = 1; k < aug.size(); k++) {
                  switch (aug[k]) {
                  case 'L': ci.L_enc = buf[p++]; ci.has_L = true; break;
                  case 'R': ci.R_enc = buf[p++]; ci.has_R = true; break;
                  case 'P': { uint8_t pe = buf[p++]; int sz = eh_enc_size(pe);
                              if (sz < 0) { eh_uleb(buf, p); } else { p += sz; } break; }
                  case 'S': break;                  /* signal frame, no data */
                  default: break;
                  }
               }
            }
            cies[i] = ci;
         } else {                                   /* FDE */
            const size_t cie_off = (id <= after_len) ? after_len - id : SIZE_MAX;
            auto it = cies.find(cie_off);
            if (it != cies.end() && it->second.has_R) {
               const CieInfo& ci = it->second;
               size_t p = after_len + 4;            /* after CIE pointer */
               uint32_t pcbegin = 0;
               if (eh_read_encoded(buf, p, ci.R_enc, base + (uint32_t)p, pcbegin)) {
                  int rsz = eh_enc_size(ci.R_enc);  /* skip pc-range */
                  if (rsz < 0) { eh_uleb(buf, p); } else { p += rsz; }
                  uint32_t lsda = 0;
                  bool has_lsda = false;
                  if (ci.has_aug) {
                     eh_uleb(buf, p);               /* FDE aug data length */
                     if (ci.has_L) {
                        has_lsda = eh_read_encoded(buf, p, ci.L_enc, base + (uint32_t)p, lsda) &&
                                   lsda != 0;
                     }
                  }
                  f(pcbegin, has_lsda, lsda);
               }
            }
         }
         i = rec_end;
      }
   }

   /* The call-site table of the LSDA at gbuf[off] (vmaddr gbase + off) for the
    * function at `func`: f(start, length, landing_pad) per record, absolute
    * vmaddrs (landing_pad 0: no handler). False if the header is malformed. */
   template <typename F>
   bool eh_for_each_call_site(const std::vector<uint8_t>& gbuf, size_t off, uint32_t gbase,
                              uint32_t func, F f) {
      if (!eh_lsda_valid_at(gbuf, off)) { return false; }
      size_t p = off;
      const uint8_t lpenc = gbuf[p++];
      uint32_t lpstart = func;
      if (lpenc != 0xff && !eh_read_encoded(gbuf, p, lpenc, gbase + (uint32_t)p, lpstart)) { return false; }
      const uint8_t ttenc = gbuf[p++];
      if (ttenc != 0xff) { eh_uleb(gbuf, p); }
      const uint8_t csenc = gbuf[p++];
      const size_t end = p + eh_uleb(gbuf, p);
      if ((csenc & 0x70) != 0) { return false; }   /* call-site fields are offsets */
      while (p < end) {
         uint32_t start, len, lp;
         if (!eh_read_encoded(gbuf, p, csenc, 0, start) || !eh_read_encoded(gbuf, p, csenc, 0, len) ||
             !eh_read_encoded(gbuf, p, csenc, 0, lp) || p > end) {
            return false;
         }
         eh_uleb(gbuf, p);                         /* action */
         f(lpstart + start, len, lp == 0 ? 0 : lpstart + lp);
      }
      return true;
   }

}
