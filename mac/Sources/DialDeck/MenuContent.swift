import DialDeckKit
import ServiceManagement
import SwiftUI

/// The menu bar menu: what reaches the device, the settings window, and launch at login.
struct MenuContent: View {
    @ObservedObject var link: DeviceLink
    @ObservedObject var herdr: HerdrBridge
    @ObservedObject var usage: UsageMonitor
    @Environment(\.openSettings) private var openSettings
    @State private var launchAtLogin = SMAppService.mainApp.status == .enabled

    var body: some View {
        Text(deviceText)
        Text(herdrText)
        Text(usageText)
        Divider()
        Button("Settings…") {
            // A menu bar app is not active on its own; without this the window opens behind others.
            NSApp.activate(ignoringOtherApps: true)
            openSettings()
        }
        .keyboardShortcut(",")
        Toggle("Launch at Login", isOn: $launchAtLogin)
            .onChange(of: launchAtLogin) { _, enabled in setLaunchAtLogin(enabled) }
        Divider()
        Button("Quit DialDeck") { NSApp.terminate(nil) }
            .keyboardShortcut("q")
    }

    private var deviceText: String {
        switch link.state {
        case .ready: "Device: connected"
        case .connecting: "Device: connecting"
        case .searching: "Device: not connected"
        case .bluetoothOff: "Device: Bluetooth off"
        case .failed(let message): "Device: \(message)"
        }
    }

    private var herdrText: String {
        switch herdr.state {
        case .starting: "herdr: starting"
        case .running(let agents): "herdr: \(agents) agents"
        case .unavailable(let message): "herdr: \(message)"
        }
    }

    private var usageText: String {
        switch usage.state {
        case .off: "Usage: off"
        case .running(let windows), .stale(let windows, _):
            "\(usage.provider.name): " + windows.map { "\($0.label) \($0.percent.map { "\($0)%" } ?? "--")" }
                .joined(separator: " · ")
        case .unavailable(let message): "\(usage.provider.name): \(message)"
        }
    }

    private func setLaunchAtLogin(_ enabled: Bool) {
        do {
            if enabled {
                try SMAppService.mainApp.register()
            } else {
                try SMAppService.mainApp.unregister()
            }
        } catch {
            NSLog("DialDeck: launch at login: \(error.localizedDescription)")
        }
        launchAtLogin = SMAppService.mainApp.status == .enabled
    }
}
