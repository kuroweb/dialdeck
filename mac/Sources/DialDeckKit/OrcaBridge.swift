import AppKit
import Foundation

/// Keeps the device's agent list in step with local Orca sessions and carries out supported actions.
/// Orca is queried through its JSON CLI; paired execution hosts are intentionally not included.
@MainActor
public final class OrcaBridge: ObservableObject {
    public enum State: Equatable {
        case starting
        case running(agents: Int)
        case unavailable(String)
    }

    @Published public private(set) var state: State = .starting

    private let link: DeviceLink
    private var timer: Timer?
    private var lastSent: [Data]?
    private var linkReady = false
    private var generation = 0
    private var pollingGeneration: Int?
    private var idsByPaneKey: [String: String] = [:]
    private var handlesByID: [String: String] = [:]
    private var detailCache: [String: (stateStartedAt: Int?, text: String)] = [:]

    public init(link: DeviceLink) {
        self.link = link
    }

    public func start() {
        timer?.invalidate()
        generation += 1
        state = .starting
        let activeGeneration = generation
        timer = Timer.scheduledTimer(withTimeInterval: 5, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.poll(generation: activeGeneration) }
        }
        poll(generation: activeGeneration)
    }

    public func stop() {
        generation += 1
        timer?.invalidate()
        timer = nil
        pollingGeneration = nil
        lastSent = nil
        idsByPaneKey = [:]
        handlesByID = [:]
        detailCache = [:]
    }

    private func poll(generation pollGeneration: Int) {
        guard pollGeneration == generation else { return }
        let ready = link.state == .ready
        if ready && !linkReady {
            lastSent = nil
        }
        linkReady = ready
        guard pollingGeneration != pollGeneration else { return }
        pollingGeneration = pollGeneration

        let oldIDs = idsByPaneKey
        let oldDetails = detailCache
        Task.detached {
            let result = Result { try Self.fetchAgents(idsByPaneKey: oldIDs, detailCache: oldDetails) }
            await MainActor.run {
                guard pollGeneration == self.generation else { return }
                self.pollingGeneration = nil
                switch result {
                case .success(let snapshot):
                    self.idsByPaneKey = snapshot.idsByPaneKey
                    self.handlesByID = snapshot.handlesByID
                    self.detailCache = snapshot.detailCache
                    self.state = .running(agents: snapshot.agents.count)
                    self.send(snapshot.agents)
                case .failure(let error):
                    self.idsByPaneKey = [:]
                    self.handlesByID = [:]
                    self.detailCache = [:]
                    self.state = .unavailable(error.localizedDescription)
                    self.send([])
                }
            }
        }
    }

    private func send(_ agents: [AgentSummary]) {
        guard link.state == .ready else { return }
        let pages = AgentFeed.encode(agents)
        if pages != lastSent {
            lastSent = pages
            link.writeAgents(pages)
        }
    }

    /// Perform an action using the latest in-memory mapping from the short device ID to a local
    /// Orca terminal handle. Focus switches Orca to the terminal; key actions are sent as terminal
    /// input.
    public func perform(_ action: AgentAction, on id: String) {
        guard let handle = handlesByID[id] else { return }
        Task.detached {
            do {
                switch action {
                case .focus:
                    _ = try Self.runCLI(["terminal", "switch", "--terminal", handle, "--json"])
                case .enter:
                    _ = try Self.runCLI(["terminal", "send", "--terminal", handle, "--enter", "--json"])
                case .esc, .up, .down:
                    let key = switch action {
                    case .esc: "\u{1b}"
                    case .up: "\u{1b}[A"
                    case .down: "\u{1b}[B"
                    default: ""
                    }
                    _ = try Self.runCLI(["terminal", "send", "--terminal", handle, "--text", key, "--json"])
                }
            } catch {
                NSLog("DialDeck: Orca action \(action) failed: \(error.localizedDescription)")
            }
        }
    }

    private struct Snapshot {
        var agents: [AgentSummary]
        var idsByPaneKey: [String: String]
        var handlesByID: [String: String]
        var detailCache: [String: (stateStartedAt: Int?, text: String)]
    }

    private nonisolated static func fetchAgents(
        idsByPaneKey oldIDs: [String: String],
        detailCache oldDetails: [String: (stateStartedAt: Int?, text: String)]
    ) throws -> Snapshot {
        let ps = try runCLI(["worktree", "ps", "--json"])
        let terminalList = try runCLI(["terminal", "list", "--json"])
        let worktrees = ps["worktrees"] as? [[String: Any]] ?? []
        let terminals = terminalList["terminals"] as? [[String: Any]] ?? []
        var handlesByPaneKey: [String: String] = [:]
        for terminal in terminals where terminal["executionHostId"] as? String == "local" {
            guard let tabID = terminal["tabId"] as? String,
                  let leafID = terminal["leafId"] as? String,
                  let handle = terminal["handle"] as? String else { continue }
            handlesByPaneKey["\(tabID):\(leafID)"] = handle
        }
        var rawAgents: [(paneKey: String, entry: [String: Any], handle: String)] = []
        for worktree in worktrees where worktree["hostId"] as? String == "local" {
            guard let entries = worktree["agents"] as? [[String: Any]] else { continue }
            for entry in entries {
                guard let paneKey = entry["paneKey"] as? String,
                      let handle = handlesByPaneKey[paneKey] else { continue }
                rawAgents.append((paneKey, entry, handle))
            }
        }

        var idsByPaneKey: [String: String] = [:]
        var handlesByID: [String: String] = [:]
        var agents: [AgentSummary] = []
        var detailCache: [String: (stateStartedAt: Int?, text: String)] = [:]
        for item in rawAgents {
            let id = oldIDs[item.paneKey] ?? opaqueID(avoiding: Set(handlesByID.keys))
            idsByPaneKey[item.paneKey] = id
            handlesByID[id] = item.handle
            let title = Self.agentTitle(item.entry)
            let status = AgentSummary.Status(agentState: item.entry["state"] as? String)
            var detail = ""
            if status == .blocked {
                let stateStartedAt = item.entry["stateStartedAt"] as? Int
                if let cached = oldDetails[item.paneKey], cached.stateStartedAt == stateStartedAt {
                    detail = cached.text
                } else {
                    detail = (try? readScreen(item.handle)) ?? ""
                }
                detailCache[item.paneKey] = (stateStartedAt, detail)
            }
            agents.append(AgentSummary(status: status, id: id, title: title, detail: detail))
        }
        agents.sort {
            $0.status.rawValue != $1.status.rawValue ? $0.status.rawValue < $1.status.rawValue
                : $0.title.localizedStandardCompare($1.title) == .orderedAscending
        }
        return Snapshot(agents: agents, idsByPaneKey: idsByPaneKey, handlesByID: handlesByID,
                        detailCache: detailCache)
    }

    private nonisolated static func agentTitle(_ entry: [String: Any]) -> String {
        for key in ["taskTitle", "displayName", "prompt", "agentType"] {
            guard let value = entry[key] as? String else { continue }
            let firstLine = value.split(whereSeparator: \.isNewline).first ?? ""
            let title = firstLine.split(whereSeparator: \.isWhitespace).joined(separator: " ")
            if !title.isEmpty { return title }
        }
        return "Orca agent"
    }

    /// Use the rendered screen only for a blocked agent. Draft composer text is UI-only and is
    /// intentionally excluded by Orca's CLI response.
    private nonisolated static func readScreen(_ handle: String) throws -> String {
        let result = try runCLI(["terminal", "read", "--terminal", handle, "--screen", "--limit", "8", "--json"])
        let terminal = result["terminal"] as? [String: Any] ?? [:]
        let lines = terminal["tail"] as? [String] ?? []
        return lines.map { $0.trimmingCharacters(in: .whitespaces) }
            .filter { !$0.isEmpty }
            .suffix(4)
            .joined(separator: "\n")
    }

    /// Device IDs stay below AgentFeed's 23-byte payload limit and reveal no Orca handle.
    private nonisolated static func opaqueID(avoiding existing: Set<String>) -> String {
        while true {
            let id = "o" + String(UUID().uuidString.replacingOccurrences(of: "-", with: "").prefix(22))
            if !existing.contains(id) { return id }
        }
    }

    /// Run Orca with an argument vector; no shell is involved. The top-level JSON envelope is
    /// unwrapped here so callers consume only the command's result object.
    private nonisolated static func runCLI(_ arguments: [String]) throws -> [String: Any] {
        let process = Process()
        let override = ProcessInfo.processInfo.environment["ORCA_CLI_COMMAND"]
        let bundledCLI = "/Applications/Orca.app/Contents/Resources/bin/orca"
        let command = override.flatMap { $0.isEmpty ? nil : $0 }
        if let command, command.contains("/") {
            process.executableURL = URL(fileURLWithPath: command)
            process.arguments = arguments
        } else if let command {
            process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
            process.arguments = [command] + arguments
        } else if FileManager.default.isExecutableFile(atPath: bundledCLI) {
            process.executableURL = URL(fileURLWithPath: bundledCLI)
            process.arguments = arguments
        } else {
            process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
            process.arguments = ["orca"] + arguments
        }
        let output = Pipe()
        process.standardOutput = output
        process.standardError = FileHandle.nullDevice
        try process.run()
        let data = output.fileHandleForReading.readDataToEndOfFile()
        process.waitUntilExit()
        guard process.terminationStatus == 0,
              let envelope = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              envelope["ok"] as? Bool == true,
              let result = envelope["result"] as? [String: Any] else {
            throw CLIError.commandFailed(arguments.first ?? "command")
        }
        return result
    }

    private enum CLIError: LocalizedError {
        case commandFailed(String)

        var errorDescription: String? {
            switch self {
            case .commandFailed(let command): "Orca \(command) did not return a successful JSON result."
            }
        }
    }
}
