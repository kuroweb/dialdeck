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
    static let hidServiceUUID = CBUUID(string: "1812")
    static let supportedProtocol: UInt8 = 1

    @Published public private(set) var state: State = .searching
    @Published public private(set) var slots: [DeckSlot] = []

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var deckCharacteristic: CBCharacteristic?
    private var retryTimer: Timer?
    private var pendingWrites: [(Int, DeckSlot)] = []

    public override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    /// Read every assignment again.
    public func reload() {
        guard let peripheral, let deckCharacteristic else { return }
        peripheral.readValue(for: deckCharacteristic)
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
        pendingWrites.removeAll()
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
            peripheral.discoverCharacteristics([Self.infoUUID, Self.deckUUID], for: service)
        }
    }

    public nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        MainActor.assumeIsolated {
            let characteristics = service.characteristics ?? []
            guard let info = characteristics.first(where: { $0.uuid == Self.infoUUID }),
                  let deck = characteristics.first(where: { $0.uuid == Self.deckUUID }) else {
                reset(.failed("The config service is incomplete."))
                return
            }
            deckCharacteristic = deck
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
            default:
                break
            }
        }
    }

    public nonisolated func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        MainActor.assumeIsolated {
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
