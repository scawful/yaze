import SwiftUI
import UniformTypeIdentifiers

struct DocumentPicker: UIViewControllerRepresentable {
  let contentTypes: [UTType]
  /// When true, the picker will import (copy) the selected document into the
  /// app sandbox. When false, it will open the document in place (preferred for
  /// iCloud Drive workflows, including `.yazeproj` bundles).
  var asCopy: Bool = true
  let onPick: (URL) -> Void

  func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
    let controller = UIDocumentPickerViewController(
      forOpeningContentTypes: contentTypes,
      asCopy: asCopy
    )
    controller.allowsMultipleSelection = false
    controller.delegate = context.coordinator
    return controller
  }

  func updateUIViewController(_ uiViewController: UIDocumentPickerViewController, context: Context) {}

  func makeCoordinator() -> Coordinator {
    Coordinator(onPick: onPick)
  }

  final class Coordinator: NSObject, UIDocumentPickerDelegate {
    let onPick: (URL) -> Void

    init(onPick: @escaping (URL) -> Void) {
      self.onPick = onPick
    }

    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
      guard let url = urls.first else { return }
      onPick(url)
    }
  }
}

struct ExportPicker: UIViewControllerRepresentable {
  let url: URL

  func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
    UIDocumentPickerViewController(forExporting: [url])
  }

  func updateUIViewController(_ uiViewController: UIDocumentPickerViewController, context: Context) {}
}
