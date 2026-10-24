import DialDeckKit
import SwiftUI

/// Lives in the menu bar so herdr and usage keep reaching the device with no window open. The
/// editor is the settings window, opened from the menu.
@main
struct DialDeckApp: App {
    @StateObject private var link: DeviceLink
    @StateObject private var herdr: HerdrBridge
    @StateObject private var usage: UsageMonitor

    init() {
        let link = DeviceLink()
        let herdr = HerdrBridge(link: link)
        herdr.start()
        let usage = UsageMonitor(link: link)
        usage.start()
        _link = StateObject(wrappedValue: link)
        _herdr = StateObject(wrappedValue: herdr)
        _usage = StateObject(wrappedValue: usage)
    }

    var body: some Scene {
        MenuBarExtra("DialDeck", systemImage: "dial.medium") {
            MenuContent(link: link, herdr: herdr, usage: usage)
        }
        Settings {
            RootView(link: link, herdr: herdr, usage: usage)
        }
    }
}
