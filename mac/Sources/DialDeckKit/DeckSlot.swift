import Foundation

/// One deck button assignment. Mirrors `deck_slot_t` in firmware/main/deck_config.h.
public struct DeckSlot: Equatable, Sendable {
    public static let wireSize = 20
    public static let labelLength = 16   // bytes including the terminating NUL

    public var modifiers: UInt8   // HIDModifier bits
    public var key: UInt8         // HID keyboard usage, 0 for none
    public var consumer: UInt16   // consumer control usage, used when key is 0
    public var label: String

    public init(modifiers: UInt8 = 0, key: UInt8 = 0, consumer: UInt16 = 0, label: String = "") {
        self.modifiers = modifiers
        self.key = key
        self.consumer = consumer
        self.label = label
    }

    public init?(wire: Data) {
        guard wire.count == Self.wireSize else { return nil }
        let bytes = [UInt8](wire)
        modifiers = bytes[0]
        key = bytes[1]
        consumer = UInt16(bytes[2]) | UInt16(bytes[3]) << 8
        let labelBytes = bytes[4...].prefix { $0 != 0 }
        label = String(decoding: labelBytes, as: UTF8.self)
    }

    /// Longest label the device accepts, in UTF-8 bytes.
    public static var maxLabelBytes: Int { labelLength - 1 }

    public var wire: Data {
        var bytes: [UInt8] = [modifiers, key, UInt8(consumer & 0xFF), UInt8(consumer >> 8)]
        var labelBytes = Array(label.utf8.prefix(Self.maxLabelBytes))
        labelBytes += Array(repeating: 0, count: Self.labelLength - labelBytes.count)
        bytes += labelBytes
        return Data(bytes)
    }
}

/// Keyboard modifier bits of the HID report.
public enum HIDModifier {
    public static let control: UInt8 = 0x01
    public static let shift: UInt8 = 0x02
    public static let option: UInt8 = 0x04
    public static let command: UInt8 = 0x08
}

/// Media keys the deck can send.
public enum MediaKey: UInt16, CaseIterable, Identifiable, Sendable {
    case playPause = 0x00CD
    case next = 0x00B5
    case previous = 0x00B6
    case volumeUp = 0x00E9
    case volumeDown = 0x00EA
    case mute = 0x00E2

    public var id: UInt16 { rawValue }

    public var name: String {
        switch self {
        case .playPause: "Play / Pause"
        case .next: "Next track"
        case .previous: "Previous track"
        case .volumeUp: "Volume up"
        case .volumeDown: "Volume down"
        case .mute: "Mute"
        }
    }
}
