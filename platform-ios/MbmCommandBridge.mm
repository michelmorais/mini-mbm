/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License                                                                                                            |
|                                                                                                                        |
| Copyright (c) 2026 Michel Braz de Morais                                                                               |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation the    |
| rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to        |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                        |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of the   |
| Software.                                                                                                              |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------*/

#import "MbmCommandBridge.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

/// Called from the engine thread when an async UIKit operation completes.
/// Defined in MetalViewController.mm.
extern "C" void mbm_ios_onCallBackCommands(const char *cmdName, const char *result);

// ---------------------------------------------------------------------------
@implementation MbmCommandBridge

- (void)handleCommand:(NSString *)cmd
               param:(NSString *)param
              result:(char *)result
             maxSize:(int32_t)maxSize
{
    // 1. Let the game-specific delegate handle it first.
    if (self.customDelegate &&
        [self.customDelegate respondsToSelector:@selector(handleCommand:param:result:maxSize:)])
    {
        if ([self.customDelegate handleCommand:cmd param:param result:result maxSize:maxSize])
            return;
    }

    // 2. Built-in command handlers.
    if ([cmd isEqualToString:@"vibrate"])
    {
        dispatch_async(dispatch_get_main_queue(), ^{
            UIImpactFeedbackGenerator *gen =
                [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
            [gen impactOccurred];
        });
    }
    else if ([cmd isEqualToString:@"clipboard_read"])
    {
        NSString *text = [UIPasteboard generalPasteboard].string ?: @"";
        strncpy(result, text.UTF8String, (size_t)(maxSize - 1));
        result[maxSize - 1] = '\0';
    }
    else if ([cmd isEqualToString:@"clipboard_write"])
    {
        [UIPasteboard generalPasteboard].string = param;
    }
    else if ([cmd isEqualToString:@"openURL"])
    {
        dispatch_async(dispatch_get_main_queue(), ^{
            NSURL *url = [NSURL URLWithString:param];
            if (url)
                [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
        });
    }
    else if ([cmd isEqualToString:@"pickFile"])
    {
        NSString *utTypeString = [param copy];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self presentFilePicker:utTypeString];
        });
    }
    else if ([cmd isEqualToString:@"share"])
    {
        NSString *shareParam = [param copy];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self presentShareSheet:shareParam];
        });
    }
}

// ---------------------------------------------------------------------------
#pragma mark - Private helpers

- (void)presentFilePicker:(NSString *)utTypeString
{
    UIDocumentPickerViewController *picker;
    if (@available(iOS 14.0, *))
    {
        UTType *utType = [UTType typeWithIdentifier:utTypeString];
        NSArray<UTType *> *types = utType ? @[utType] : @[UTTypeItem];
        picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:types];
    }
    else
    {
        NSArray<NSString *> *docTypes = utTypeString.length ? @[utTypeString] : @[@"public.item"];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        picker = [[UIDocumentPickerViewController alloc]
                      initWithDocumentTypes:docTypes
                                     inMode:UIDocumentPickerModeOpen];
#pragma clang diagnostic pop
    }
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self.presenter presentViewController:picker animated:YES completion:nil];
}

- (void)presentShareSheet:(NSString *)param
{
    NSString *shareText = param.length ? param : @"";
    UIActivityViewController *vc =
        [[UIActivityViewController alloc] initWithActivityItems:@[shareText]
                                          applicationActivities:nil];
    vc.completionWithItemsHandler =
        ^(UIActivityType _Nullable type, BOOL completed,
          NSArray *_Nullable items, NSError *_Nullable error) {
            mbm_ios_onCallBackCommands("share", "done");
        };

    // iPad requires a popover source.
    UIPopoverPresentationController *pop = vc.popoverPresentationController;
    if (pop)
    {
        pop.sourceView = self.presenter.view;
        CGSize sz = self.presenter.view.bounds.size;
        pop.sourceRect = CGRectMake(sz.width / 2.0, sz.height / 2.0, 0, 0);
    }

    [self.presenter presentViewController:vc animated:YES completion:nil];
}

// ---------------------------------------------------------------------------
#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController *)controller
didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
    const char *path = urls.firstObject ? urls.firstObject.path.UTF8String : "";
    mbm_ios_onCallBackCommands("pickFile", path);
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller
{
    mbm_ios_onCallBackCommands("pickFile", "");
}

@end
