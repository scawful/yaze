import Foundation

private final class MockDesktopProtocol: URLProtocol {
  override class func canInit(with request: URLRequest) -> Bool { true }
  override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }
  private var work: DispatchWorkItem?
  override func startLoading() {
    let slow = request.url?.host == "slow.test"
    let item = DispatchWorkItem { [weak self] in
      guard let self else { return }
      let status = self.request.url?.host == "failed.test" ? 503 : 200
      let response = HTTPURLResponse(url: self.request.url!, statusCode: status,
                                     httpVersion: nil, headerFields: nil)!
      self.client?.urlProtocol(self, didReceive: response, cacheStoragePolicy: .notAllowed)
      self.client?.urlProtocol(self, didLoad: Data(
        "{\"status\":\"ok\",\"version\":\"test\",\"service\":\"yaze\"}".utf8))
      self.client?.urlProtocolDidFinishLoading(self)
    }
    work = item
    DispatchQueue.main.asyncAfter(deadline: .now() + (slow ? 0.15 : 0.01), execute: item)
  }
  override func stopLoading() { work?.cancel() }
}

@main
struct ConnectionHarness {
  @MainActor static func main() async throws {
    let config = URLSessionConfiguration.ephemeral
    config.protocolClasses = [MockDesktopProtocol.self]
    let api = DesktopAPIClient(session: URLSession(configuration: config))
    api.connectManual(host: "slow.test", port: 8080)
    precondition(api.isConnecting && !api.isConnected)
    try await Task.sleep(nanoseconds: 20_000_000)
    api.disconnect()
    try await Task.sleep(nanoseconds: 250_000_000)
    precondition(!api.isConnected && !api.isConnecting && api.connectedHost == nil)
    precondition(api.lastError.isEmpty)

    api.connectManual(host: "slow.test", port: 8080)
    try await Task.sleep(nanoseconds: 20_000_000)
    api.connectManual(host: "fast.test", port: 8080)
    try await Task.sleep(nanoseconds: 250_000_000)
    precondition(api.isConnected && !api.isConnecting)
    precondition(api.connectedHost?.host == "fast.test")
    precondition(api.lastError.isEmpty)
    api.disconnect()
    precondition(!api.isConnected && api.connectedHost == nil)
    api.connectManual(host: "failed.test", port: 8080)
    try await Task.sleep(nanoseconds: 250_000_000)
    precondition(!api.isConnected && !api.isConnecting && !api.lastError.isEmpty)
    api.connectManual(host: "fast.test", port: 8080)
    try await Task.sleep(nanoseconds: 250_000_000)
    precondition(api.isConnected && !api.isConnecting && api.lastError.isEmpty)
    api.disconnect()
    print("PASS: pending connection, disconnect, superseded connection, successful connection, final disconnect, failed health check, retry")
  }
}
