import Foundation

/// Selects which local agent host supplies the device's Agents screen.
@MainActor
public final class AgentSourceController: ObservableObject {
    public enum Source: String, CaseIterable, Identifiable, Sendable {
        case herdr
        case orca

        public var id: Self { self }

        public var name: String {
            switch self {
            case .herdr: "herdr"
            case .orca: "Orca"
            }
        }

        var wireValue: UInt8 { self == .herdr ? 0 : 1 }

        init?(wireValue: UInt8) {
            switch wireValue {
            case 0: self = .herdr
            case 1: self = .orca
            default: return nil
            }
        }
    }

    private static let sourceKey = "agentSource"

    @Published public private(set) var source: Source

    private let link: DeviceLink
    private let herdr: HerdrBridge
    private let orca: OrcaBridge

    public init(link: DeviceLink, herdr: HerdrBridge, orca: OrcaBridge) {
        self.link = link
        self.herdr = herdr
        self.orca = orca
        source = Source(rawValue: UserDefaults.standard.string(forKey: Self.sourceKey) ?? "") ?? .herdr
        link.onAgentAction = { [weak self] action, id in self?.perform(action, on: id) }
        link.onAgentHost = { [weak self] value in self?.selectFromDevice(value) }
    }

    public func start() {
        activate(clearList: false)
    }

    public func select(_ source: Source) {
        guard source != self.source else { return }
        self.source = source
        UserDefaults.standard.set(source.rawValue, forKey: Self.sourceKey)
        activate(clearList: true)
        link.writeAgentHost(source.wireValue)
    }

    private func selectFromDevice(_ value: UInt8) {
        guard let selected = Source(wireValue: value), selected != source else { return }
        source = selected
        UserDefaults.standard.set(selected.rawValue, forKey: Self.sourceKey)
        activate(clearList: true)
    }

    private func activate(clearList: Bool) {
        herdr.stop()
        orca.stop()
        if clearList {
            link.writeAgents(AgentFeed.encode([]))
        }
        switch source {
        case .herdr: herdr.start()
        case .orca: orca.start()
        }
    }

    private func perform(_ action: AgentAction, on id: String) {
        switch source {
        case .herdr: herdr.perform(action, on: id)
        case .orca: orca.perform(action, on: id)
        }
    }
}
