/*
 * my-commands.h — game-specific native command handler template.
 *
 * This file is copied to your game's source folder by CMake on first configure.
 * Edit it freely — the engine never overwrites it once it exists.
 *
 * Add your own commands in my-commands.m by checking `cmd` and returning YES
 * when handled.  Return NO to fall through to the built-in handlers
 * (vibrate, clipboard_read, clipboard_write, openURL, pickFile, share).
 */

#ifndef MY_COMMANDS_H
#define MY_COMMANDS_H

#import "MbmCommandBridge.h"

@interface MyCommands : NSObject <MbmCommandsProtocol>
@end

#endif /* MY_COMMANDS_H */
