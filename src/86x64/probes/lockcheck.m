#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ApplicationServices/ApplicationServices.h>
extern CFDictionaryRef CGSessionCopyCurrentDictionary(void);
int main(void) {
  @autoreleasepool {
    NSDictionary *d = (__bridge_transfer NSDictionary *)CGSessionCopyCurrentDictionary();
    printf("ScreenIsLocked      = %s\n", [[d objectForKey:@"CGSSessionScreenIsLocked"] boolValue] ? "TRUE (synthetic input CANNOT reach apps)" : "false");
    printf("OnConsole           = %s\n", [[d objectForKey:@"kCGSSessionOnConsoleKey"] boolValue] ? "true" : "FALSE");
    printf("AXIsProcessTrusted  = %d\n", AXIsProcessTrusted());
    id u = [d objectForKey:@"kCGSSessionUserNameKey"];
    printf("SessionUser         = %s\n", u ? [[u description] UTF8String] : "(none)");
  }
  return 0;
}
