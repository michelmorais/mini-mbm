/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License — see main-lua.mm for full text.                                                                           |
|-----------------------------------------------------------------------------------------------------------------------*/

#pragma once

#if defined(MBM_PLATFORM_IOS)

#import <UIKit/UIKit.h>
#import "MBMMetalView.h"

// Root view controller for the mini-mbm iOS application.
// Responsibilities:
//   • Owns the MBMMetalView (Metal-backed UIView).
//   • Initialises the mini-mbm engine (LUA_MANAGER) on viewDidLoad.
//   • Drives one engine frame per display refresh via CADisplayLink.
//   • Routes UITouch events to the engine's onTouchDown/Move/Up callbacks.
@interface MetalViewController : UIViewController

@end

#endif // MBM_PLATFORM_IOS
