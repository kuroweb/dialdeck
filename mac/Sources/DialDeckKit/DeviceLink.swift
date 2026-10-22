import CoreBluetooth
import Foundation

/// Connection to the DialDeck config GATT service (see docs/config-protocol.md).
///
/// macOS itself keeps the device connected as a keyboard and mouse, so the link does not scan:
/// it picks up the already connected peripheral and opens its own channel on the same connection.
@MainActor
public final class DeviceLink: NSObject, ObservableObject {
    public enum State: Equatable {
        case bluetoothOff
        case searching   // the device is not connected to this Mac
        case connecting
        case ready
        case failed(String)
    }

    public static let serviceUUID = CBUUID(string: "98D3462C-2EB8-4022-BA87-4F253E72A24B")
    static let infoUUID = CBUUID(string: "98D3462D-2EB8-4022-BA87-4F253E72A24B")
    static let deckUUID = CBUUID(string: "98D3462E-2EB8-4022-BA87-4F253E72A24B")
    static let settingsUUID = CBUUID(string: "98D3462F-2EB8-4022-BA87-4F253E72A24B")
    static let agentsUUID = CBUUID(string: "98D34630-2EB8-4022-BA87-4F253E72A24B")
    static let actionUUID = CBUUID(string: "98D34631-2EB8-4022-BA87-4F253E72A24B")
    static let hidServiceUUID = CBUUID(string: "1812")
    static let supportedProtocol: UInt8 = 3

    @Published public private(set) var state: State = .searching
    @Published public private(set) var slots: [DeckSlot] = []
    @Published public private(set) var settings: DeviceSettings?

    /// Called when the device asks to act on a herdr agent (see AgentAction).
    public var onAgentAction: ((AgentAction, String) -> Void)?

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var deckCharacteristic: CBCharacteristic?
    private var settingsCharacteristic: CBCharacteristic?
    private var agentsCharacteristic: CBCharacteristic?
    private var pendingSettings: DeviceSettings?
    private var retryTimer: Timer?
    private var pendingWrites: [(Int, DeckSlot)] = []

    public override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    /// Read every assignment and the settings again.
    public func reload() {
        guard let peripheral, let deckCharacteristic, let settingsCharacteristic else { return }
        peripheral.readValue(for: deckCharacteristic)
        peripheral.readValue(for: settingsCharacteristic)
    }

    /// Replace the settings. The device applies and saves them right away.
    public func write(_ newSettings: DeviceSettings) {
        guard let peripheral, let settingsCharacteristic else { return }
        pendingSettings = newSettings
        peripheral.writeValue(newSettings.wire, for: settingsCharacteristic, type: .withResponse)
    }

    /// Replace the herdr agent list shown on the device. Values over one packet are sent as a
    /// long write, which CoreBluetooth does on its own for writes with response.
    public func writeAgents(_ data: Data) {
        guard let peripheral, let agentsCharacteristic else { return }
        peripheral.writeValue(data, for: agentsCharacteristic, type: .withResponse)
    }

    /// Write one assignment. The device saves it and redraws the button.
    public func write(_ slot: DeckSlot, at index: Int) {
        guard let peripheral, let deckCharacteristic else { return }
        var data = Data([UInt8(index)])
        data.append(slot.wire)
        pendingWrites.append((index, slot))
        peripheral.writeValue(data, for: deckCharacteristic, type: .withResponse)
    }

    private func findDevice() {
        guard central.state == .poweredOn else { return }
        let connected = central.retrieveConnectedPeripherals(withServices: [Self.serviceUUID, Self.hidServiceUUID])
        guard let device = connected.first(where: { $0.name == "DialDeck" }) else {
            state = .searching
            scheduleRetry()
            return
        }
        retryTimer?.invalidate()
        peripheral = device
        device.delegate = self
        state = .connecting
        central.connect(device)
    }

    private func scheduleRetry() {
        retryTimer?.invalidate()
        retryTimer = Timer.scheduledTimer(withTimeInterval: 2, repeats: false) { [weak self] _ in
            Task { @MainActor in self?.findDevice() }
        }
    }

    private func reset(_ newState: State) {
        peripheral = nil
        deckCharacteristic = nil
        settingsCharacteristic = nil
        agentsCharacteristic = nil
        pendingWrites.removeAll()
        pendingSettings = nil
        state = newState
    }
}

extension DeviceLink: CBCentralManagerDelegate {
    public nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        MainActor.assumeIsolated {
            if central.state == .poweredOn {
                findDevice()
            } else {
                reset(.bluetoothOff)
            }
        }
    }

    public nonisolated func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        MainActor.assumeIsolated {
            peripheral.discoverServices([Self.serviceUUID])
        }
    }

    public nonisolated func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        MainActor.assumeIsolated {
            reset(.failed(error?.localizedDescription ?? "Connection failed"))
            scheduleRetry()
        }
    }

    public nonisolated func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        MainActor.assumeIsolated {
            reset(.searching)
            scheduleRetry()
        }
    }
}

extension DeviceLink: CBPeripheralDelegate {
    public nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        MainActor.assumeIsolated {
            guard let service = peripheral.services?.first(where: { $0.uuid == Self.serviceUUID }) else {
                reset(.failed("The device has no config service. Update its firmware."))
                return
            }
            peripheral.discoverCharacteristics(
                [Self.infoUUID, Self.deckUUID, Self.settingsUUID, Self.agentsUUID, Self.actionUUID], for: service)
        }
    }

    public nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        MainActor.assumeIsolated {
            let characteristics = service.characteristics ?? []
            guard let info = characteristics.first(where: { $0.uuid == Self.infoUUID }),
                  let deck = characteristics.first(where: { $0.uuid == Self.deckUUID }),
                  let settings = characteristics.first(where: { $0.uuid == Self.settingsUUID }),
                  let agents = characteristics.first(where: { $0.uuid == Self.agentsUUID }),
                  let action = characteristics.first(where: { $0.uuid == Self.actionUUID }) else {
                reset(.failed("The config service is incomplete. Update the firmware."))
                return
            }
            deckCharacteristic = deck
            settingsCharacteristic = settings
            agentsCharacteristic = agents
            peripheral.setNotifyValue(true, for: action)
            peripheral.readValue(for: info)
        }
    }

    public nonisolated func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        MainActor.assumeIsolated {
            if let error {
                state = .failed(error.localizedDescription)
                return
            }
            let value = characteristic.value ?? Data()
            switch characteristic.uuid {
            case Self.infoUUID:
                guard value.first == Self.supportedProtocol else {
                    reset(.failed("Unsupported firmware protocol \(value.first.map(String.init) ?? "?")."))
                    return
                }
                reload()
            case Self.deckUUID:
                slots = stride(from: 0, to: value.count - DeckSlot.wireSize + 1, by: DeckSlot.wireSize).compactMap {
                    DeckSlot(wire: value.subdata(in: $0..<$0 + DeckSlot.wireSize))
                }
                state = .ready
            case Self.settingsUUID:
                settings = DeviceSettings(wire: value)
            case Self.actionUUID:
                // [action, id length, id]
                guard value.count >= 2, let action = AgentAction(rawValue: value[0]),
                      value.count == 2 + Int(value[1]) else { return }
                onAgentAction?(action, String(decoding: value.dropFirst(2), as: UTF8.self))
            default:
                break
            }
        }
    }

    public nonisolated func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        MainActor.assumeIsolated {
            if characteristic.uuid == Self.agentsUUID {
                // A refused list is replaced by the next one; it must not break the editor.
                if let error { NSLog("DialDeck: agents write failed: \(error.localizedDescription)") }
                return
            }
            if characteristic.uuid == Self.settingsUUID {
                if let error {
                    state = .failed(error.localizedDescription)
                } else if let pendingSettings {
                    settings = pendingSettings
                }
                pendingSettings = nil
                return
            }
            guard !pendingWrites.isEmpty else { return }
            let (index, slot) = pendingWrites.removeFirst()
            if let error {
                state = .failed(error.localizedDescription)
            } else if index < slots.count {
                slots[index] = slot
            }
        }
    }
}
