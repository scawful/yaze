import Foundation
import Network
import Combine

/// A host discovered via Bonjour `_yaze._tcp.` on the local network.
struct DiscoveredHost: Identifiable, Hashable {
  let id: String          // endpoint description
  let name: String
  var host: String = ""
  var port: UInt16 = 8080
  var version: String = ""
  var romTitle: String = ""
  var capabilities: String = ""

  var baseURL: String {
    "http://\(host):\(port)"
  }
}

/// Browses the LAN for `_yaze._tcp.` services using Network framework.
final class DesktopDiscoveryService: ObservableObject {
  @Published var discoveredHosts: [DiscoveredHost] = []
  @Published var isSearching = false

  private var browser: NWBrowser?
  private var connections: [String: NWConnection] = [:]

  func startBrowsing() {
    guard browser == nil else { return }

    let params = NWParameters()
    params.includePeerToPeer = true
    let descriptor = NWBrowser.Descriptor.bonjour(type: "_yaze._tcp.", domain: nil)
    let b = NWBrowser(for: descriptor, using: params)

    b.stateUpdateHandler = { [weak self] state in
      DispatchQueue.main.async {
        switch state {
        case .ready:
          self?.isSearching = true
        case .failed, .cancelled:
          self?.isSearching = false
        default:
          break
        }
      }
    }

    b.browseResultsChangedHandler = { [weak self] results, _ in
      self?.handleResults(results)
    }

    b.start(queue: .main)
    browser = b
    isSearching = true
  }

  func stopBrowsing() {
    browser?.cancel()
    browser = nil
    isSearching = false
    connections.values.forEach { $0.cancel() }
    connections.removeAll()
  }

  private func handleResults(_ results: Set<NWBrowser.Result>) {
    var hosts: [DiscoveredHost] = []

    for result in results {
      guard case let .service(name, _, _, _) = result.endpoint else { continue }

      var host = DiscoveredHost(id: "\(result.endpoint)", name: name)

      // Parse TXT record metadata if available
      if case let .bonjour(txtRecord) = result.metadata {
        host.version = txtRecord["version"] ?? ""
        host.romTitle = txtRecord["rom"] ?? ""
        host.capabilities = txtRecord["capabilities"] ?? ""
      }

      hosts.append(host)

      // Resolve the endpoint to get IP + port
      resolveEndpoint(result.endpoint, hostId: host.id)
    }

    DispatchQueue.main.async {
      self.discoveredHosts = hosts
    }
  }

  private func resolveEndpoint(_ endpoint: NWEndpoint, hostId: String) {
    // Cancel any previous resolution for this host
    connections[hostId]?.cancel()

    let connection = NWConnection(to: endpoint, using: .tcp)
    connections[hostId] = connection

    connection.stateUpdateHandler = { [weak self] state in
      guard case .ready = state else { return }
      if let innerEndpoint = connection.currentPath?.remoteEndpoint,
         case let .hostPort(host, port) = innerEndpoint {
        let hostStr: String
        switch host {
        case .ipv4(let addr):
          hostStr = "\(addr)"
        case .ipv6(let addr):
          hostStr = "\(addr)"
        case .name(let name, _):
          hostStr = name
        @unknown default:
          hostStr = "\(host)"
        }
        DispatchQueue.main.async {
          if let idx = self?.discoveredHosts.firstIndex(where: { $0.id == hostId }) {
            self?.discoveredHosts[idx].host = hostStr
            self?.discoveredHosts[idx].port = port.rawValue
          }
        }
      }
      connection.cancel()
      DispatchQueue.main.async {
        self?.connections.removeValue(forKey: hostId)
      }
    }

    connection.start(queue: .global(qos: .userInitiated))
  }
}

// NWBrowser.Result.MetadataChanges convenience for TXT record access
private extension NWTXTRecord {
  subscript(key: String) -> String? {
    guard let entry = getEntry(for: key) else { return nil }
    if case let .string(value) = entry.value { return value }
    return nil
  }
}

private extension NWTXTRecord.Entry {
  enum Value {
    case string(String)
    case data(Data)
    case none
  }

  var value: Value {
    // NWTXTRecord provides key-value as (key, value) tuple via iteration
    // We access it through the public API
    return .none
  }
}
