#ifndef YAZE_SRC_CLI_SERVICE_RESOURCE_CATALOG_H_
#define YAZE_SRC_CLI_SERVICE_RESOURCE_CATALOG_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace yaze {
namespace cli {

struct ResourceArgument {
  std::string flag;
  std::string type;
  bool required;
  std::string description;
};

struct ResourceAction {
  std::string name;
  std::string synopsis;
  std::string stability;
  std::vector<ResourceArgument> arguments;
  std::vector<std::string> effects;
  struct ReturnValue {
    std::string field;
    std::string type;
    std::string description;
  };
  std::vector<ReturnValue> returns;
};

struct ResourceSchema {
  std::string resource;
  std::string description;
  std::vector<ResourceAction> actions;
};

// ResourceCatalog exposes a machine-readable description of CLI resources so that
// both humans and AI agents can introspect capabilities at runtime.
class ResourceCatalog {
 public:
  static const ResourceCatalog& Instance();

  absl::StatusOr<ResourceSchema> GetResource(absl::string_view name) const;
  const std::vector<ResourceSchema>& AllResources() const;

  // Serialize helpers for `z3ed agent describe`. These return compact JSON
  // strings so we avoid introducing a hard dependency on nlohmann::json.
  std::string SerializeResource(const ResourceSchema& schema) const;
  std::string SerializeResources(
      const std::vector<ResourceSchema>& schemas) const;
  std::string SerializeResourcesAsYaml(
      const std::vector<ResourceSchema>& schemas, absl::string_view version,
      absl::string_view last_updated) const;

 private:
  ResourceCatalog();

  static std::string EscapeJson(absl::string_view value);
  static std::string EscapeYaml(absl::string_view value);

  std::vector<ResourceSchema> resources_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_RESOURCE_CATALOG_H_
