/*
 * my-commands.m — game-specific native command handler template.
 *
 * This file is copied to your game's source folder by CMake on first configure.
 * Edit it freely — the engine never overwrites it once it exists.
 *
 * Add cases for your game's custom commands here.  Return YES when you have
 * handled a command so the engine stops processing it.  Return NO to fall
 * through to the built-in MbmCommandBridge handlers.
 *
 * Example — Lua side:
 *   mbm.doCommands("myCustomCommand", "someParam")
 *
 * ObjC side:
 *   if ([cmd isEqualToString:@"myCustomCommand"]) {
 *       NSLog(@"Got myCustomCommand with param: %@", param);
 *       return YES;
 *   }
 */

#import "my-commands.h"

@implementation MyCommands

- (BOOL)handleCommand:(NSString *)cmd
               param:(NSString *)param
              result:(char *)result
             maxSize:(int32_t)maxSize
{
    // Add your game-specific commands here.
    // Return YES when handled; return NO to use the built-in handlers.
    (void)cmd; (void)param; (void)result; (void)maxSize;
    return NO;
}

@end
