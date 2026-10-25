import DialDeckKit
import SwiftUI

private enum SettingsPage: String, CaseIterable, Identifiable {
    case agents = "Agents"
    case touchpad = "Touchpad"
    case deck = "Deck"

    var id: Self { self }

    var symbol: String {
        switch self {
        case .agents: "terminal"
        case .touchpad: "hand.point.up.left"
        case .deck: "circle.grid.cross"
        }
    }
}

/// The settings window: select a device page in the sidebar and edit it on the right.
struct RootView: View {
    @ObservedObject var link: DeviceLink
    @ObservedObject var herdr: HerdrBridge
    @ObservedObject var usage: UsageMonitor
    @State private var selection: SettingsPage = .agents
    @FocusState private var focusedPage: SettingsPage?

    var body: some View {
        HStack(spacing: 0) {
            sidebar
            Divider()
            VStack(spacing: 0) {
                Group {
                    switch selection {
                    case .agents:
                        AgentsSettingsView(link: link, herdr: herdr, usage: usage)
                    case .touchpad:
                        deviceContent { TouchpadSettingsView(link: link) }
                    case .deck:
                        deviceContent { DeckEditorView(link: link).padding(20) }
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)

                Divider()
                ConnectionBar(link: link)
            }
        }
        .frame(width: 880, height: 540)
    }

    private var sidebar: some View {
        VStack(alignment: .leading, spacing: 0) {
            Label("DialDeck", systemImage: "dial.medium")
                .font(.headline)
                .padding(.horizontal, 18)
                .frame(height: 58)

            Text("DEVICE")
                .font(.caption2.weight(.semibold))
                .foregroundStyle(.secondary)
                .padding(.leading, 18)
                .padding(.bottom, 8)

            VStack(spacing: 3) {
                ForEach(SettingsPage.allCases) { page in
                    Button {
                        selection = page
                        focusedPage = page
                    } label: {
                        Label(page.rawValue, systemImage: page.symbol)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding(.horizontal, 12)
                            .frame(height: 34)
                            .background(selection == page ? Color.primary.opacity(0.12) : .clear,
                                        in: RoundedRectangle(cornerRadius: 8))
                            .contentShape(Rectangle())
                    }
                    .buttonStyle(SidebarButtonStyle())
                    .focused($focusedPage, equals: page)
                    .focusEffectDisabled()
                    // Button actions run on mouse-up; switch pages as soon as the press starts.
                    .simultaneousGesture(DragGesture(minimumDistance: 0).onChanged { _ in
                        if selection != page { selection = page }
                    })
                }
            }
            .padding(.horizontal, 10)

            Spacer()
        }
        .frame(width: 190)
        .background(Color(nsColor: .windowBackgroundColor))
    }

    /// Pages that read and write the device need it connected.
    @ViewBuilder
    private func deviceContent<Content: View>(@ViewBuilder content: () -> Content) -> some View {
        if link.state == .ready, !link.slots.isEmpty {
            content()
        } else {
            Text(hint(for: link.state))
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .padding()
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }

    private func hint(for state: DeviceLink.State) -> String {
        switch state {
        case .bluetoothOff: "Bluetooth is off or not allowed for DialDeck."
        case .searching: "DialDeck is not connected.\nPair it in System Settings → Bluetooth."
        case .connecting: "Connecting…"
        case .ready: "Reading the device…"
        case .failed(let message): message
        }
    }
}

private struct SidebarButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
    }
}

/// The device connection, and Reload to read the assignments and settings again.
private struct ConnectionBar: View {
    @ObservedObject var link: DeviceLink

    var body: some View {
        let (text, color): (String, Color) = switch link.state {
        case .ready: ("Connected", .green)
        case .connecting: ("Connecting", .orange)
        case .searching: ("Not connected", .secondary)
        case .bluetoothOff: ("Bluetooth off", .red)
        case .failed: ("Error", .red)
        }
        HStack {
            Image(systemName: "circle.fill")
                .font(.system(size: 8))
                .foregroundStyle(color)
            Text(text)
                .foregroundStyle(.secondary)
            Spacer()
            Button("Reload") { link.reload() }
                .controlSize(.small)
                .disabled(link.state != .ready)
        }
        .font(.callout)
        .padding(.horizontal, 20)
        .padding(.vertical, 10)
    }
}
