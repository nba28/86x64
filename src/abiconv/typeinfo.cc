#include <clang-c/Index.h>

#include "typeinfo.hh"
#include "util.hh"

size_t reg_width_size(reg_width width) {
   switch (width) {
   case reg_width::B: return 1;
   case reg_width::W: return 2;
   case reg_width::D: return 4;
   case reg_width::Q: return 8;
   default: throw std::invalid_argument("bad register width");
   }
}

const char *reg_width_to_str(reg_width width) {
   switch (width) {
   case reg_width::B: return "byte";
   case reg_width::W: return "word";
   case reg_width::D: return "dword";
   case reg_width::Q: return "qword";
   default: throw std::invalid_argument("bad register width");
   }
}

bool get_type_signed(CXTypeKind type_kind) {
   switch (type_kind) {
   case CXType_Invalid:
   case CXType_Unexposed:
   case CXType_Void:
      throw std::invalid_argument("invalid type");
      
   case CXType_Pointer:
   case CXType_IncompleteArray:
   case CXType_Bool:
   case CXType_UChar:
   case CXType_Char_U:
   case CXType_UShort:
   case CXType_UInt:
   case CXType_ULong:
   case CXType_ULongLong:
   case CXType_BlockPointer:
   case CXType_ObjCObjectPointer:
   case CXType_ObjCId:
   case CXType_ObjCClass:
   case CXType_ObjCSel:
      return false;
      
   case CXType_SChar:
   case CXType_Char_S:
   case CXType_Short:
   case CXType_Int:
   case CXType_Long:
   case CXType_LongLong:
   case CXType_Enum:
   case CXType_Float:
   case CXType_Double:
   case CXType_LongDouble:
      return true;

   case CXType_ConstantArray:
      throw std::invalid_argument("get_type_signed: constant array");

   default:
      throw std::invalid_argument("get_type_signed: unsupported type kind");
   }
}

type_domain get_type_domain(CXTypeKind kind) {
   switch (kind) {
   case CXType_Invalid:
   case CXType_Unexposed:
   case CXType_Void:
      throw std::invalid_argument("invalid type");
      
   case CXType_Pointer:
   case CXType_IncompleteArray:
   case CXType_Bool:
   case CXType_UChar:
   case CXType_Char_U:
   case CXType_UShort:
   case CXType_UInt:
   case CXType_ULong:
   case CXType_ULongLong:
   case CXType_BlockPointer:
   case CXType_SChar:
   case CXType_Char_S:
   case CXType_Short:
   case CXType_Int:
   case CXType_Long:
   case CXType_LongLong:
   case CXType_Enum:
   /* objc object/Class/SEL params are pointers in the GP domain; typeconv
    * marshals them through the proxy-handle bridge (convert_objc_ptr) */
   case CXType_ObjCObjectPointer:
   case CXType_ObjCId:
   case CXType_ObjCClass:
   case CXType_ObjCSel:
      return type_domain::INT;

   case CXType_Float:
   case CXType_Double:
   case CXType_LongDouble:
      return type_domain::REAL;

   case CXType_ConstantArray:
      throw std::invalid_argument("get_type_domain: constant array");

   default:
      /* e.g. CXType_Record (struct/union passed by value): not handled by the
       * register/stack classifier here. Throw so abigen can skip this one
       * function instead of aborting the whole generation run. */
      throw std::invalid_argument("get_type_domain: unsupported type kind (struct-by-value?)");
   }
}

// NOTE: only works for POD types.
reg_width get_type_width(CXTypeKind type_kind, arch a) {
   switch (type_kind) {
   case CXType_Invalid:
   case CXType_Unexposed:
   case CXType_Void:
      throw std::invalid_argument("invalid type");
   case CXType_Bool:
   case CXType_UChar:
   case CXType_Char_U:
   case CXType_SChar:
   case CXType_Char_S:
      return reg_width::B;
   case CXType_Short:
   case CXType_UShort:
      return reg_width::W;
   case CXType_Int:
   case CXType_UInt:
   case CXType_Enum:
   case CXType_Float:
      return reg_width::D;
   case CXType_Long:
   case CXType_ULong:
      return a == arch::i386 ? reg_width::D : reg_width::Q;
   case CXType_LongLong:
   case CXType_ULongLong:
   case CXType_Double:
      return reg_width::Q;

   case CXType_Pointer:
   case CXType_IncompleteArray:
   case CXType_BlockPointer:
   case CXType_ObjCObjectPointer:
   case CXType_ObjCId:
   case CXType_ObjCClass:
   case CXType_ObjCSel:
      return a == arch::i386 ? reg_width::D : reg_width::Q;

   case CXType_LongDouble:
      throw std::invalid_argument("long doubles not supported yet");

   case CXType_ConstantArray:
      throw std::invalid_argument("get_type_width: constant array");

   default:
      throw std::invalid_argument("get_type_width: unsupported type kind");
   }
}

std::ostream& operator<<(std::ostream& os, const CXString& str) {
   os << clang_getCString(str);
   clang_disposeString(str);
   return os;
}

bool operator<(reg_width lhs, reg_width rhs) {
   return reg_width_size(lhs) < reg_width_size(rhs);
}

const char *reg_width_to_sse(reg_width width) {
   switch (width) {
   case reg_width::D: // float
      return "ss";
   case reg_width::Q: // double
      return "sd";
   default:
      throw std::invalid_argument("reg_width_to_sse: unsupported width");
   }
}
