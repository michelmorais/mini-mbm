/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License — see main-lua.mm for full text.                                                                           |
|-----------------------------------------------------------------------------------------------------------------------*/

#pragma once

#if defined(MBM_PLATFORM_IOS)

#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

// UIView whose backing CALayer is a CAMetalLayer.
// Use +layerClass override so the OS creates the layer type on init.
@interface MBMMetalView : UIView

@property (nonatomic, readonly) CAMetalLayer* metalLayer;

@end

#endif // MBM_PLATFORM_IOS
