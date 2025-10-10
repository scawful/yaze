# Future Improvements & Long-Term Vision

**Last Updated:** October 10, 2025  
**Status:** Living Document

This document outlines potential improvements, experimental features, and long-term vision for yaze. Items here are aspirational and may or may not be implemented depending on community needs, technical feasibility, and development resources.

---

## Architecture & Performance

### Emulator Core Improvements
See `docs/E6-emulator-improvements.md` for detailed emulator improvement roadmap.

**Priority Items:**
- **APU Timing Fix**: Cycle-accurate SPC700 execution for reliable music playback
- **CPU Cycle Accuracy**: Variable instruction timing for better game compatibility
- **PPU Scanline Renderer**: Replace pixel-based renderer for 20%+ performance boost
- **Audio Buffering**: Lock-free ring buffer to eliminate stuttering

### Plugin Architecture (v0.5.x+)
Enable community extensions and custom tools.

**Features:**
- C API for plugin development
- Hot-reload capability for rapid iteration
- Plugin registry and discovery system
- Example plugins (custom exporters, automation tools)

**Benefits:**
- Community can extend without core changes
- Experimentation without bloating core
- Custom workflow tools per project needs

### Multi-Threading Improvements
Parallelize heavy operations for better performance.

**Opportunities:**
- Background ROM loading
- Parallel graphics decompression
- Asynchronous file I/O
- Worker thread pool for batch operations

---

## Graphics & Rendering

### Advanced Graphics Editing
Full graphics sheet import/export workflow.

**Features:**
- Import modified PNG graphics sheets
- Automatic palette extraction and optimization
- Tile deduplication and compression
- Preview impact on ROM size

**Use Cases:**
- Complete graphics overhauls
- HD texture packs (with downscaling)
- Art asset pipelines

### Alternative Rendering Backends
Support beyond SDL3 for specialized use cases.

**Potential Backends:**
- **OpenGL**: Maximum compatibility, explicit control
- **Vulkan**: High-performance, low-overhead (Linux/Windows)
- **Metal**: Native macOS/iOS performance
- **WebGPU**: Browser-based editor

**Benefits:**
- Platform-specific optimization
- Testing without hardware dependencies
- Future-proofing for new platforms

### High-DPI / 4K Support
Perfect rendering on modern displays.

**Improvements:**
- Retina/4K-aware canvas rendering
- Scalable UI elements
- Crisp text at any zoom level
- Per-monitor DPI awareness

---

## AI & Automation

### Multi-Modal AI Input
Enhance `z3ed` with visual understanding.

**Features:**
- Screenshot → context for AI
- "Fix this room" with image reference
- Visual diff analysis
- Automatic sprite positioning from mockups

### Collaborative AI Sessions
Shared AI context in multiplayer editing.

**Features:**
- Shared AI conversation history
- AI-suggested edits visible to all users
- Collaborative problem-solving
- Role-based AI permissions

### Automation & Scripting
Python/Lua scripting for batch operations.

**Use Cases:**
- Batch room modifications
- Automated testing scripts
- Custom validation rules
- Import/export pipelines

---

## Content Editors

### Music Editor UI
Visual interface for sound and music editing.

**Features:**
- Visual SPC700 music track editor
- Sound effect browser and editor
- Import custom SPC files
- Live preview while editing

### Dialogue Editor
Comprehensive text editing system.

**Features:**
- Visual dialogue tree editor
- Text search across all dialogues
- Translation workflow support
- Character count warnings
- Preview in-game font rendering

### Event Editor
Visual scripting for game events.

**Features:**
- Node-based event editor
- Trigger condition builder
- Preview event flow
- Debug event sequences

### Hex Editor Enhancements
Power-user tool for low-level editing.

**Features:**
- Structure definitions (parse ROM data types)
- Search by data pattern
- Diff view between ROM versions
- Bookmark system for addresses
- Disassembly view integration

---

## Collaboration & Networking

### Real-Time Collaboration Improvements
Enhanced multi-user editing.

**Features:**
- Conflict resolution strategies
- User presence indicators (cursor position)
- Chat integration
- Permission system (read-only, edit, admin)
- Rollback/version control

### Cloud ROM Storage
Optional cloud backup and sync.

**Features:**
- Encrypted cloud storage
- Automatic backups
- Cross-device sync
- Shared project workspaces
- Version history

---

## Platform Support

### Web Assembly Build
Browser-based yaze editor.

**Benefits:**
- No installation required
- Cross-platform by default
- Shareable projects via URL
- Integrated with cloud storage

**Challenges:**
- File system access limitations
- Performance considerations
- WebGPU renderer requirement

### Mobile Support (iOS/Android)
Touch-optimized editor for tablets.

**Features:**
- Touch-friendly UI
- Stylus support
- Cloud sync with desktop
- Read-only preview mode for phones

**Use Cases:**
- Tablet editing on the go
- Reference/preview on phone
- Show ROM to players on mobile

---

## Quality of Life

### Undo/Redo System Enhancement
More granular and reliable undo.

**Improvements:**
- Per-editor undo stacks
- Undo history viewer
- Branching undo (tree structure)
- Persistent undo across sessions

### Project Templates
Quick-start templates for common ROM hacks.

**Templates:**
- Vanilla+ (minimal changes)
- Graphics overhaul
- Randomizer base
- Custom story framework

### Asset Library
Shared library of community assets.

**Features:**
- Import community sprites/graphics
- Share custom rooms/dungeons
- Tag-based search
- Rating and comments
- License tracking

### Accessibility
Make yaze usable by everyone.

**Features:**
- Screen reader support
- Keyboard-only navigation
- Colorblind-friendly palettes
- High-contrast themes
- Adjustable font sizes

---

## Testing & Quality

### Automated Regression Testing
Catch bugs before they ship.

**Features:**
- Automated UI testing framework
- Visual regression tests (screenshot diffs)
- Performance regression detection
- Automated ROM patching tests

### ROM Validation
Ensure ROM hacks are valid.

**Features:**
- Detect common errors (invalid pointers, etc.)
- Warn about compatibility issues
- Suggest fixes for problems
- Export validation report

### Continuous Integration Enhancements
Better CI/CD pipeline.

**Improvements:**
- Build artifacts for every commit
- Automated performance benchmarks
- Coverage reports
- Security scanning

---

## Documentation & Community

### API Documentation Generator
Auto-generated API docs from code.

**Features:**
- Doxygen → web docs pipeline
- Example code snippets
- Interactive API explorer
- Versioned documentation

### Video Tutorial System
In-app video tutorials.

**Features:**
- Embedded tutorial videos
- Step-by-step guided walkthroughs
- Context-sensitive help
- Community-contributed tutorials

### ROM Hacking Wiki Integration
Link editor to wiki documentation.

**Features:**
- Context-sensitive wiki links
- Inline documentation for ROM structures
- Community knowledge base
- Translation support

---

## Experimental / Research

### Machine Learning Integration
AI-assisted ROM hacking.

**Possibilities:**
- Auto-generate room layouts
- Suggest difficulty curves
- Detect similar room patterns
- Generate sprite variations

### VR/AR Visualization
Visualize SNES data in 3D.

**Use Cases:**
- 3D preview of overworld
- Virtual dungeon walkthrough
- Spatial room editing

### Symbolic Execution
Advanced debugging technique.

**Features:**
- Explore all code paths
- Find unreachable code
- Detect potential bugs
- Generate test cases

---

## Implementation Priority

These improvements are **not scheduled** and exist here as ideas for future development. Priority will be determined by:

1. **Community demand** - What users actually need
2. **Technical feasibility** - What's possible with current architecture
3. **Development resources** - Available time and expertise
4. **Strategic fit** - Alignment with project vision

---

## Contributing Ideas

Have an idea for a future improvement? 

- Open a GitHub Discussion in the "Ideas" category
- Describe the problem it solves
- Outline potential implementation approach
- Consider technical challenges

The best ideas are:
- **Specific**: Clear problem statement
- **Valuable**: Solves real user pain points
- **Feasible**: Realistic implementation
- **Scoped**: Can be broken into phases

---

**Note:** This is a living document. Ideas may be promoted to the active roadmap (`I1-roadmap.md`) or removed as project priorities evolve.

