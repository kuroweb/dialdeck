import DialDeckKit
import SwiftUI

/// The settings window: one tab per part of the device, each sized to its content, with the
/// connection shown along the bottom.
struct RootView: View {
    @ObservedObject var link: DeviceLink
    @ObservedObject var herdr: HerdrBridge
    @ObservedObject var usage: UsageMonitor

    var body: some View {
        TabView {
            page(width: 480, height: 330) {
                AgentsSettingsView(link: link, herdr: herdr, usage: usage)
            }
            .tabItem { Label("Agents", systemImage: "terminal") }

            page(width: 480, height: 430) {
                deviceContent { TouchpadSettingsView(link: link) }
            }
            .tabItem { Label("Touchpad", systemImage: "hand.point.up.left") }

            page(width: 700, height: 380) {
                deviceContent { DeckEditorView(link: link).padding(20) }
            }
            .tabItem { Label("Deck", systemImage: "circle.grid.cross") }
        }
    }

    private func page<Content: View>(width: CGFloat, height: CGFloat,
                                     @ViewBuilder content: () -> Content) -> some View {
        VStack(spacing: 0) {
            content()
                .frame(width: width, height: height)
            Divider()
            ConnectionBar(link: link)
        }
        .frame(width: width)
    }

    /// Tabs that read and write the device need it connected.
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
