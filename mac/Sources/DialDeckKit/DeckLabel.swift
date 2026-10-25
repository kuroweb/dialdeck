import Foundation

/// LVGL symbol glyphs (U+F000…) in deck labels are drawn on the device font; map them for Mac UI.
public enum DeckLabel {
    /// Wire code point → string shown in the editor and previews.
    private static let wireToDisplay: [UInt32: String] = [
        0xF001: "♫",
        0xF04B: "▶",
        0xF04C: "⏸",
        0xF04D: "■",
        0xF026: "🔈",
        0xF027: "🔉",
        0xF028: "🔊",
    ]

    /// Shorter labels for small deck preview buttons.
    private static let wireToCompact: [UInt32: String] = [
        0xF04B: "▶",
        0xF04C: "⏸",
        0xF026: "V",
        0xF027: "V−",
        0xF028: "V+",
    ]

    private static let displayScalarToWire: [UInt32: UInt32] = {
        var map: [UInt32: UInt32] = [:]
        for (wire, text) in wireToDisplay {
            for scalar in text.unicodeScalars {
                map[scalar.value] = wire
            }
        }
        return map
    }()

    public static func display(_ label: String, compact: Bool = false) -> String {
        label.unicodeScalars.map { scalar in
            if compact, let text = wireToCompact[scalar.value] { return text }
            if let text = wireToDisplay[scalar.value] { return text }
            if (0xF000...0xF8FF).contains(scalar.value) { return "?" }
            return String(scalar)
        }.joined()
    }

    public static func wire(from display: String) -> String {
        display.unicodeScalars.map { scalar in
            if let wire = displayScalarToWire[scalar.value] {
                return String(UnicodeScalar(wire)!)
            }
            return String(scalar)
        }.joined()
    }
}
