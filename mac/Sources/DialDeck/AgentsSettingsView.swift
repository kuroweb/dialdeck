import DialDeckKit
import SwiftUI

/// Settings of the device's Agents screen: whether herdr's agents reach it, and which agent's
/// plan usage its middle shows.
struct AgentsSettingsView: View {
    @ObservedObject var link: DeviceLink
    @ObservedObject var herdr: HerdrBridge
    @ObservedObject var usage: UsageMonitor

    var body: some View {
        Form {
            Section("herdr") {
                LabeledContent("Agents") {
                    switch herdr.state {
                    case .starting:
                        Text("Looking for herdr…")
                    case .running(let agents):
                        Text("\(agents) sent to the device")
                    case .unavailable(let message):
                        Label(message, systemImage: "exclamationmark.triangle")
                            .foregroundStyle(.orange)
                    }
                }
            }

            Section {
                ForEach(UsageProvider.configurable) { agent in
                    Toggle(agent.name, isOn: enabledBinding(agent))
                }
            } header: {
                Text("Usage sources")
            } footer: {
                Text("Turn off agents that are not installed on this Mac. The device Settings → Usage screen uses the same list when connected.")
                    .foregroundStyle(.secondary)
            }

            Section {
                Picker("Show usage of", selection: $usage.provider) {
                    ForEach(usage.selectableProviders) { Text($0.name).tag($0) }
                }
                switch usage.state {
                case .off:
                    if usage.provider == .off {
                        EmptyView()
                    } else {
                        Label("Reading usage…", systemImage: "ellipsis")
                            .foregroundStyle(.secondary)
                    }
                case .running(let windows):
                    rows(windows)
                case .stale(let windows, let message):
                    rows(windows)
                    Label("Showing the last values: \(message)", systemImage: "clock.arrow.circlepath")
                        .foregroundStyle(.secondary)
                case .unavailable(let message):
                    Label(message, systemImage: "exclamationmark.triangle")
                        .foregroundStyle(.orange)
                }
                if link.state == .ready, !link.usageSupported {
                    Label("The device did not offer usage. Remove DialDeck in System Settings → Bluetooth and pair it again.",
                          systemImage: "exclamationmark.triangle")
                        .foregroundStyle(.orange)
                }
            } header: {
                Text("Usage")
            } footer: {
                Text(usageFooter)
                    .foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
    }

    private func enabledBinding(_ agent: UsageProvider) -> Binding<Bool> {
        Binding(
            get: { usage.isProviderEnabled(agent) },
            set: { usage.setProviderEnabled(agent, enabled: $0) }
        )
    }

    private var usageFooter: String {
        let onDevice = "Shown in the middle of the Agents screen. Tap the center on the device to switch provider."
        switch usage.provider {
        case .off: return onDevice
        case .claude: return "\(onDevice) Claude is read every five minutes."
        case .codex: return "\(onDevice) Codex is read every minute from local session logs."
        case .cursor: return "\(onDevice) Cursor is read every five minutes."
        }
    }

    private func rows(_ windows: [UsageWindow]) -> some View {
        ForEach(windows, id: \.label) { window in
            LabeledContent(window.label) {
                Text(text(for: window)).monospacedDigit()
            }
        }
    }

    private func text(for window: UsageWindow) -> String {
        let percent = window.percent.map { "\($0)%" } ?? "--"
        guard let resetsAt = window.resetsAt else { return percent }
        return "\(percent) · resets in \(duration(until: resetsAt))"
    }

    /// "45m", "3h 20m", "57h" up to a week, then days: the same form as the device.
    private func duration(until date: Date) -> String {
        let minutes = max(0, Int(date.timeIntervalSinceNow / 60))
        if minutes < 60 { return "\(minutes)m" }
        if minutes < 10 * 60 { return "\(minutes / 60)h \(minutes % 60)m" }
        if minutes <= 7 * 24 * 60 { return "\(minutes / 60)h" }
        return "\(minutes / 1440)d"
    }
}
