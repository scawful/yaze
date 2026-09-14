# Platform compatibility and release evidence

Last reviewed: 2026-09-14 for the v0.8.0 development line.

Yaze targets macOS, Windows, and Linux. The WebAssembly build is a browser
preview. A successful compiler build is necessary but does not, by itself,
prove that a package is ready for testers.

## Evidence levels

| Level | Meaning |
| --- | --- |
| Source/config | The platform path and build configuration exist and can be reviewed. |
| Build | The platform compiler produces the requested targets. |
| Test | Discovered unit/integration suites run; zero selected tests is a failure. |
| Package | The delivered archive/installer has the expected layout, assets, dependencies, version, and Git provenance. |
| Hands-on | A person launches the packaged app from its delivered location and completes the named workflow. |

Release notes must state the highest level actually completed for each artifact.

## Desktop target matrix

| Platform | Intended tester artifact | Automated release contract | Remaining manual or distribution limit |
| --- | --- | --- | --- |
| macOS 14+ | DMG containing `yaze.app` and packaged `z3ed` | Relocate the app out of the DMG; verify bundle assets, runtime dependencies, signature integrity, exact version/provenance, and CLI self-test from a neutral directory. | Open the relocated app and verify welcome screen, theme/font assets, ROM picker, one supported editor, and clean quit. CI may use ad-hoc signing; notarization and universal-binary support require separate proof. |
| Windows 11 / Server 2022 x64 | ZIP and NSIS installer | Verify app-local runtime libraries, assets, exact version/provenance, ZIP execution, isolated install, registry entry, installed execution, uninstall, and cleanup. | Open the installed GUI and exercise a supported editor workflow. Artifacts are not yet Authenticode-signed. |
| Ubuntu 22.04 x86_64 | TGZ and DEB | Relocate the TGZ to a neutral directory; verify loader dependencies, assets, exact version/provenance, CLI self-test, guarded DEB install, PATH execution, and purge. | Open the packaged GUI under a real desktop session and exercise a supported editor workflow. Other distributions remain source-build or community-test targets. |

Do not infer support for another OS version, CPU architecture, Linux
distribution, display server, or signing mode from these rows. Record additional
targets only after testing their exact artifact.

## Web / WASM preview

The WASM gate must produce the packaged browser files and pass a serial Chromium
smoke test. That proves browser startup and the debug API subset. It does not
prove native-editor parity, durable browser storage under every policy, or the
desktop emulator.

Use the [Web App guide](../usage/web-app.md) for current limitations.

## Standard local build entry points

Use the repository presets instead of copying platform-specific dependency
lists into this page:

```bash
# macOS development build used by maintainers
cmake --preset mac-ai
cmake --build --preset mac-ai --parallel 4

# Inspect all current configure/build/test presets
cmake --list-presets
cmake --build --list-presets
ctest --list-presets
```

For Windows and Linux, select the current preset documented in the
[Build and Test Quick Reference](quick-reference.md). Presets and CI setup are
the source of truth for dependencies and compiler flags.

## Release-candidate gate

Before calling a commit tester-ready:

1. Confirm the exact Git SHA used by every hosted job.
2. Require native build/test, security, WASM, and a `publish=false` Release
   workflow to finish successfully on that SHA.
3. Download the produced artifacts; do not substitute a local build for package
   evidence.
4. Complete and record the hands-on checks in the
   [release checklist](../../internal/release-checklist.md).
5. Test editor persistence only in the lanes listed in the
   [editor readiness matrix](../reference/feature-coverage-report.md).

## Current distribution limits

- Windows code signing is not configured for release artifacts.
- macOS notarization and a universal-binary claim are not automatic.
- Linux desktop behavior varies beyond the tested Ubuntu package environment.
- The browser build has no emulator and remains a preview.
- Cross-platform package smoke does not replace GUI interaction testing.

Report platform, package type, exact Yaze version/commit, and whether a failure
occurred at install, launch, ROM open, edit, save, reopen, or quit.
