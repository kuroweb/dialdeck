import DialDeckKit
import SwiftUI

/// Lives in the menu bar so herdr and usage keep reaching the device with no window open. The
/// editor is the settings window, opened from the menu.
@main
struct DialDeckApp: App {
    @StateObject private var link: DeviceLink
    @StateObject private var herdr: HerdrBridge
    @StateObject private var orca: OrcaBridge
    @StateObject private var agentSource: AgentSourceController
    @StateObject private var usage: UsageMonitor

    init() {
        let link = DeviceLink()
        let herdr = HerdrBridge(link: link)
        let orca = OrcaBridge(link: link)
        let agentSource = AgentSourceController(link: link, herdr: herdr, orca: orca)
        agentSource.start()
        let usage = UsageMonitor(link: link)
        usage.start()
        _link = StateObject(wrappedValue: link)
        _herdr = StateObject(wrappedValue: herdr)
        _orca = StateObject(wrappedValue: orca)
        _agentSource = StateObject(wrappedValue: agentSource)
        _usage = StateObject(wrappedValue: usage)
    }

    var body: some Scene {
        MenuBarExtra("DialDeck", systemImage: "dial.medium") {
            MenuContent(link: link, herdr: herdr, orca: orca, agentSource: agentSource, usage: usage)
        }
        Settings {
            RootView(link: link, herdr: herdr, orca: orca, agentSource: agentSource, usage: usage)
        }
    }
}
