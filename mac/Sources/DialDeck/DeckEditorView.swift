import DialDeckKit
import SwiftUI

/// Where each slot sits on the round screen: slot 0 is the center, 1-8 run clockwise from the top.
private let slotNames = [
    "Center", "Top right", "Right, upper", "Right, lower", "Bottom right",
    "Bottom left", "Left, lower", "Left, upper", "Top left",
]

struct DeckEditorView: View {
    @ObservedObject var link: DeviceLink
    @State private var selection = 0

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack {
                Spacer(minLength: 0)
                DeckPreview(slots: link.slots, selection: $selection)
                Spacer(minLength: 0)
            }
            SlotEditor(name: slotName(selection), stored: link.slots[selection]) {
                link.write($0, at: selection)
            }
            .id(selection)
        }
    }

    private func slotName(_ index: Int) -> String {
        index < slotNames.count ? slotNames[index] : "Slot \(index)"
    }
}

/// The deck as it looks on the device. Click a button to edit it.
private struct DeckPreview: View {
    let slots: [DeckSlot]
    @Binding var selection: Int

    private let size: CGFloat = 240
    private let ringRadius: CGFloat = 75
    private let ringButton: CGFloat = 44
    private let centerButton: CGFloat = 63

    var body: some View {
        ZStack {
            Circle().fill(Color.black)
            ForEach(Array(slots.enumerated()), id: \.offset) { index, slot in
                button(index: index, slot: slot)
                    .offset(offset(for: index))
            }
        }
        .frame(width: size, height: size)
    }

    private func offset(for index: Int) -> CGSize {
        guard index > 0 else { return .zero }
        let ring = slots.count - 1
        // Same half-step offset as the firmware, so no button sits at the very top.
        let angle = 2 * Double.pi / Double(ring) * (Double(index - 1) + 0.5)
        return CGSize(width: ringRadius * sin(angle), height: -ringRadius * cos(angle))
    }

    private func button(index: Int, slot: DeckSlot) -> some View {
        let diameter = index == 0 ? centerButton : ringButton
        return Button { selection = index } label: {
            Text(displayLabel(slot.label))
                .font(.system(size: 10, weight: .medium))
                .foregroundStyle(.white)
                .lineLimit(1)
                .minimumScaleFactor(0.6)
                .padding(4)
                .frame(width: diameter, height: diameter)
                .background(Circle().fill(index == 0 ? Color.teal.opacity(0.7) : Color.gray.opacity(0.45)))
                .overlay(Circle().stroke(Color.accentColor, lineWidth: selection == index ? 3 : 0))
        }
        .buttonStyle(.plain)
    }
}

/// The device draws a few labels with its own symbol glyphs, which macOS cannot render.
func displayLabel(_ label: String) -> String {
    let symbols: [UInt32: String] = [0xF04B: "▶", 0xF04C: "⏸", 0xF028: "🔊", 0xF027: "🔉", 0xF026: "🔈"]
    return label.unicodeScalars.map { symbols[$0.value] ?? String($0) }.joined()
}

private enum ActionKind: String, CaseIterable, Identifiable {
    case shortcut = "Shortcut"
    case media = "Media key"
    case none = "None"
    var id: String { rawValue }
}

/// Editor for one slot. Changes stay local until Apply writes them to the device.
private struct SlotEditor: View {
    let name: String
    let stored: DeckSlot
    let apply: (DeckSlot) -> Void

    @State private var draft = DeckSlot()
    @State private var kind = ActionKind.none

    var body: some View {
        Form {
            Section(name) {
                TextField("Label", text: $draft.label)
                if !labelFits {
                    Text("Up to \(DeckSlot.maxLabelBytes) letters, digits or symbols (A-Z only).")
                        .font(.caption)
                        .foregroundStyle(.red)
                }

                Picker("Action", selection: $kind) {
                    ForEach(ActionKind.allCases) { Text($0.rawValue).tag($0) }
                }
                .onChange(of: kind) { _, newKind in setKind(newKind) }

                switch kind {
                case .shortcut:
                    LabeledContent("Shortcut") {
                        ShortcutEditor(modifiers: $draft.modifiers, key: $draft.key)
                    }
                case .media:
                    Picker("Media key", selection: $draft.consumer) {
                        ForEach(MediaKey.allCases) { Text($0.name).tag($0.rawValue) }
                    }
                case .none:
                    EmptyView()
                }
            }

            HStack {
                Spacer()
                Button("Revert") { load() }
                    .disabled(draft == stored)
                Button("Apply") { apply(draft) }
                    .keyboardShortcut(.defaultAction)
                    .disabled(draft == stored || !labelFits)
            }
        }
        .formStyle(.grouped)
        .onAppear(perform: load)
        .onChange(of: stored) { _, _ in load() }
    }

    /// The device font has ASCII only; the built-in labels also use its private-use symbol glyphs.
    private var labelFits: Bool {
        draft.label.utf8.count <= DeckSlot.maxLabelBytes
            && draft.label.unicodeScalars.allSatisfy { $0.isASCII || (0xF000...0xF8FF).contains($0.value) }
    }

    private func load() {
        draft = stored
        kind = stored.key != 0 ? .shortcut : (stored.consumer != 0 ? .media : .none)
    }

    private func setKind(_ newKind: ActionKind) {
        switch newKind {
        case .shortcut:
            draft.consumer = 0
        case .media:
            draft.modifiers = 0
            draft.key = 0
            if MediaKey(rawValue: draft.consumer) == nil { draft.consumer = MediaKey.playPause.rawValue }
        case .none:
            draft.modifiers = 0
            draft.key = 0
            draft.consumer = 0
        }
    }
}
