import DialDeckKit
import SwiftUI

/// Which agent's plan usage the middle of the device's agents screen shows, and what was read.
struct UsageSettingsView: View {
    @ObservedObject var link: DeviceLink
    @ObservedObject var usage: UsageMonitor

    var body: some View {
        Form {
            Section {
                Picker("Show usage of", selection: $usage.provider) {
                    ForEach(UsageProvider.allCases) { Text($0.name).tag($0) }
                }
            } footer: {
                Text("Read once a minute and shown in the middle of the Agents screen.")
                    .foregroundStyle(.secondary)
            }
            if !link.usageSupported {
                Section {
                    Label("The device did not offer usage. Remove DialDeck in System Settings → Bluetooth and pair it again.",
                          systemImage: "exclamationmark.triangle")
                        .foregroundStyle(.orange)
                }
            }
            Section("Now") {
                switch usage.state {
                case .off:
                    Text("The device shows the number of agents instead.")
                        .foregroundStyle(.secondary)
                case .running(let windows):
                    ForEach(windows, id: \.label) { window in
                        LabeledContent(window.label) {
                            Text(text(for: window))
                        }
                    }
                case .unavailable(let message):
                    Label(message, systemImage: "exclamationmark.triangle")
                        .foregroundStyle(.orange)
                }
            }
        }
        .formStyle(.grouped)
    }

    private func text(for window: UsageWindow) -> String {
        let percent = window.percent.map { "\($0)%" } ?? "--"
        guard let resetsAt = window.resetsAt else { return percent }
        return "\(percent), resets \(resetsAt.formatted(.relative(presentation: .named)))"
    }
}
