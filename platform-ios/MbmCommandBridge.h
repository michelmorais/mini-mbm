/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License                                                                                                            |
|                                                                                                                        |
| Copyright (c) 2025 Michel Machado                                                                                      |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated         |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation the   |
| rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to      |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                       |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of the  |
| Software.                                                                                                              |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE  |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.      |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef MBM_COMMAND_BRIDGE_H
#define MBM_COMMAND_BRIDGE_H

#import <UIKit/UIKit.h>

/// Protocol for game-specific command handling.
/// Implement this in your game's MyCommands class (my-commands.m).
/// Return YES if the command was handled; returning NO falls through to the
/// built-in handlers in MbmCommandBridge.
@protocol MbmCommandsProtocol <NSObject>
@optional
- (BOOL)handleCommand:(NSString *)cmd
               param:(NSString *)param
              result:(char *)result
             maxSize:(int32_t)maxSize;
@end

/// Engine-owned command dispatcher for mbm.doCommands().
/// Handles the built-in commands: vibrate, clipboard_read, clipboard_write,
/// openURL, pickFile, share.  Game-specific commands are forwarded first to
/// customDelegate (MyCommands) and only fall through to the built-ins if the
/// delegate returns NO.
@interface MbmCommandBridge : NSObject <UIDocumentPickerDelegate>

/// View controller used to present modal UIKit sheets (file picker, share).
/// Set to self (MetalViewController) in viewDidLoad.
@property (nonatomic, weak) UIViewController *presenter;

/// Game-specific command delegate.  Strong reference — MbmCommandBridge owns
/// this instance.  Assign an instance of MyCommands here.
@property (nonatomic, strong) id<MbmCommandsProtocol> customDelegate;

/// Dispatch a command received from the C++ / Lua side.
/// Called from the ios_command_handler C function in MetalViewController.mm.
- (void)handleCommand:(NSString *)cmd
               param:(NSString *)param
              result:(char *)result
             maxSize:(int32_t)maxSize;

@end

#endif /* MBM_COMMAND_BRIDGE_H */
