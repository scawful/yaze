import Foundation

struct RemoteJob: Identifiable, Codable {
  enum Kind: String, Codable {
    case build
    case test
  }

  enum Status: String, Codable {
    case queued
    case running
    case succeeded
    case failed
  }

  var id: String
  var kind: Kind
  var status: Status
  var createdAt: Date
  var message: String
}

final class RemoteBuildStore: ObservableObject {
  @Published var jobs: [RemoteJob] = []
  @Published var lastError: String = ""
  @Published var lastStatus: String = ""

  func enqueueBuild(host: YazeSettings.AiHost, projectPath: String) {
    enqueue(host: host, endpoint: "/api/build", kind: .build, payload: [
      "project_path": projectPath
    ])
  }

  func enqueueTests(host: YazeSettings.AiHost, projectPath: String) {
    enqueue(host: host, endpoint: "/api/test", kind: .test, payload: [
      "project_path": projectPath
    ])
  }

  func refreshJobs(host: YazeSettings.AiHost) {
    guard let url = URL(string: host.baseUrl + "/api/jobs") else {
      lastError = "Invalid host URL"
      return
    }

    var request = URLRequest(url: url)
    attachAuthHeaders(&request, host: host)

    URLSession.shared.dataTask(with: request) { data, _, error in
      DispatchQueue.main.async {
        if let error = error {
          self.lastError = error.localizedDescription
          return
        }
        guard let data = data else {
          self.lastError = "No response"
          return
        }
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        if let decoded = try? decoder.decode([RemoteJob].self, from: data) {
          self.jobs = decoded
          self.lastStatus = "Jobs refreshed"
        } else {
          self.lastError = "Failed to decode jobs"
        }
      }
    }.resume()
  }

  private func enqueue(host: YazeSettings.AiHost, endpoint: String, kind: RemoteJob.Kind, payload: [String: String]) {
    guard let url = URL(string: host.baseUrl + endpoint) else {
      lastError = "Invalid host URL"
      return
    }
    var request = URLRequest(url: url)
    request.httpMethod = "POST"
    request.setValue("application/json", forHTTPHeaderField: "Content-Type")
    attachAuthHeaders(&request, host: host)

    let body = ["kind": kind.rawValue, "payload": payload] as [String: Any]
    request.httpBody = try? JSONSerialization.data(withJSONObject: body)

    URLSession.shared.dataTask(with: request) { data, _, error in
      DispatchQueue.main.async {
        if let error = error {
          self.lastError = error.localizedDescription
          return
        }
        guard let data = data else {
          self.lastError = "No response"
          return
        }
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        if let job = try? decoder.decode(RemoteJob.self, from: data) {
          self.jobs.insert(job, at: 0)
          self.lastStatus = "Queued \(kind.rawValue)"
        } else {
          self.lastError = "Failed to decode job response"
        }
      }
    }.resume()
  }

  private func attachAuthHeaders(_ request: inout URLRequest, host: YazeSettings.AiHost) {
    guard !host.credentialId.isEmpty else { return }
    if let token = try? KeychainStore.load(key: host.credentialId),
       !token.isEmpty {
      request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
      request.setValue(token, forHTTPHeaderField: "X-API-Key")
    }
  }
}
