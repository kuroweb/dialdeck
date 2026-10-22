import DialDeckKit
import SwiftUI

@main
struct DialDeckApp: App {
    @StateObject private var link: DeviceLink
    @StateObject private var herdr: HerdrBridge

    init() {
        let link = DeviceLink()
        let herdr = HerdrBridge(link: link)
        herdr.start()
        _link = StateObject(wrappedValue: link)
        _herdr = StateObject(wrappedValue: herdr)
    }

    var body: some Scene {
        Window("DialDeck", id: "main") {
            RootView(link: link, herdr: herdr)
        }
        .windowResizability(.contentSize)
    }
}
