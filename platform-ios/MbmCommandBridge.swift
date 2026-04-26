/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2026 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| See LICENSE.md in the engine root for full license text.                                                               |
|                                                                                                                        |
| ENGINE FILE — do not modify this file in your game project.                                                            |
| Add your custom native iOS commands in my-commands.swift instead.                                                      |
|-----------------------------------------------------------------------------------------------------------------------*/

import UIKit
import UniformTypeIdentifiers

// ---------------------------------------------------------------------------
// Protocol — implemented by MyCommands in your game project
// ---------------------------------------------------------------------------

/// Implement this protocol in MyCommands (my-commands.swift, copied to your
/// game folder by CMake) to handle custom native iOS commands sent via
/// mbm.doCommands().
@objc public protocol MbmCommandsProtocol: AnyObject {

    /// Handle a game-specific native command.
    ///
    /// - Parameters:
    ///   - command:  The command name passed to mbm.doCommands().
    ///   - param:    The parameter string passed to mbm.doCommands().
    ///   - result:   Buffer to write a synchronous return value into.
    ///   - maxSize:  Maximum bytes that may be written to result (including NUL).
    /// - Returns: `true` if the command was handled; `false` to log a warning.
    @objc optional func handleCommand(
        _ command: String,
        param: String,
        result: UnsafeMutablePointer<CChar>,
        maxSize: Int32
    ) -> Bool
}

// ---------------------------------------------------------------------------
// MbmCommandBridge — engine iOS command dispatcher
// ---------------------------------------------------------------------------

/// Routes mbm.doCommands() calls from C++ to UIKit.
///
/// Instantiated by MetalViewController and registered as the engine's
/// native-command handler.  Game-specific commands are delegated to
/// customDelegate (an instance of MyCommands from your game project).
///
/// Do NOT subclass or modify this class.
/// Use my-commands.swift (copied to your game folder by CMake) instead.
@objc public class MbmCommandBridge: NSObject {

    /// The view controller used to present sheets.
    /// Set to self by MetalViewController in viewDidLoad.
    @objc public weak var presenter: UIViewController?

    /// Delegate for game-specific commands.
    /// Set to an instance of MyCommands by MetalViewController in viewDidLoad.
    /// Strong reference — MbmCommandBridge owns this instance.
    @objc public var customDelegate: MbmCommandsProtocol?

    // -------------------------------------------------------------------------
    // Main dispatch — called from C++ via the ObjC bridge in MetalViewController
    // -------------------------------------------------------------------------

    /// Dispatch a native command received from Lua via mbm.doCommands().
    ///
    /// Synchronous commands fill the result buffer before returning.
    /// Async commands (pickFile, share) return with an empty result immediately;
    /// the Lua callback fires later via mbm_ios_onCallBackCommands().
    @objc public func handleCommand(
        _ command: String,
        param: String,
        result: UnsafeMutablePointer<CChar>,
        maxSize: Int32
    ) {
        switch command {

        // ── Haptic feedback ──────────────────────────────────────────────────
        // param: ignored (always medium impact)
        case "vibrate":
            DispatchQueue.main.async {
                UIImpactFeedbackGenerator(style: .medium).impactOccurred()
            }

        // ── Clipboard read ───────────────────────────────────────────────────
        // Returns the current clipboard string synchronously.
        // Lua: local text = mbm.doCommands("clipboard_read", "")
        case "clipboard_read":
            let text = UIPasteboard.general.string ?? ""
            fillResult(result, maxSize: maxSize, value: text)

        // ── Clipboard write ──────────────────────────────────────────────────
        // param: string to copy to the clipboard.
        // Lua: mbm.doCommands("clipboard_write", "hello")
        case "clipboard_write":
            UIPasteboard.general.string = param

        // ── Open URL ─────────────────────────────────────────────────────────
        // param: URL string (https://, tel:, mailto:, etc.)
        // Lua: mbm.doCommands("openURL", "https://example.com")
        case "openURL":
            guard let url = URL(string: param) else {
                NSLog("[mini-mbm] openURL: invalid URL: %@", param)
                return
            }
            DispatchQueue.main.async {
                UIApplication.shared.open(url)
            }

        // ── File picker (async) ──────────────────────────────────────────────
        // param: UTType identifier string, e.g. "public.item" (any),
        //        "public.image", "public.plain-text", "com.adobe.pdf", etc.
        // Result arrives via the Lua global:  function pickFile(path) ... end
        // An empty path means the user cancelled.
        // Lua: mbm.doCommands("pickFile", "public.item")
        case "pickFile":
            let utTypeStr = param.isEmpty ? "public.item" : param
            presentFilePicker(utTypeString: utTypeStr)

        // ── Share sheet (async) ──────────────────────────────────────────────
        // param: text to share OR an absolute file path to share.
        // Result arrives via the Lua global:  function share(status) ... end
        //   status = "done" when the sheet is dismissed (any outcome).
        // Lua: mbm.doCommands("share", "Check out my score!")
        //      mbm.doCommands("share", "/path/to/screenshot.png")
        case "share":
            presentShareSheet(param: param)

        // ── Game-specific / custom ────────────────────────────────────────────
        default:
            let handled = customDelegate?.handleCommand?(
                command,
                param: param,
                result: result,
                maxSize: maxSize
            ) ?? false
            if !handled {
                NSLog("[mini-mbm] doCommands: unhandled command '%@'", command)
            }
        }
    }

    // -------------------------------------------------------------------------
    // Private helpers
    // -------------------------------------------------------------------------

    /// Copy a Swift string into a C char buffer, NUL-terminated, clamped to maxSize.
    private func fillResult(
        _ buf: UnsafeMutablePointer<CChar>,
        maxSize: Int32,
        value: String
    ) {
        guard maxSize > 0 else { return }
        let limit = Int(maxSize) - 1
        let bytes = Array(value.utf8.prefix(limit))
        for (i, b) in bytes.enumerated() {
            buf[i] = CChar(bitPattern: b)
        }
        buf[bytes.count] = 0
    }

    /// Present UIDocumentPickerViewController so the user can pick a file.
    private func presentFilePicker(utTypeString: String) {
        guard let vc = presenter else {
            NSLog("[mini-mbm] pickFile: no presenter view controller")
            return
        }
        let contentTypes: [UTType] = [UTType(utTypeString) ?? .item]
        DispatchQueue.main.async { [weak self] in
            guard let self = self else { return }
            let picker = UIDocumentPickerViewController(
                forOpeningContentTypes: contentTypes)
            picker.allowsMultipleSelection = false
            picker.delegate = self
            vc.present(picker, animated: true)
        }
    }

    /// Present UIActivityViewController to share text or a file.
    private func presentShareSheet(param: String) {
        guard let vc = presenter else {
            NSLog("[mini-mbm] share: no presenter view controller")
            return
        }
        DispatchQueue.main.async {
            // Use a file URL when param looks like an absolute path that exists.
            let item: Any
            if param.hasPrefix("/"), FileManager.default.fileExists(atPath: param) {
                item = URL(fileURLWithPath: param)
            } else {
                item = param
            }
            let sheet = UIActivityViewController(
                activityItems: [item],
                applicationActivities: nil)
            // On iPad the share sheet requires a popover source anchor.
            if let popover = sheet.popoverPresentationController {
                popover.sourceView = vc.view
                popover.sourceRect = CGRect(
                    x: vc.view.bounds.midX, y: vc.view.bounds.midY,
                    width: 0, height: 0)
                popover.permittedArrowDirections = []
            }
            sheet.completionWithItemsHandler = { _, _, _, _ in
                mbm_ios_onCallBackCommands("share", "done")
            }
            vc.present(sheet, animated: true)
        }
    }
}

// ---------------------------------------------------------------------------
// UIDocumentPickerDelegate
// ---------------------------------------------------------------------------

extension MbmCommandBridge: UIDocumentPickerDelegate {

    public func documentPicker(
        _ controller: UIDocumentPickerViewController,
        didPickDocumentsAt urls: [URL]
    ) {
        guard let url = urls.first else { return }
        // Request access to the security-scoped URL returned by the picker.
        let accessing = url.startAccessingSecurityScopedResource()
        defer { if accessing { url.stopAccessingSecurityScopedResource() } }
        mbm_ios_onCallBackCommands("pickFile", url.path)
    }

    public func documentPickerWasCancelled(
        _ controller: UIDocumentPickerViewController
    ) {
        // Empty string signals cancellation to Lua.
        mbm_ios_onCallBackCommands("pickFile", "")
    }
}
