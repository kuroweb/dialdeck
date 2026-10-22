import DialDeckKit
import SwiftUI

/// Pick the modifiers and the key. Shortcuts are picked rather than recorded because macOS handles
/// system shortcuts such as ⌘Tab, ⌃↑ or ⌘⇧4 before any app sees the key press.
struct ShortcutEditor: View {
    @Binding var modifiers: UInt8
    @Binding var key: UInt8

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 12) {
                ModifierToggle(symbol: "⌃", bit: HIDModifier.control, modifiers: $modifiers)
                ModifierToggle(symbol: "⌥", bit: HIDModifier.option, modifiers: $modifiers)
                ModifierToggle(symbol: "⇧", bit: HIDModifier.shift, modifiers: $modifiers)
                ModifierToggle(symbol: "⌘", bit: HIDModifier.command, modifiers: $modifiers)
            }
            Picker("Key", selection: $key) {
                Text("None").tag(UInt8(0))
                ForEach(KeyCodes.allKeys, id: \.usage) { Text($0.name).tag($0.usage) }
            }
            .frame(width: 160)
            if key != 0 {
                Text(KeyCodes.display(modifiers: modifiers, key: key))
                    .font(.title3.monospaced())
            }
        }
    }
}

private struct ModifierToggle: View {
    let symbol: String
    let bit: UInt8
    @Binding var modifiers: UInt8

    var body: some View {
        Toggle(symbol, isOn: Binding(
            get: { modifiers & bit != 0 },
            set: { modifiers = $0 ? modifiers | bit : modifiers & ~bit }
        ))
        .toggleStyle(.checkbox)
    }
}
