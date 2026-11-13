# Editor and release status

## 0.8.0 release candidate

The clean release cut combines application baseline
`14c11882498549dfb5b069dea65e531587846a4f`, a nonthrowing project-integer parser
fix and the requested Dungeon Workbench cleanup. The object browser has List
and Icons views with shared filters and placement actions. Compact Selection,
a larger Room Matrix and the entrance list give the right side a navigation
focus. `VERSION` remains `0.8.0`; the package qualification uses `v0.8.0` with no
prerelease suffix and `publish=false`.

The release validation record binds the final source, package hashes and
checks. Source builds and focused UI tests do not establish packaged native
editing, Save/reopen or public release acceptance.

The earlier application-code baseline passed all Linux/macOS/Windows package and
Release-test jobs in [run 41496853533](https://github.com/scawful/yaze/actions/runs/41496853533),
which used the earlier exp.2 package suffix. That result is reused only at its
actual scope. The unsuffixed artifacts, exact metadata commit, package smoke,
and native editing/save/reopen checks are recorded separately before
publication. No public 0.8.0 tag or release has been created by this preparation.

This cut does not absorb unrelated private Lyra writer branches or treat
Dungeon completion and the 1.0 roadmap as 0.8.0 release prerequisites. The
[release checklist](release-checklist.md) and
[release notes](../public/release-notes.md#v080) retain the actual supported,
experimental and unchecked paths.

## Published v0.8.0-exp.1

Latest experimental prerelease: [v0.8.0-exp.1](https://github.com/scawful/yaze/releases/tag/v0.8.0-exp.1), published
2026-10-28 Eastern (2026-10-29 00:28 UTC). Latest stable release remains v0.7.2.
`VERSION` is `0.8.0`; it is not proof that a new package has been published.
Dungeon Editor completion remains separate roadmap work.

| Evidence | Exact result |
| --- | --- |
| Published source | `cc673c04f5dae90d17bdc25d0923d540522340c0` (`cc673c04`) |
| Release dry run | [40748726881](https://github.com/scawful/yaze/actions/runs/40748726881): all three platform build/test jobs PASS; publication skipped |
| Publication run | [40781419989](https://github.com/scawful/yaze/actions/runs/40781419989): all three platform build/test jobs and Create Release PASS |
| Artifacts | macOS DMG, Linux TGZ/DEB, Windows ZIP/installer, and [SHA-256 checksums](https://github.com/scawful/yaze/releases/download/v0.8.0-exp.1/checksums.txt) |
| HTTP API | `HTTP API Server: OFF` in all three publication build logs |
| Source CI | Candidate CI 40748775998 PASS; later [tag CI 40798319425](https://github.com/scawful/yaze/actions/runs/40798319425) FAILED in Memory Sanitizer Test; Windows/Linux/macOS test jobs passed |
| Manual/package limits | This audit adds no fresh GUI/ROM save-reopen or clean-machine result. Interactive Windows Save As [#107](https://github.com/scawful/yaze/issues/107), macOS signing/notarization/Gatekeeper and clean-machine portability are not closed by package build jobs |

The earlier temporary-lifetime sanitizer defect on `5d4b39ae0` was fixed before
publication. It does not explain or waive the later tag-job failure. Preserve
both results. Release scope and remaining qualification are in the
[release notes](../public/release-notes.md#v080) and
[release checklist](release-checklist.md).

The checksum file's own digest matches GitHub asset metadata, and its five
package digests match the published assets. One filename differs: the Debian
row names `yaze_0.8.0~exp.1_amd64.deb`, while the download is
`yaze_0.8.0.exp.1_amd64.deb`; both have SHA-256
`a3b2d0eb04ffb04faea001157ad2e98436683adeae2436e93d79fe7ac3a470a5`.
The source fix uses `yaze-<display-version>-Linux.deb` for future uploads and
keeps `~` in the Debian control version. Published files are unchanged.

## Development after the prerelease

The audited mainline baseline is `86c9f5989f78b5ad7a248d0ba79635d1d1b9b593`.
Its [CI 40939952551](https://github.com/scawful/yaze/actions/runs/40939952551)
has GCC-12/older-Abseil compilation failures in `sheet_image_import.cc` and
rejects new ad-hoc `image_import_panel` files. CodeQL stops at the same compiler
error. Those are development-source results, not failures of the published
package job. The release-hardening branch fixes the formatter argument and places
the import view under the existing graphics feature-view composition; it does
not loosen editor guardrails or add another window. Its exact-commit
qualification belongs to that branch's CI and does not retag or republish exp.1.

## Lyra source and qualification boundaries

The tail-map page range is `$A0–$BF` (32 pages). It is not a synonym for a region
or a guarantee of gameplay. Grouped gameplay areas are a separate model.

| Source | Document model | Qualification boundary |
| --- | --- | --- |
| Published v0.8.0-exp.1, `cc673c04` | Schema 3, 32 tail-map pages | Experimental project authoring; no later gameplay-area, transport or persistent-reward model |
| Mainline baseline `86c9f598` | Schema 5, 32 pages with gameplay areas; 192-map navigation | Later development source, not in exp.1 downloads; ordinary full-map Oracle Build/Test is not release-qualified |
| Private Lyra candidate `690eb9c4cb7ac7171cc88e03f7ba9df339621ff4` | Schema 10 with gameplay areas, boat/whirlpool references and persistent-reward authoring | Local experimental source; host/source checks do not grant Oracle allocation/runtime admission or prove native gameplay |

The coordinator's current copied Save input is schema 4. The approved 21-area
schema-5 import is prepared but still needs an owned normal editor Save and
fresh-process reopen, followed by its new genuine receipt/transfer. Never reuse
an older Save generation or replay binding after those edits. Private source
support does not imply that the current saved project contains those features.

## Historical candidate notes

The combined editor candidate brings together overworld/Tile16 changes from
`0f3855d6f` and dungeon authoring from `804b32eea`. The combined application build
and 1,303 focused automated tests passed; three optional asset tests skipped. Native editing and disposable-ROM acceptance
remain unchecked.

Use [the roadmap](roadmap.md), [the capability plan](plans/editor-capability-parity-plan.md),
and [the release checklist](release-checklist.md) for scope and acceptance.
Historical test counts are not evidence for the combined candidate.

See the [combined candidate handoff](agents/combined-editor-candidate-2026-09-23.md)
for evidence and the single manual testing queue.

## Room template files

The combined branch now adds native desktop room JSON file import/export.
The app/unit build and 128 focused room-document/transfer tests passed with no
skips. This is incremental evidence after the combined baseline above, not a
rerun of all 1,303 earlier tests. See the
[room file handoff](agents/dungeon-room-template-files-2026-09-23.md).

## Pit and staircase destinations

Shared Workbench/standalone destination controls now include pit arrival layer,
named plane choices, explicit stair header slots and destination navigation.
App/unit build and 264 focused tests passed with no skips. Runtime traversal and
placed-staircase mapping remain unqualified. See the
[destination handoff](agents/dungeon-destinations-2026-09-23.md).
