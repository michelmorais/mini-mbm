/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License — see main-lua.mm for full text.                                                                           |
|-----------------------------------------------------------------------------------------------------------------------*/

#import "MBMMetalView.h"

@implementation MBMMetalView

// Override +layerClass so UIKit creates a CAMetalLayer as the backing layer.
+ (Class)layerClass
{
    return [CAMetalLayer class];
}

- (CAMetalLayer*)metalLayer
{
    return (CAMetalLayer*)self.layer;
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    // Keep the drawable size in sync with the view's actual pixel dimensions.
    // This is called whenever the view's bounds change (rotation, split-screen…).
    const CGFloat scale          = self.contentScaleFactor;
    self.metalLayer.drawableSize = CGSizeMake(self.bounds.size.width  * scale,
                                              self.bounds.size.height * scale);
}

@end
