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

    private static let contentWidth: CGFloat = 400
    private static let sidebarWidth: CGFloat = 160

    var body: some View {
        HStack(spacing: 0) {
            sidebar
            Divider()
            VStack(spacing: 0) {
                ScrollView {
                    Group {
                        switch selection {
                        case .agents:
                            AgentsSettingsView(link: link, herdr: herdr, usage: usage)
                        case .touchpad:
                            deviceContent { TouchpadSettingsView(link: link) }
                        case .deck:
                            deviceContent { DeckEditorView(link: link) }
                        }
                    }
                    .frame(maxWidth: Self.contentWidth)
                    .frame(maxWidth: .infinity)
                    .padding(.horizontal, 16)
                    .padding(.vertical, 12)
                }
                Divider()
                ConnectionBar(link: link)
            }
        }
        .frame(width: Self.sidebarWidth + Self.contentWidth + 32, height: 600)
    }

    private var sidebar: some View {
        VStack(alignment: .leading, spacing: 0) {
            Label("DialDeck", systemImage: "dial.medium")
                .font(.headline)
                .padding(.horizontal, 14)
                .frame(height: 52)

            Text("DEVICE")
                .font(.caption2.weight(.semibold))
                .foregroundStyle(.secondary)
                .padding(.leading, 14)
                .padding(.bottom, 6)

            VStack(spacing: 2) {
                ForEach(SettingsPage.allCases) { page in
                    Button {
                        selection = page
                        focusedPage = page
                    } label: {
                        Label(page.rawValue, systemImage: page.symbol)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding(.horizontal, 10)
                            .frame(height: 32)
                            .background(selection == page ? Color.primary.opacity(0.12) : .clear,
                                        in: RoundedRectangle(cornerRadius: 7))
                            .contentShape(Rectangle())
                    }
                    .buttonStyle(SidebarButtonStyle())
                    .focused($focusedPage, equals: page)
                    .focusEffectDisabled()
                    .simultaneousGesture(DragGesture(minimumDistance: 0).onChanged { _ in
                        if selection != page { selection = page }
                    })
                }
            }
            .padding(.horizontal, 8)

            Spacer()
        }
        .frame(width: Self.sidebarWidth)
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
                .frame(maxWidth: .infinity, minHeight: 200)
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
        .padding(.horizontal, 16)
        .padding(.vertical, 9)
    }
}
