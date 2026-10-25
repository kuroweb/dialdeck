import Foundation

/// Deck button labels on the device use ASCII and LVGL symbol code points (U+F000…), same as `LV_SYMBOL_*`
/// in firmware. The Mac editor never puts those bytes in a `TextField`; it edits a structured choice instead.
public enum DeckLabel {
    /// One glyph from the device font. Code points match `lv_symbol_def.h` in the LVGL build pinned by the firmware.
    public enum Glyph: UInt32, CaseIterable, Identifiable, Sendable {
        case play = 0xF04B
        case pause = 0xF04C
        case stop = 0xF04D
        case mute = 0xF026
        case volumeMid = 0xF027
        case volumeMax = 0xF028

        public var id: UInt32 { rawValue }

        public var wire: String { String(UnicodeScalar(rawValue)!) }

        public var title: String {
            switch self {
            case .play: "Play"
            case .pause: "Pause"
            case .stop: "Stop"
            case .mute: "Mute"
            case .volumeMid: "Volume (mid)"
            case .volumeMax: "Volume (max)"
            }
        }

        public func systemImage(compact: Bool) -> String {
            switch self {
            case .play: "play.fill"
            case .pause: "pause.fill"
            case .stop: "stop.fill"
            case .mute: "speaker.slash.fill"
            case .volumeMid: compact ? "speaker.wave.1.fill" : "speaker.wave.2.fill"
            case .volumeMax: "speaker.wave.3.fill"
            }
        }

        public static func matching(_ wire: String) -> Glyph? {
            allCases.first { $0.wire == wire }
        }
    }

    /// Labels that are not a single glyph, shipped as factory defaults in `deck_config.c`.
    public enum Preset: String, CaseIterable, Identifiable, Sendable {
        case playPause

        public var id: String { rawValue }

        public var title: String {
            switch self {
            case .playPause: "Play / Pause"
            }
        }

        public var wire: String {
            switch self {
            case .playPause:
                Glyph.play.wire + " " + Glyph.pause.wire
            }
        }

        public static func matching(_ wire: String) -> Preset? {
            allCases.first { $0.wire == wire }
        }
    }

    public enum Kind: Equatable, Sendable {
        case text(String)
        case glyph(Glyph)
        case preset(Preset)

        public var wire: String {
            switch self {
            case .text(let text): text
            case .glyph(let glyph): glyph.wire
            case .preset(let preset): preset.wire
            }
        }
    }

    public enum Preview: Sendable {
        case text(String)
        case symbol(String)
        case symbols([String])
    }

    public static func decode(_ wire: String) -> Kind {
        if let preset = Preset.matching(wire) { return .preset(preset) }
        if let glyph = Glyph.matching(wire) { return .glyph(glyph) }
        return .text(wire)
    }

    public static func preview(_ wire: String, compact: Bool) -> Preview {
        if let preset = Preset.matching(wire) {
            switch preset {
            case .playPause:
                return compact ? .symbol("playpause") : .symbols(["play.fill", "pause.fill"])
            }
        }
        if let glyph = Glyph.matching(wire) {
            return .symbol(glyph.systemImage(compact: compact))
        }
        return .text(wire)
    }

    public static func isValidWire(_ wire: String) -> Bool {
        wire.utf8.count <= DeckSlot.maxLabelBytes
            && wire.unicodeScalars.allSatisfy { $0.isASCII || (0xF000...0xF8FF).contains($0.value) }
    }

    /// ASCII-only text for custom labels; strips other scalars.
    public static func sanitizedText(_ text: String) -> String {
        String(text.unicodeScalars.filter(\.isASCII))
    }
}
