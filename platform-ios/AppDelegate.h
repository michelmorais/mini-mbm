/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License — see main-lua.mm for full text.                                                                           |
|-----------------------------------------------------------------------------------------------------------------------*/

#pragma once

#if defined(MBM_PLATFORM_IOS)

#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow* window;

@end

#endif // MBM_PLATFORM_IOS
