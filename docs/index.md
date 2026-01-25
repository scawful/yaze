# Documentation Relocated

The published documentation now lives under [`docs/public`](public/index.md) so that Doxygen can
focus on the curated guides and references. All planning notes, AI/agent workflows, and research
documents have been moved to the repository-level [`docs/internal/`](../docs/internal/README.md).

Update your bookmarks:
- Public site entry point: [`docs/public/index.md`](public/index.md)
- Internal docs: [`docs/internal/README.md`](../docs/internal/README.md)

### Integration Note: Mesen2-OoS

Yaze integrates with the custom [Mesen2-OoS](../../../oracle-of-secrets/Docs/Tooling/Mesen2_Architecture.md) fork via the **Socket Client**.
-   **Source**: `src/app/emu/mesen/mesen_socket_client.cc`
-   **UI**: `MesenDebugPanel` in the Editor.
-   **Protocol**: JSON over Unix Domain Socket.

See the Oracle of Secrets documentation for the full API specification.
