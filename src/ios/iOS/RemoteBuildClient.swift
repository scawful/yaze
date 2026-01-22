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

    URLSession.shared.dataTask(with: url) { data, _, error in
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
}
