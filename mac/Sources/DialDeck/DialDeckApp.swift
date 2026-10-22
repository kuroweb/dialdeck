import DialDeckKit
import SwiftUI

@main
struct DialDeckApp: App {
    @StateObject private var link = DeviceLink()

    var body: some Scene {
        Window("DialDeck", id: "main") {
            RootView(link: link)
        }
        .windowResizability(.contentSize)
    }
}
