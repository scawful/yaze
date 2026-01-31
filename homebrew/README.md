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
curl -sL https://github.com/scawful/yaze/archive/refs/tags/v0.5.5.tar.gz | shasum -a 256
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
