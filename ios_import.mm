//
// ios-only bridge to UIDocumentPicker so roms can be imported into the writable folder,
// the whole file compiles to nothing off iphone
//

#include <TargetConditionals.h>
#if TARGET_OS_IPHONE

#import <UIKit/UIKit.h>

static bool g_import_done = false;

@interface GBDocPicker : NSObject <UIDocumentPickerDelegate>
@property (nonatomic, copy) NSString* destDir;
@end

// kept alive while the picker is on screen, released once it finishes
static GBDocPicker* g_picker_delegate = nil;

@implementation GBDocPicker
- (void)documentPicker:(UIDocumentPickerViewController *)controller
        didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSFileManager* fm = [NSFileManager defaultManager];
    for (NSURL* url in urls) {
        BOOL scoped = [url startAccessingSecurityScopedResource];
        NSString* dest = [self.destDir stringByAppendingPathComponent:[url lastPathComponent]];
        [fm removeItemAtPath:dest error:nil];
        [fm copyItemAtURL:url toURL:[NSURL fileURLWithPath:dest] error:nil];
        if (scoped) [url stopAccessingSecurityScopedResource];
    }
    g_import_done = true;
    g_picker_delegate = nil;
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {
    g_picker_delegate = nil;
}
@end

// walk to the currently visible view controller so the picker can be presented over it
static UIViewController* gb_top_vc() {
    UIViewController* vc = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:[UIWindowScene class]]) {
            for (UIWindow* win in ((UIWindowScene*)scene).windows)
                if (win.isKeyWindow) { vc = win.rootViewController; break; }
        }
        if (vc) break;
    }
    while (vc.presentedViewController) vc = vc.presentedViewController;
    return vc;
}

extern "C" void ios_present_document_picker(const char* destDir) {
    NSString* dest = [NSString stringWithUTF8String:destDir];
    dispatch_async(dispatch_get_main_queue(), ^{
        g_picker_delegate = [GBDocPicker new];
        g_picker_delegate.destDir = dest;
        // import mode hands us a private copy of the file, any file type is allowed
        UIDocumentPickerViewController* picker =
            [[UIDocumentPickerViewController alloc] initWithDocumentTypes:@[@"public.data"]
                                                                    inMode:UIDocumentPickerModeImport];
        picker.delegate = g_picker_delegate;
        picker.allowsMultipleSelection = YES;
        [gb_top_vc() presentViewController:picker animated:YES completion:nil];
    });
}

// true exactly once after an import finishes, so the caller can rescan the folder
extern "C" bool ios_take_import_done() {
    if (g_import_done) { g_import_done = false; return true; }
    return false;
}

#endif // TARGET_OS_IPHONE
