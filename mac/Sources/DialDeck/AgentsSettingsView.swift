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
                Picker("Show usage of", selection: $usage.provider) {
                    ForEach(UsageProvider.allCases) { Text($0.name).tag($0) }
                }
                switch usage.state {
                case .off:
                    EmptyView()
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
                Text("Shown in the middle of the Agents screen. Claude and Cursor are read every five minutes, Codex every minute. Tap it on the device to switch.")
                    .foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
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
