// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "DialDeck",
    platforms: [.macOS(.v14)],
    targets: [
        // BLE link to the device and the deck wire format, shared with future tools (herdr bridge).
        .target(name: "DialDeckKit"),
        // Menu bar app. Bundle it with scripts/build-app.sh: CoreBluetooth needs an Info.plist.
        .executableTarget(name: "DialDeck", dependencies: ["DialDeckKit"]),
    ],
    swiftLanguageModes: [.v5]
)
