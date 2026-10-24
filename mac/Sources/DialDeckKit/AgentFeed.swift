import Foundation

/// What the device asks the Mac to do with an agent. Mirrors agent_action_t in
/// firmware/main/agents.h.
public enum AgentAction: UInt8, Sendable {
    case focus = 1
    case enter
    case esc
    case up
    case down

    /// herdr's name for the key this action sends, or nil for focus.
    var herdrKey: String? {
        switch self {
        case .focus: nil
        case .enter: "enter"
        case .esc: "esc"
        case .up: "up"
        case .down: "down"
        }
    }
}

/// One agent as the device shows it. Mirrors agent_t in firmware/main/agents.h.
public struct AgentSummary: Equatable, Sendable {
    /// Wire values; also the display order, so what needs a reply comes first.
    public enum Status: UInt8, Sendable {
        case blocked = 0, done, working, idle, unknown

        init(herdr value: String?) {
            switch value {
            case "blocked": self = .blocked
            case "done": self = .done
            case "working": self = .working
            case "idle": self = .idle
            default: self = .unknown
            }
        }
    }

    public var status: Status
    public var id: String       // herdr pane id, the target of agent commands
    public var title: String
    public var detail: String   // what a blocked agent is asking

    public init(status: Status, id: String, title: String, detail: String = "") {
        self.status = status
        self.id = id
        self.title = title
        self.detail = detail
    }
}

/// Encoding of the agent list for the device (see docs/config-protocol.md).
public enum AgentFeed {
    public static let maxAgents = 32
    public static let pageSize = 8
    public static let maxBytes = 512
    // Byte limits including the device's NUL terminator.
    static let idLength = 24
    static let titleLength = 96
    static let detailLength = 160

    /// One write per page of eight: [page][page count][count] then per agent
    /// [status][id len][id][title len][title][detail len][detail]. Titles and details are cut on
    /// character boundaries to fit the device and one 512-byte write.
    public static func encode(_ agents: [AgentSummary]) -> [Data] {
        let shown = Array(agents.prefix(maxAgents))
        let pages = max(1, (shown.count + pageSize - 1) / pageSize)
        return (0..<pages).map { page in
            let agents = shown.dropFirst(page * pageSize).prefix(pageSize)
            // Details matter only for blocked agents, and are the first thing to shorten.
            for detailLimit in [detailLength - 1, 80, 0] {
                var data = Data([UInt8(page), UInt8(pages), UInt8(agents.count)])
                for agent in agents {
                    data.append(agent.status.rawValue)
                    append(agent.id, limit: idLength - 1, to: &data)
                    append(agent.title, limit: titleLength - 1, to: &data)
                    append(agent.detail, limit: detailLimit, to: &data)
                }
                if data.count <= maxBytes || detailLimit == 0 {
                    return data
                }
            }
            return Data()
        }
    }

    private static func append(_ text: String, limit: Int, to data: inout Data) {
        var bytes = Data()
        for character in text {
            let encoded = Data(String(character).utf8)
            if bytes.count + encoded.count > limit { break }
            bytes.append(encoded)
        }
        data.append(UInt8(bytes.count))
        data.append(bytes)
    }
}
