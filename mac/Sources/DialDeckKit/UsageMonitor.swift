import Foundation
import Security
import SQLite3

/// Coding agent whose plan usage is shown in the middle of the device's agents screen.
public enum UsageProvider: String, CaseIterable, Identifiable, Sendable {
    case off, claude, codex, cursor

    public var id: String { rawValue }

    /// Agents whose usage may be offered on this Mac (not Off).
    public static let configurable: [UsageProvider] = [.claude, .codex, .cursor]

    public var name: String {
        switch self {
        case .off: "Off"
        case .claude: "Claude"
        case .codex: "Codex"
        case .cursor: "Cursor"
        }
    }

    /// Bit in the usage_sources GATT byte (protocol 5).
    public var sourceBit: UInt8? {
        switch self {
        case .claude: 1 << 0
        case .codex: 1 << 1
        case .cursor: 1 << 2
        case .off: nil
        }
    }

    public static func enabledProviders(from mask: UInt8) -> Set<UsageProvider> {
        Set(configurable.filter { provider in
            guard let bit = provider.sourceBit else { return false }
            return mask & bit != 0
        })
    }

    public static func mask(from enabled: Set<UsageProvider>) -> UInt8 {
        configurable.reduce(UInt8(0)) { partial, provider in
            guard enabled.contains(provider), let bit = provider.sourceBit else { return partial }
            return partial | bit
        }
    }
}

/// One limit window of a plan, such as Claude's five hours.
public struct UsageWindow: Equatable, Sendable {
    public var label: String
    public var percent: Int?   // nil when unknown
    public var resetsAt: Date?
}

/// Reads plan usage for the chosen provider and writes it to the device. Local logs are read once
/// a minute; web APIs every five minutes, and less often after they answer 429, with the last
/// values kept on the device in the meantime.
///
/// Claude: the usage endpoint Claude Code's /usage reads, with the OAuth token Claude Code keeps
/// in the login keychain. Codex: the newest readable `rate_limits` on a session log's token_count
/// events (rows with empty windows after a limit are skipped for an older row in the same file).
/// Cursor: the usage summary of cursor.com, with the token Cursor keeps in its state database.
/// None of these are public APIs; when one fails before any value was read, the device shows the
/// windows as unknown.
@MainActor
public final class UsageMonitor: ObservableObject {
    public enum State: Equatable {
        case off
        case running([UsageWindow])
        case stale([UsageWindow], String)   // the last values, and why reading again failed
        case unavailable(String)
    }

    static let providerKey = "usageProvider"
    private static let enabledProvidersKey = "usageEnabledProviders"

    @Published public private(set) var state: State = .off
    @Published public private(set) var enabledProviders: Set<UsageProvider>
    @Published public var provider: UsageProvider {
        didSet {
            UserDefaults.standard.set(provider.rawValue, forKey: Self.providerKey)
            syncStateForProvider()
            poll()
        }
    }

    /// Off plus agents enabled in settings (picker and device tap use this order).
    public var selectableProviders: [UsageProvider] {
        [.off] + UsageProvider.configurable.filter { enabledProviders.contains($0) }
    }

    private let link: DeviceLink
    private var timer: Timer?
    private var lastSent: Data?
    private var linkReady = false
    // Per provider: the last values read, and when it may be asked again.
    private var cache: [UsageProvider: [UsageWindow]] = [:]
    private var nextFetch: [UsageProvider: Date] = [:]
    private var backoff: [UsageProvider: TimeInterval] = [:]
    private var fetching: Set<UsageProvider> = []
    private var applyingDeviceMask = false

    public init(link: DeviceLink) {
        self.link = link
        let enabled = Self.loadEnabledProviders()
        enabledProviders = enabled
        var chosen = UsageProvider(rawValue: UserDefaults.standard.string(forKey: Self.providerKey) ?? "") ?? .off
        if chosen != .off, !enabled.contains(chosen) {
            chosen = .off
        }
        provider = chosen
        link.onNextUsage = { [weak self] in self?.showNext() }
        link.onUsageSourcesMask = { [weak self] in self?.applyEnabledMaskFromDevice($0) }
    }

    public func isProviderEnabled(_ provider: UsageProvider) -> Bool {
        provider == .off || enabledProviders.contains(provider)
    }

    public func setProviderEnabled(_ provider: UsageProvider, enabled: Bool) {
        guard UsageProvider.configurable.contains(provider) else { return }
        var set = enabledProviders
        if enabled {
            set.insert(provider)
        } else {
            set.remove(provider)
        }
        enabledProviders = set
        UserDefaults.standard.set(set.map(\.rawValue).sorted(), forKey: Self.enabledProvidersKey)
        if !enabled, self.provider == provider {
            self.provider = .off
        }
        if !applyingDeviceMask {
            link.writeUsageSources(UsageProvider.mask(from: set))
        }
    }

    /// Device settings or a GATT read/notify replaced the mask (protocol 5).
    public func applyEnabledMaskFromDevice(_ mask: UInt8) {
        applyingDeviceMask = true
        let set = UsageProvider.enabledProviders(from: mask)
        enabledProviders = set
        UserDefaults.standard.set(set.map(\.rawValue).sorted(), forKey: Self.enabledProvidersKey)
        if provider != .off, !set.contains(provider) {
            provider = .off
        }
        applyingDeviceMask = false
    }

    private static func loadEnabledProviders() -> Set<UsageProvider> {
        guard let raw = UserDefaults.standard.stringArray(forKey: enabledProvidersKey) else {
            return Set(UsageProvider.configurable)
        }
        return Set(raw.compactMap(UsageProvider.init(rawValue:)))
    }

    /// Switch to the next enabled provider, wrapping around through Off.
    public func showNext() {
        let order = selectableProviders
        guard !order.isEmpty else {
            provider = .off
            return
        }
        let index = order.firstIndex(of: provider) ?? 0
        provider = order[(index + 1) % order.count]
    }

    public func start() {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 60, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.poll() }
        }
        // Resend soon after the device reconnects: it forgets the usage when it restarts.
        Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.checkLink() }
        }
        syncStateForProvider()
        poll()
    }

    /// Match `state` and the device payload to the selected provider (not the previous fetch).
    private func syncStateForProvider() {
        if provider == .off {
            state = .off
            send(provider, [])
            return
        }
        if let windows = cache[provider] {
            state = .running(windows)
            send(provider, windows)
        } else {
            state = .off
            send(provider, [UsageWindow(label: "--", percent: nil, resetsAt: nil)])
        }
    }

    private func checkLink() {
        let ready = link.state == .ready
        defer { linkReady = ready }
        if ready && !linkReady {
            lastSent = nil
            poll()
        }
    }

    /// How often a provider is read when it answers normally.
    private static func interval(_ provider: UsageProvider) -> TimeInterval {
        provider == .codex ? 60 : 300
    }

    private func poll() {
        let provider = provider
        guard provider != .off else {
            state = .off
            send(provider, [])
            return
        }
        guard Date() >= nextFetch[provider, default: .distantPast], !fetching.contains(provider) else {
            // Not yet: keep the device's reset times current from the last values.
            if let windows = cache[provider] { send(provider, windows) }
            return
        }
        fetching.insert(provider)
        Task.detached {
            let result: Result<[UsageWindow], Error>
            do {
                result = .success(try await Self.fetch(provider))
            } catch {
                result = .failure(error)
            }
            await MainActor.run { self.finish(provider, result) }
        }
    }

    private func finish(_ provider: UsageProvider, _ result: Result<[UsageWindow], Error>) {
        fetching.remove(provider)
        let normal = Self.interval(provider)
        switch result {
        case .success(let windows):
            cache[provider] = windows
            backoff[provider] = nil
            nextFetch[provider] = Date().addingTimeInterval(normal)
        case .failure(let error):
            // Back off, doubling up to half an hour, or as long as the server asks.
            let wait = min(max(backoff[provider].map { $0 * 2 } ?? normal, normal), 1800)
            backoff[provider] = wait
            let retryAfter = (error as? FetchError)?.retryAfter ?? 0
            nextFetch[provider] = Date().addingTimeInterval(max(wait, retryAfter))
        }
        // The choice may have changed while fetching.
        guard self.provider == provider else { return }
        switch (result, cache[provider]) {
        case (.success(let windows), _):
            state = .running(windows)
            send(provider, windows)
        case (.failure(let error), let windows?):
            state = .stale(windows, error.localizedDescription)
            send(provider, windows)
        case (.failure(let error), nil):
            state = .unavailable(error.localizedDescription)
            send(provider, [UsageWindow(label: "--", percent: nil, resetsAt: nil)])
        }
    }

    private func send(_ provider: UsageProvider, _ windows: [UsageWindow]) {
        guard link.state == .ready else { return }
        let data = Self.encode(provider, windows, now: Date())
        if data != lastSent {
            lastSent = data
            link.writeUsage(data)
        }
    }

    /// [name len][name][window count] then per window [label len][label][percent][reset minutes LE16].
    /// An empty name tells the device to show no usage.
    static func encode(_ provider: UsageProvider, _ windows: [UsageWindow], now: Date) -> Data {
        let name = provider == .off ? "" : provider.name
        var data = Data([UInt8(name.utf8.count)])
        data.append(contentsOf: name.utf8)
        let shown = windows.prefix(2)
        data.append(UInt8(shown.count))
        for window in shown {
            let label = Data(window.label.utf8.prefix(7))
            data.append(UInt8(label.count))
            data.append(label)
            data.append(window.percent.map { UInt8(min(max($0, 0), 100)) } ?? 0xFF)
            let minutes = window.resetsAt.map { UInt16(min(max($0.timeIntervalSince(now) / 60, 0), 0xFFFE)) } ?? 0xFFFF
            data.append(UInt8(minutes & 0xFF))
            data.append(UInt8(minutes >> 8))
        }
        return data
    }

    struct FetchError: LocalizedError {
        let errorDescription: String?
        var retryAfter: TimeInterval?   // seconds the server asked to wait
        init(_ message: String, retryAfter: TimeInterval? = nil) {
            errorDescription = message
            self.retryAfter = retryAfter
        }
    }

    private nonisolated static func fetch(_ provider: UsageProvider) async throws -> [UsageWindow] {
        switch provider {
        case .off: []
        case .claude: try await fetchClaude()
        case .codex: try fetchCodex()
        case .cursor: try await fetchCursor()
        }
    }

    // MARK: Claude

    private nonisolated static func fetchClaude() async throws -> [UsageWindow] {
        guard let credentials = keychainPassword(service: "Claude Code-credentials"),
              let json = try JSONSerialization.jsonObject(with: credentials) as? [String: Any],
              let oauth = json["claudeAiOauth"] as? [String: Any],
              let token = oauth["accessToken"] as? String else {
            throw FetchError("Claude Code is not signed in")
        }
        var request = URLRequest(url: URL(string: "https://api.anthropic.com/api/oauth/usage")!)
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        request.setValue("oauth-2025-04-20", forHTTPHeaderField: "anthropic-beta")
        let body = try await fetchJSON(request)
        return [("five_hour", "5h"), ("seven_day", "7d")].map { key, label in
            let window = body[key] as? [String: Any]
            return UsageWindow(label: label,
                               percent: (window?["utilization"] as? Double).map { Int($0.rounded()) },
                               resetsAt: (window?["resets_at"] as? String).flatMap(parseDate))
        }
    }

    private nonisolated static func keychainPassword(service: String) -> Data? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne,
        ]
        var item: CFTypeRef?
        guard SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess else { return nil }
        return item as? Data
    }

    // MARK: Codex

    /// Codex logs its rate limits with every token count, so the newest session has the latest.
    private nonisolated static func fetchCodex() throws -> [UsageWindow] {
        let sessions = FileManager.default.homeDirectoryForCurrentUser.appending(path: ".codex/sessions")
        let keys: [URLResourceKey] = [.contentModificationDateKey]
        let files = (FileManager.default.enumerator(at: sessions, includingPropertiesForKeys: keys)?
            .compactMap { $0 as? URL } ?? [])
            .filter { $0.pathExtension == "jsonl" }
            .map { ($0, (try? $0.resourceValues(forKeys: Set(keys)).contentModificationDate) ?? .distantPast) }
            .sorted { $0.1 > $1.1 }
        for (file, _) in files.prefix(8) {
            if let limits = lastRateLimits(in: file) {
                return codexUsageWindows(from: limits)
            }
        }
        throw FetchError("No Codex session with rate limits")
    }

    /// Skip token_count rows where Codex cleared the windows (for example after a limit), so an older row is used.
    private nonisolated static func codexLimitsAreReadable(_ limits: [String: Any]) -> Bool {
        for key in ["primary", "secondary"] {
            if let window = limits[key] as? [String: Any], window["used_percent"] != nil {
                return true
            }
        }
        return false
    }

    private nonisolated static func codexUsageWindows(from limits: [String: Any]) -> [UsageWindow] {
        [("primary", "5h"), ("secondary", "7d")].map { key, fallback in
            let window = limits[key] as? [String: Any]
            let label = (window?["window_minutes"] as? Int).map(windowLabel) ?? fallback
            let resetsAt = (window?["resets_at"] as? Double).map { Date(timeIntervalSince1970: $0) }
            // No new token_count after the window rolled; do not guess 0% from a stale log.
            if let resetsAt, resetsAt < Date() {
                return UsageWindow(label: label, percent: nil, resetsAt: nil)
            }
            return UsageWindow(label: label,
                               percent: (window?["used_percent"] as? Double).map { Int($0.rounded()) },
                               resetsAt: resetsAt)
        }
    }

    /// The newest readable rate_limits in the tail of a session log.
    private nonisolated static func lastRateLimits(in file: URL) -> [String: Any]? {
        guard let handle = try? FileHandle(forReadingFrom: file) else { return nil }
        defer { try? handle.close() }
        let size = (try? handle.seekToEnd()) ?? 0
        try? handle.seek(toOffset: size > 512 * 1024 ? size - 512 * 1024 : 0)
        guard let data = try? handle.readToEnd() else { return nil }
        for line in String(decoding: data, as: UTF8.self).split(separator: "\n").reversed()
            where line.contains("\"rate_limits\"") {
            if let json = try? JSONSerialization.jsonObject(with: Data(line.utf8)) as? [String: Any],
               let payload = json["payload"] as? [String: Any],
               let limits = payload["rate_limits"] as? [String: Any],
               codexLimitsAreReadable(limits) {
                return limits
            }
        }
        return nil
    }

    private nonisolated static func windowLabel(minutes: Int) -> String {
        minutes % 1440 == 0 ? "\(minutes / 1440)d" : minutes % 60 == 0 ? "\(minutes / 60)h" : "\(minutes)m"
    }

    // MARK: Cursor

    private nonisolated static func fetchCursor() async throws -> [UsageWindow] {
        let database = FileManager.default.homeDirectoryForCurrentUser
            .appending(path: "Library/Application Support/Cursor/User/globalStorage/state.vscdb")
        guard let token = sqliteValue(database, key: "cursorAuth/accessToken"),
              let userID = jwtSubject(token)?.split(separator: "|").last else {
            throw FetchError("Cursor is not signed in")
        }
        var request = URLRequest(url: URL(string: "https://cursor.com/api/usage-summary")!)
        request.setValue("WorkosCursorSessionToken=\(userID)%3A%3A\(token)", forHTTPHeaderField: "Cookie")
        let body = try await fetchJSON(request)
        let plan = (body["individualUsage"] as? [String: Any])?["plan"] as? [String: Any]
        var percent = (plan?["totalPercentUsed"] as? Double).map { Int($0.rounded()) }
        if percent == nil, let used = plan?["used"] as? Double, let limit = plan?["limit"] as? Double, limit > 0 {
            percent = Int((used / limit * 100).rounded())
        }
        return [UsageWindow(label: "Month", percent: percent,
                            resetsAt: (body["billingCycleEnd"] as? String).flatMap(parseDate))]
    }

    private nonisolated static func sqliteValue(_ database: URL, key: String) -> String? {
        var db: OpaquePointer?
        guard sqlite3_open_v2(database.path, &db, SQLITE_OPEN_READONLY, nil) == SQLITE_OK else { return nil }
        defer { sqlite3_close(db) }
        var statement: OpaquePointer?
        guard sqlite3_prepare_v2(db, "SELECT value FROM ItemTable WHERE key = ?", -1, &statement, nil) == SQLITE_OK else {
            return nil
        }
        defer { sqlite3_finalize(statement) }
        let transient = unsafeBitCast(-1, to: sqlite3_destructor_type.self)
        sqlite3_bind_text(statement, 1, key, -1, transient)
        guard sqlite3_step(statement) == SQLITE_ROW, let text = sqlite3_column_text(statement, 0) else { return nil }
        return String(cString: text)
    }

    private nonisolated static func jwtSubject(_ token: String) -> String? {
        let parts = token.split(separator: ".")
        guard parts.count == 3 else { return nil }
        var base64 = parts[1].replacingOccurrences(of: "-", with: "+").replacingOccurrences(of: "_", with: "/")
        base64 += String(repeating: "=", count: (4 - base64.count % 4) % 4)
        guard let data = Data(base64Encoded: base64),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return nil }
        return json["sub"] as? String
    }

    // MARK: Shared

    private nonisolated static func fetchJSON(_ request: URLRequest) async throws -> [String: Any] {
        var request = request
        request.timeoutInterval = 15
        let (data, response) = try await URLSession.shared.data(for: request)
        if let http = response as? HTTPURLResponse, http.statusCode != 200 {
            throw FetchError("\(request.url?.host() ?? "Server") returned \(http.statusCode)",
                             retryAfter: (http.value(forHTTPHeaderField: "Retry-After")).flatMap(TimeInterval.init))
        }
        guard let json = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw FetchError("Unexpected response")
        }
        return json
    }

    private nonisolated static func parseDate(_ text: String) -> Date? {
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        if let date = formatter.date(from: text) { return date }
        formatter.formatOptions = [.withInternetDateTime]
        return formatter.date(from: text)
    }
}
