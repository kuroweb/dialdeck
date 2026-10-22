import DialDeckKit
import SwiftUI

struct RootView: View {
    @ObservedObject var link: DeviceLink
    @ObservedObject var herdr: HerdrBridge

    private let tabHeight: CGFloat = 540

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack {
                Text("DialDeck").font(.title2.bold())
                Spacer()
                HerdrBadge(state: herdr.state)
                StatusBadge(state: link.state)
                Button("Reload") { link.reload() }
                    .disabled(link.state != .ready)
            }

            if link.state == .ready, !link.slots.isEmpty {
                TabView {
                    DeckEditorView(link: link)
                        .padding()
                        .tabItem { Text("Deck") }
                    TouchpadSettingsView(link: link)
                        .padding()
                        .tabItem { Text("Touchpad") }
                }
                // Tall enough that neither tab scrolls when the window opens.
                .frame(minHeight: tabHeight)
            } else {
                Text(hint(for: link.state))
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, minHeight: 320)
            }
        }
        .padding(24)
        .frame(width: 760)
    }

    private func hint(for state: DeviceLink.State) -> String {
        switch state {
        case .bluetoothOff: "Bluetooth is off or not allowed for DialDeck."
        case .searching: "DialDeck is not connected. Pair it in System Settings → Bluetooth."
        case .connecting: "Connecting…"
        case .ready: ""
        case .failed(let message): message
        }
    }
}

private struct StatusBadge: View {
    let state: DeviceLink.State

    var body: some View {
        let (text, color): (String, Color) = switch state {
        case .ready: ("Connected", .green)
        case .connecting: ("Connecting", .orange)
        case .searching: ("Not connected", .secondary)
        case .bluetoothOff: ("Bluetooth off", .red)
        case .failed: ("Error", .red)
        }
        Label(text, systemImage: "circle.fill")
            .font(.callout)
            .foregroundStyle(color)
    }
}

/// Whether agents reach the device: herdr reachable, and how many agents it reports.
private struct HerdrBadge: View {
    let state: HerdrBridge.State

    var body: some View {
        let (text, color, help): (String, Color, String) = switch state {
        case .starting: ("herdr", .secondary, "Looking for herdr")
        case .running(let agents): ("herdr: \(agents)", .green, "Agents are sent to the device")
        case .unavailable(let message): ("herdr", .orange, message)
        }
        Label(text, systemImage: "terminal")
            .font(.callout)
            .foregroundStyle(color)
            .help(help)
    }
}
