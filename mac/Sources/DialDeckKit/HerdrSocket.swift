import Foundation

/// One request to the herdr socket API: a JSON line out, a JSON line back.
/// Blocking; call it off the main thread.
public enum HerdrSocket {
    public enum Failure: Error, LocalizedError {
        case unreachable(String)
        case server(String)
        case malformed

        public var errorDescription: String? {
            switch self {
            case .unreachable(let path): "herdr is not running (no socket at \(path))."
            case .server(let message): message
            case .malformed: "herdr sent a response DialDeck does not understand."
            }
        }
    }

    /// The default socket of an unnamed herdr session.
    public static let defaultPath = NSString(string: "~/.config/herdr/herdr.sock").expandingTildeInPath

    public static func request(_ method: String, _ params: [String: Any] = [:],
                               path: String = defaultPath) throws -> [String: Any] {
        let fd = socket(AF_UNIX, SOCK_STREAM, 0)
        guard fd >= 0 else { throw Failure.unreachable(path) }
        defer { close(fd) }

        var timeout = timeval(tv_sec: 5, tv_usec: 0)
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, socklen_t(MemoryLayout<timeval>.size))

        var address = sockaddr_un()
        address.sun_family = sa_family_t(AF_UNIX)
        let pathBytes = Array(path.utf8)
        guard pathBytes.count < MemoryLayout.size(ofValue: address.sun_path) else { throw Failure.unreachable(path) }
        withUnsafeMutableBytes(of: &address.sun_path) { raw in
            raw.copyBytes(from: pathBytes)
        }
        let connected = withUnsafePointer(to: &address) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                connect(fd, $0, socklen_t(MemoryLayout<sockaddr_un>.size))
            }
        }
        guard connected == 0 else { throw Failure.unreachable(path) }

        var line = try JSONSerialization.data(withJSONObject: ["id": "dialdeck", "method": method, "params": params])
        line.append(0x0A)
        let sent = line.withUnsafeBytes { send(fd, $0.baseAddress, $0.count, 0) }
        guard sent == line.count else { throw Failure.unreachable(path) }

        // The response is one JSON line.
        var response = Data()
        var buffer = [UInt8](repeating: 0, count: 65536)
        while !response.contains(0x0A) {
            let n = recv(fd, &buffer, buffer.count, 0)
            guard n > 0 else { break }
            response.append(contentsOf: buffer[0..<n])
        }
        if let end = response.firstIndex(of: 0x0A) {
            response = response.prefix(upTo: end)
        }

        guard let envelope = try? JSONSerialization.jsonObject(with: response) as? [String: Any] else {
            throw Failure.malformed
        }
        if let error = envelope["error"] as? [String: Any] {
            throw Failure.server(error["message"] as? String ?? "herdr request failed")
        }
        guard let result = envelope["result"] as? [String: Any] else { throw Failure.malformed }
        return result
    }
}
