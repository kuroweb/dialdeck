// Draw the app icon: the device's deck screen (eight buttons around a center button) on a
// macOS-style rounded square. Usage: swift make-icon.swift <output.png>
import AppKit

let size: CGFloat = 1024
let output = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "icon.png"

let image = NSImage(size: NSSize(width: size, height: size))
image.lockFocus()
let ctx = NSGraphicsContext.current!.cgContext

// Rounded square on Apple's icon grid: 824 pt body inside the 1024 pt canvas.
let body = CGRect(x: 100, y: 100, width: 824, height: 824)
let squircle = CGPath(roundedRect: body, cornerWidth: 185, cornerHeight: 185, transform: nil)
ctx.addPath(squircle)
ctx.clip()
let background = CGGradient(
    colorsSpace: CGColorSpaceCreateDeviceRGB(),
    colors: [NSColor(calibratedRed: 0.20, green: 0.24, blue: 0.29, alpha: 1).cgColor,
             NSColor(calibratedRed: 0.08, green: 0.09, blue: 0.11, alpha: 1).cgColor] as CFArray,
    locations: [0, 1])!
ctx.drawLinearGradient(background, start: CGPoint(x: 0, y: size), end: CGPoint(x: 0, y: 0), options: [])
ctx.resetClip()

let center = CGPoint(x: size / 2, y: size / 2)

func circle(_ c: CGPoint, _ diameter: CGFloat, _ color: NSColor) {
    color.setFill()
    NSBezierPath(ovalIn: CGRect(x: c.x - diameter / 2, y: c.y - diameter / 2, width: diameter, height: diameter)).fill()
}

// Bezel and screen.
circle(center, 660, NSColor(calibratedWhite: 0.35, alpha: 1))
circle(center, 620, .black)

// Ring buttons, offset by half a step like the firmware so none sits at the very top.
let ringRadius: CGFloat = 215
for i in 0..<8 {
    let a = 2 * Double.pi / 8 * (Double(i) + 0.5)
    let p = CGPoint(x: center.x + ringRadius * sin(a), y: center.y + ringRadius * cos(a))
    circle(p, 118, NSColor(calibratedRed: 0.33, green: 0.41, blue: 0.47, alpha: 1))
}

// Center button with a play mark.
circle(center, 190, NSColor(calibratedRed: 0.0, green: 0.55, blue: 0.52, alpha: 1))
let play = NSBezierPath()
play.move(to: CGPoint(x: center.x - 26, y: center.y + 42))
play.line(to: CGPoint(x: center.x - 26, y: center.y - 42))
play.line(to: CGPoint(x: center.x + 44, y: center.y))
play.close()
NSColor.white.setFill()
play.fill()

image.unlockFocus()

let bitmap = NSBitmapImageRep(data: image.tiffRepresentation!)!
try! bitmap.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: output))
