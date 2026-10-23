import Foundation

/// Touchpad, knob and display settings. Mirrors `settings_t` in firmware/main/settings.h:
/// six little-endian int32 values. Fractional values are x100 on the wire.
public struct DeviceSettings: Equatable, Sendable {
    public static let wireSize = 24

    public var pointerGain: Int32      // x100
    public var pointerAccel: Int32     // x100
    public var pointerGainMax: Int32   // x100
    public var scrollStep: Int32
    public var scrollReverse: Bool
    public var brightness: Int32       // percent

    /// Ranges the device accepts, matching firmware/main/settings.c.
    public static let pointerGainRange: ClosedRange<Int32> = 100...500
    public static let pointerAccelRange: ClosedRange<Int32> = 0...50
    public static let pointerGainMaxRange: ClosedRange<Int32> = 100...1000
    public static let scrollStepRange: ClosedRange<Int32> = 1...5
    public static let brightnessRange: ClosedRange<Int32> = 1...100

    public init?(wire: Data) {
        guard wire.count == Self.wireSize else { return nil }
        let values = stride(from: 0, to: Self.wireSize, by: 4).map { offset in
            wire.subdata(in: offset..<offset + 4).withUnsafeBytes { Int32(littleEndian: $0.loadUnaligned(as: Int32.self)) }
        }
        pointerGain = values[0]
        pointerAccel = values[1]
        pointerGainMax = values[2]
        scrollStep = values[3]
        scrollReverse = values[4] != 0
        brightness = values[5]
    }

    public var wire: Data {
        let values = [pointerGain, pointerAccel, pointerGainMax, scrollStep, scrollReverse ? 1 : 0, brightness]
        var data = Data()
        for value in values {
            withUnsafeBytes(of: value.littleEndian) { data.append(contentsOf: $0) }
        }
        return data
    }
}
