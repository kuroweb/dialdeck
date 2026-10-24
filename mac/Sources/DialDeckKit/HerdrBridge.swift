import AppKit
import Foundation

/// Keeps the device's agent list in step with herdr and carries out what the device asks.
///
/// herdr is polled once a second over its socket; the list is written to the device only when it
/// changes, or when the device connects again.
@MainActor
public final class HerdrBridge: ObservableObject {
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
    private var polling = false
    // What a blocked agent is asking, keyed by pane id and the state change it belongs to, so the
    // screen is read once per prompt rather than every second.
    private var detailCache: [String: (seq: Int, text: String)] = [:]

    /// Terminal apps brought to the front after focusing an agent, in order of preference.
    static let terminalBundleIDs = [
        "com.mitchellh.ghostty", "com.googlecode.iterm2", "dev.warp.Warp-Stable",
        "net.kovidgoyal.kitty", "com.github.wez.wezterm", "com.apple.Terminal",
    ]

    public init(link: DeviceLink) {
        self.link = link
        link.onAgentAction = { [weak self] action, id in self?.perform(action, on: id) }
    }

    public func start() {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.poll() }
        }
        poll()
    }

    private func poll() {
        // Resend after the device reconnects: it forgets the list when it restarts.
        let ready = link.state == .ready
        if ready && !linkReady {
            lastSent = nil
        }
        linkReady = ready
        guard !polling else { return }
        polling = true

        let cache = detailCache
        Task.detached {
            let result = Result { try Self.fetchAgents(detailCache: cache) }
            await MainActor.run {
                self.polling = false
                switch result {
                case .success(let (agents, cache)):
                    self.detailCache = cache
                    self.state = .running(agents: agents.count)
                    self.send(agents)
                case .failure(let error):
                    self.state = .unavailable(error.localizedDescription)
                    self.send([])
                }
            }
        }
    }

    private func send(_ agents: [AgentSummary]) {
        guard link.state == .ready else { return }
        let data = AgentFeed.encode(agents)
        if data != lastSent {
            lastSent = data
            link.writeAgents(data)
        }
    }

    private nonisolated static func fetchAgents(detailCache: [String: (seq: Int, text: String)])
        throws -> ([AgentSummary], [String: (seq: Int, text: String)]) {
        let result = try HerdrSocket.request("agent.list")
        let raw = result["agents"] as? [[String: Any]] ?? []
        var cache: [String: (seq: Int, text: String)] = [:]

        let agents: [AgentSummary] = raw.compactMap { entry in
            guard let pane = entry["pane_id"] as? String else { return nil }
            let status = AgentSummary.Status(herdr: entry["agent_status"] as? String)
            let title = (entry["terminal_title_stripped"] as? String)
                .flatMap { $0.trimmingCharacters(in: .whitespaces).isEmpty ? nil : $0 } ?? pane
            var detail = ""
            if status == .blocked {
                let seq = entry["state_change_seq"] as? Int ?? 0
                if let cached = detailCache[pane], cached.seq == seq {
                    detail = cached.text
                } else {
                    detail = (try? readPrompt(pane)) ?? ""
                }
                cache[pane] = (seq, detail)
            }
            return AgentSummary(status: status, id: pane, title: title, detail: detail)
        }
        let sorted = agents.sorted {
            $0.status.rawValue != $1.status.rawValue ? $0.status.rawValue < $1.status.rawValue
                : $0.title.localizedStandardCompare($1.title) == .orderedAscending
        }
        return (sorted, cache)
    }

    /// The last lines of what herdr detected as the agent's prompt: usually the question and the
    /// highlighted choice.
    private nonisolated static func readPrompt(_ pane: String) throws -> String {
        let result = try HerdrSocket.request("agent.read", ["target": pane, "source": "detection"])
        let text = (result["read"] as? [String: Any])?["text"] as? String ?? ""
        let lines = text.split(separator: "\n").map { $0.trimmingCharacters(in: .whitespaces) }.filter { !$0.isEmpty }
        return lines.suffix(4).joined(separator: "\n")
    }

    private func perform(_ action: AgentAction, on pane: String) {
        Task.detached {
            do {
                if let key = action.herdrKey {
                    _ = try HerdrSocket.request("agent.send_keys", ["target": pane, "keys": [key]])
                } else {
                    try Self.focus(pane)
                    await MainActor.run { Self.activateTerminal() }
                }
            } catch {
                NSLog("DialDeck: herdr action \(action) on \(pane) failed: \(error.localizedDescription)")
            }
        }
    }

    /// herdr only moves its own focus to the pane; the workspace and tab have to be focused first
    /// for the pane to be on screen.
    private nonisolated static func focus(_ pane: String) throws {
        let info = try HerdrSocket.request("agent.get", ["target": pane])["agent"] as? [String: Any] ?? [:]
        if let workspace = info["workspace_id"] as? String {
            _ = try HerdrSocket.request("workspace.focus", ["workspace_id": workspace])
        }
        if let tab = info["tab_id"] as? String {
            _ = try HerdrSocket.request("tab.focus", ["tab_id": tab])
        }
        do {
            _ = try HerdrSocket.request("pane.focus", ["pane_id": pane])
        } catch {
            _ = try HerdrSocket.request("agent.focus", ["target": pane])
        }
    }

    private static func activateTerminal() {
        for bundleID in terminalBundleIDs {
            if let app = NSRunningApplication.runningApplications(withBundleIdentifier: bundleID).first {
                app.activate()
                return
            }
        }
    }
}
