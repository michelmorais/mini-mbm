/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License — see main-lua.mm for full text.                                                                           |
|-----------------------------------------------------------------------------------------------------------------------*/

#import "AppDelegate.h"
#import "MetalViewController.h"

extern "C" void avfoundation_audio_pause(void);
extern "C" void avfoundation_audio_resume(void);

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    (void)application;
    (void)launchOptions;

    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[MetalViewController alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application
{
    (void)application;
    MetalViewController* vc = (MetalViewController*)self.window.rootViewController;
    [vc pauseRendering];
    avfoundation_audio_pause();
}

- (void)applicationDidBecomeActive:(UIApplication*)application
{
    (void)application;
    MetalViewController* vc = (MetalViewController*)self.window.rootViewController;
    [vc resumeRendering];
    avfoundation_audio_resume();
}

@end
