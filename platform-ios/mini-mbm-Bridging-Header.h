/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2026 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| See LICENSE.md in the engine root for full license text.                                                               |
|                                                                                                                        |
| ENGINE FILE — do not modify in your game project.                                                                      |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef MINI_MBM_BRIDGING_HEADER_H
#define MINI_MBM_BRIDGING_HEADER_H

/*
 * Objective-C / C symbols exported to Swift for the MiniMbm module.
 *
 * This file is the SWIFT_OBJC_BRIDGING_HEADER set by CMake via
 *   XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER
 * It is compiled once by Xcode and made available to all Swift sources in
 * the project.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Route an async native-command result back into the Lua scene.
 *
 * Call this from Swift after an async UIKit operation (file picker, share
 * sheet, etc.) completes.  The engine dispatches to the Lua global function
 * named by @p cmdName, passing @p result as its string argument.
 *
 * @param cmdName  Name of the Lua global callback (e.g. "pickFile", "share").
 * @param result   Result string passed as the function's argument.
 */
void mbm_ios_onCallBackCommands(const char * _Nonnull cmdName,
                                const char * _Nonnull result);

#ifdef __cplusplus
}
#endif

#endif /* MINI_MBM_BRIDGING_HEADER_H */
