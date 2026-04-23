/*
 * ╔════════════════════════════════════════════════════════════════════════╗
 * ║ MIT License — mini-mbm                                                ║
 * ║ Copyright (c) 2025 mini-mbm contributors                              ║
 * ║ Permission is hereby granted, free of charge, to any person obtaining ║
 * ║ a copy of this software and associated documentation files (the       ║
 * ║ "Software"), to deal in the Software without restriction.             ║
 * ╚════════════════════════════════════════════════════════════════════════╝
 *
 * flatten_icon.swift — composite a PNG onto a solid white background.
 *
 * Usage (invoked by CMake at configure time via execute_process):
 *   swift platform-ios/flatten_icon.swift <src.png> <dst.png>
 *
 * Reason: iOS home-screen icons must be fully opaque.  A source PNG that has
 * an alpha channel (even an all-opaque one) triggers iOS to render the icon
 * as invisible/black on the home screen.  This script flattens any alpha onto
 * white and writes an RGB (no alpha) PNG to <dst.png>.
 *
 * Uses only CoreGraphics + ImageIO — no AppKit, no UIKit, no Pillow.
 * Swift 5.x / 6.x compatible (no concurrency features used).
 */

import CoreGraphics
import ImageIO
import Foundation

guard CommandLine.arguments.count >= 3 else {
    fputs("Usage: swift flatten_icon.swift <src.png> <dst.png>\n", stderr)
    exit(1)
}

let srcURL = URL(fileURLWithPath: CommandLine.arguments[1]) as CFURL
let dstURL = URL(fileURLWithPath: CommandLine.arguments[2]) as CFURL

guard let src = CGImageSourceCreateWithURL(srcURL, nil),
      let img = CGImageSourceCreateImageAtIndex(src, 0, nil) else {
    fputs("flatten_icon: cannot open \(CommandLine.arguments[1])\n", stderr)
    exit(1)
}

let w = img.width
let h = img.height
let cs = CGColorSpaceCreateDeviceRGB()

// noneSkipLast → RGBX — three colour bytes + one ignored byte, fully opaque.
guard let ctx = CGContext(data: nil, width: w, height: h, bitsPerComponent: 8,
                          bytesPerRow: 0, space: cs,
                          bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue) else {
    fputs("flatten_icon: cannot create CGContext\n", stderr)
    exit(1)
}

// Flood-fill with white, then draw the source image on top.
ctx.setFillColor(CGColor(colorSpace: cs, components: [1.0, 1.0, 1.0, 1.0])!)
ctx.fill(CGRect(x: 0, y: 0, width: Double(w), height: Double(h)))
ctx.draw(img, in: CGRect(x: 0, y: 0, width: Double(w), height: Double(h)))

guard let outImg = ctx.makeImage() else {
    fputs("flatten_icon: cannot create output image\n", stderr)
    exit(1)
}

guard let dest = CGImageDestinationCreateWithURL(dstURL, "public.png" as CFString, 1, nil) else {
    fputs("flatten_icon: cannot create image destination\n", stderr)
    exit(1)
}
CGImageDestinationAddImage(dest, outImg, nil)
guard CGImageDestinationFinalize(dest) else {
    fputs("flatten_icon: finalize failed\n", stderr)
    exit(1)
}
