import DialDeckKit
import SwiftUI

@main
struct DialDeckApp: App {
    @StateObject private var link = DeviceLink()

    var body: some Scene {
        Window("DialDeck", id: "main") {
            DeckEditorView(link: link)
        }
        .windowResizability(.contentSize)
    }
}
