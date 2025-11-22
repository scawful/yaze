# GitHub Actions Remote Workflow Documentation

This document describes how to trigger GitHub Actions workflows remotely, specifically focusing on the `ci.yml` workflow and its custom inputs.

## Triggering `ci.yml` Remotely

The `ci.yml` workflow can be triggered manually via the GitHub UI or programmatically using the GitHub API (or `gh` CLI) thanks to the `workflow_dispatch` event.

### Inputs

The `workflow_dispatch` event for `ci.yml` supports the following custom inputs:

- **`build_type`**:
  - **Description**: Specifies the CMake build type.
  - **Type**: `choice`
  - **Options**: `Debug`, `Release`, `RelWithDebInfo`
  - **Default**: `RelWithDebInfo`

- **`run_sanitizers`**:
  - **Description**: A boolean flag to enable or disable memory sanitizer runs.
  - **Type**: `boolean`
  - **Default**: `false`

- **`upload_artifacts`**:
  - **Description**: A boolean flag to enable or disable uploading build artifacts.
  - **Type**: `boolean`
  - **Default**: `false`

- **`enable_http_api_tests`**:
  - **Description**: **(NEW)** A boolean flag to enable or disable an additional step that runs HTTP API tests after the build. When set to `true`, a script (`scripts/agents/test-http-api.sh`) will be executed to validate the HTTP server (checking if the port is up and the health endpoint responds).
  - **Type**: `boolean`
  - **Default**: `false`

### Example Usage (GitHub CLI)

To trigger the `ci.yml` workflow with custom inputs using the `gh` CLI:

```bash
gh workflow run ci.yml -f build_type=Release -f enable_http_api_tests=true
```

This command will:
- Trigger the `ci.yml` workflow.
- Set the `build_type` to `Release`.
- Enable the HTTP API tests.