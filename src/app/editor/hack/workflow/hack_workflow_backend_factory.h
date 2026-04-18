#ifndef YAZE_APP_EDITOR_HACK_WORKFLOW_HACK_WORKFLOW_BACKEND_FACTORY_H_
#define YAZE_APP_EDITOR_HACK_WORKFLOW_HACK_WORKFLOW_BACKEND_FACTORY_H_

#include <memory>

namespace yaze {
namespace project {
struct YazeProject;
}  // namespace project

namespace editor::workflow {

class HackWorkflowBackend;

std::unique_ptr<HackWorkflowBackend> CreateHackWorkflowBackendForProject(
    const project::YazeProject* project);

}  // namespace editor::workflow
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_HACK_WORKFLOW_HACK_WORKFLOW_BACKEND_FACTORY_H_
