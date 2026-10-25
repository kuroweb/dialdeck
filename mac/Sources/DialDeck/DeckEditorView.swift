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
            DeckSlotLabelPreview(wire: slot.label, compact: index != 0)
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

private enum ActionKind: String, CaseIterable, Identifiable {
    case shortcut = "Shortcut"
    case media = "Media key"
    case none = "None"
    var id: String { rawValue }
}

private enum LabelPickerChoice: Hashable {
    case custom
    case preset(DeckLabel.Preset)
    case glyph(DeckLabel.Glyph)
}

/// Editor for one slot. Changes stay local until Apply writes them to the device.
private struct SlotEditor: View {
    let name: String
    let stored: DeckSlot
    let apply: (DeckSlot) -> Void

    @State private var draft = DeckSlot()
    @State private var kind = ActionKind.none
    @State private var labelChoice = LabelPickerChoice.custom
    @State private var customText = ""

    var body: some View {
        Form {
            Section(name) {
                Picker("Label", selection: $labelChoice) {
                    Text("Custom text").tag(LabelPickerChoice.custom)
                    ForEach(DeckLabel.Preset.allCases) { preset in
                        Text(preset.title).tag(LabelPickerChoice.preset(preset))
                    }
                    ForEach(DeckLabel.Glyph.allCases) { glyph in
                        Label(glyph.title, systemImage: glyph.systemImage(compact: false))
                            .tag(LabelPickerChoice.glyph(glyph))
                    }
                }
                .onChange(of: labelChoice) { _, _ in
                    syncLabelFromChoice()
                }

                if labelChoice == .custom {
                    TextField("Text", text: $customText)
                        .onChange(of: customText) { _, newValue in
                            let text = DeckLabel.sanitizedText(newValue)
                            if text != newValue { customText = text }
                            draft.label = text
                        }
                } else {
                    LabeledContent("On device") {
                        DeckSlotLabelPreview(wire: draft.label)
                    }
                }

                if !labelFits {
                    Text("Up to \(DeckSlot.maxLabelBytes) bytes. Use custom text (A–Z, digits, symbols) or a device symbol above.")
                        .font(.caption)
                        .foregroundStyle(.red)
                } else if !labelChoiceMatchesDraft {
                    Text("This slot uses a label the editor cannot round-trip. Pick a label type and Apply to replace it.")
                        .font(.caption)
                        .foregroundStyle(.orange)
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

    private var labelFits: Bool { DeckLabel.isValidWire(draft.label) }

    private var labelChoiceMatchesDraft: Bool {
        switch labelChoice {
        case .custom:
            draft.label == customText
        case .preset(let preset):
            draft.label == preset.wire
        case .glyph(let glyph):
            draft.label == glyph.wire
        }
    }

    private func load() {
        draft = stored
        kind = stored.key != 0 ? .shortcut : (stored.consumer != 0 ? .media : .none)
        switch DeckLabel.decode(stored.label) {
        case .text(let text):
            labelChoice = .custom
            customText = text
        case .preset(let preset):
            labelChoice = .preset(preset)
            customText = ""
        case .glyph(let glyph):
            labelChoice = .glyph(glyph)
            customText = ""
        }
    }

    private func syncLabelFromChoice() {
        switch labelChoice {
        case .custom:
            draft.label = DeckLabel.sanitizedText(customText)
        case .preset(let preset):
            draft.label = preset.wire
        case .glyph(let glyph):
            draft.label = glyph.wire
        }
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
