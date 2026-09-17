# Homebrew Tap for YAZE

Homebrew formulae for [YAZE](https://github.com/scawful/yaze) and z3ed.

## Setup

To use these formulae, you'll need to create a separate tap repository (`homebrew-yaze`) and copy the `Formula/` directory there.

### Quick Start

```bash
# Add the tap (once the tap repo exists)
brew tap scawful/yaze

# Install the GUI editor
brew install yaze

# Install the CLI tool only
brew install z3ed
```

### Creating the Tap Repository

1. Create a new GitHub repo named `scawful/homebrew-yaze`
2. Copy `Formula/` to the repo root
3. Update the `sha256` values after each release:

```bash
# Get SHA256 for a release tarball
curl -sL https://github.com/scawful/yaze/archive/refs/tags/v0.6.0.tar.gz | shasum -a 256
```

### Manual Install (without tap)

```bash
brew install --formula homebrew/Formula/yaze.rb
```

## Available Formulae

| Formula | Description |
|---------|-------------|
| `yaze` | Full GUI editor (SDL2 + ImGui) |
| `z3ed` | CLI-only ROM hacking tool |

## Open follow-up: formulae are stale (raised 2026-09-16)

Both formulae still pin `v0.5.6` (`url` and `sha256`) while the repository `VERSION`
is `0.8.0`, and nothing in this repository references them — the tap described above
has to be created by hand. They were deliberately left untouched during the
2026-09-16 maintenance-surface cleanup because updating them is a release-packaging
decision, not a documentation one.

Resolving this needs a separate slice that decides between:

1. **Refresh** — bump `url`/`sha256` to the current tag, re-check the `depends_on`
   lists against the present CMake options, build both formulae locally, and create
   the tap repository.
2. **Retire** — delete `homebrew/` and point macOS users at the release DMG and
   `scripts/install-nightly.sh` instead.

Until then, treat `homebrew/Formula/*.rb` as unmaintained. The Homebrew section of
[docs/public/build/install-options.md](../docs/public/build/install-options.md)
should be revisited in the same slice. Cross-referenced from
[scripts/README.md](../scripts/README.md).
