#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "iLifeImageAnalysis", s); }

@interface PIECalendarSlicerParams : NSObject
@end
@implementation PIECalendarSlicerParams

@end

@interface PIECompositeScore : NSObject
@end
@implementation PIECompositeScore

@end

@interface PIEGroupTitleParams : NSObject
@end
@implementation PIEGroupTitleParams

@end

@interface PIESlice : NSObject
@end
@implementation PIESlice

@end

@interface PIESlicerParams : NSObject
@end
@implementation PIESlicerParams

@end

@interface PIETabulator : NSObject
@end
@implementation PIETabulator

@end

@interface PhotoInferenceEngine : NSObject
@end
@implementation PhotoInferenceEngine

@end
