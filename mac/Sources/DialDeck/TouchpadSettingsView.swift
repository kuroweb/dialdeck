import DialDeckKit
import SwiftUI

/// Touchpad, knob and display settings. Each change is written to the device as soon as a slider
/// is released, so its effect can be tried right away on the device.
struct TouchpadSettingsView: View {
    @ObservedObject var link: DeviceLink
    @State private var draft: DeviceSettings?

    var body: some View {
        if let binding = Binding($draft) {
            Form {
                Section("Pointer") {
                    ScaledSlider(title: "Speed", value: binding.pointerGain,
                                 range: DeviceSettings.pointerGainRange, step: 25, commit: commit)
                    ScaledSlider(title: "Acceleration", value: binding.pointerAccel,
                                 range: DeviceSettings.pointerAccelRange, step: 5, commit: commit)
                    ScaledSlider(title: "Max speed", value: binding.pointerGainMax,
                                 range: DeviceSettings.pointerGainMaxRange, step: 50, commit: commit)
                }
                Section("Knob scroll") {
                    Stepper(value: binding.scrollStep, in: DeviceSettings.scrollStepRange) {
                        LabeledContent("Lines per click", value: "\(binding.wrappedValue.scrollStep)")
                    }
                    .onChange(of: binding.wrappedValue.scrollStep) { _, _ in commit() }
                    Toggle("Reverse direction", isOn: binding.scrollReverse)
                        .onChange(of: binding.wrappedValue.scrollReverse) { _, _ in commit() }
                }
                Section("Display") {
                    PercentSlider(title: "Brightness", value: binding.brightness,
                                  range: DeviceSettings.brightnessRange, commit: commit)
                }
            }
            .formStyle(.grouped)
            .onChange(of: link.settings) { _, newValue in draft = newValue }
        } else {
            ProgressView()
                .frame(maxWidth: .infinity, minHeight: 320)
                .onAppear { draft = link.settings }
                .onChange(of: link.settings) { _, newValue in draft = newValue }
        }
    }

    private func commit() {
        if let draft, draft != link.settings {
            link.write(draft)
        }
    }
}

/// Slider for a value stored x100, shown as a multiplier such as "2.50×".
private struct ScaledSlider: View {
    let title: String
    @Binding var value: Int32
    let range: ClosedRange<Int32>
    let step: Int32
    let commit: () -> Void

    var body: some View {
        LabeledContent {
            HStack {
                Slider(value: Binding(get: { Double(value) }, set: { value = Int32($0) }),
                       in: Double(range.lowerBound)...Double(range.upperBound),
                       step: Double(step)) { editing in
                    if !editing { commit() }
                }
                Text(String(format: "%.2f×", Double(value) / 100))
                    .monospacedDigit()
                    .frame(width: 52, alignment: .trailing)
            }
        } label: {
            Text(title)
        }
    }
}

private struct PercentSlider: View {
    let title: String
    @Binding var value: Int32
    let range: ClosedRange<Int32>
    let commit: () -> Void

    var body: some View {
        LabeledContent {
            HStack {
                Slider(value: Binding(get: { Double(value) }, set: { value = Int32($0) }),
                       in: Double(range.lowerBound)...Double(range.upperBound),
                       step: 1) { editing in
                    if !editing { commit() }
                }
                Text("\(value)%")
                    .monospacedDigit()
                    .frame(width: 52, alignment: .trailing)
            }
        } label: {
            Text(title)
        }
    }
}
