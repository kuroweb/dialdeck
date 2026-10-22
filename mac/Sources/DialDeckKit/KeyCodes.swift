import Foundation

/// HID keyboard usages the deck can send, with the names macOS uses for them.
public enum KeyCodes {
    // (HID usage, display name)
    private static let table: [(UInt8, String)] = [
        (0x04, "A"), (0x05, "B"), (0x06, "C"), (0x07, "D"),
        (0x08, "E"), (0x09, "F"), (0x0A, "G"), (0x0B, "H"),
        (0x0C, "I"), (0x0D, "J"), (0x0E, "K"), (0x0F, "L"),
        (0x10, "M"), (0x11, "N"), (0x12, "O"), (0x13, "P"),
        (0x14, "Q"), (0x15, "R"), (0x16, "S"), (0x17, "T"),
        (0x18, "U"), (0x19, "V"), (0x1A, "W"), (0x1B, "X"),
        (0x1C, "Y"), (0x1D, "Z"),
        (0x1E, "1"), (0x1F, "2"), (0x20, "3"), (0x21, "4"),
        (0x22, "5"), (0x23, "6"), (0x24, "7"), (0x25, "8"),
        (0x26, "9"), (0x27, "0"),
        (0x28, "↩"), (0x29, "⎋"), (0x2A, "⌫"), (0x2B, "⇥"),
        (0x2C, "Space"), (0x2D, "-"), (0x2E, "="), (0x2F, "["),
        (0x30, "]"), (0x31, "\\"), (0x33, ";"), (0x34, "'"),
        (0x35, "`"), (0x36, ","), (0x37, "."), (0x38, "/"),
        (0x3A, "F1"), (0x3B, "F2"), (0x3C, "F3"), (0x3D, "F4"),
        (0x3E, "F5"), (0x3F, "F6"), (0x40, "F7"), (0x41, "F8"),
        (0x42, "F9"), (0x43, "F10"), (0x44, "F11"), (0x45, "F12"),
        (0x4A, "↖"), (0x4B, "⇞"), (0x4C, "⌦"), (0x4D, "↘"),
        (0x4E, "⇟"), (0x4F, "→"), (0x50, "←"), (0x51, "↓"),
        (0x52, "↑"),
    ]

    private static let nameByUsage = Dictionary(uniqueKeysWithValues: table)

    /// Every key that can be assigned, in menu order.
    public static var allKeys: [(usage: UInt8, name: String)] {
        table.map { (usage: $0.0, name: $0.1) }
    }

    /// Shortcut as macOS shows it, e.g. "⌃⇧⌘4".
    public static func display(modifiers: UInt8, key: UInt8) -> String {
        var s = ""
        if modifiers & HIDModifier.control != 0 { s += "⌃" }
        if modifiers & HIDModifier.option != 0 { s += "⌥" }
        if modifiers & HIDModifier.shift != 0 { s += "⇧" }
        if modifiers & HIDModifier.command != 0 { s += "⌘" }
        return s + (nameByUsage[key] ?? String(format: "0x%02X", key))
    }
}
