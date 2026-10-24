import DialDeckKit
import SwiftUI

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
        Window("DialDeck", id: "main") {
            RootView(link: link, herdr: herdr, usage: usage)
        }
        .windowResizability(.contentSize)
    }
}
