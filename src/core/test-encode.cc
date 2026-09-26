extern "C" {
#include <xed/xed-interface.h>
}
#include <cstdio>

int main(void) {
   xed_tables_init();


   xed_encoder_request_t xedd;
   xed_encoder_request_zero(&xedd);

   xed_encoder_request_set_iclass(&xedd, XED_ICLASS_MOV);
   xed_encoder_request_set_reg(&xedd, XED_OPERAND_REG0, XED_REG_EAX);
   xed_encoder_request_set_reg(&xedd, XED_OPERAND_REG1, XED_REG_EAX);

   uint8_t buf[16];
   xed_error_enum_t err;
   unsigned len;
   if ((err = xed_encode(&xedd, buf, 16, &len)) != XED_ERROR_NONE) {
      fprintf(stderr, "xed_encode: %s\n", xed_error_enum_t2str(err));
      return 2;
   }

   for (int i = 0; i < len; ++i) {
      printf("%hhx ", buf[i]);
   }
   printf("\n");

   return 0;
}
