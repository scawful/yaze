# LC_LZ2 Compression

The compression algorithm has multiple implementations with varying levels of quality, based primarily on the implementations made in skarsnik/sneshacking, Zarby89/ZScreamDungeon and ZCompress with optimizations made for C++.

Currently, the Compress and Uncompress methods from Hyrule Magic are used and all other compression methods are considered deprecated.

## Key Definitions

### Constants and Macros:
- `BUILD_HEADER(command, length)`: Macro to build a header from a command and a length.
- Command Constants: Constants to represent different commands like `kCommandDirectCopy`, `kCommandByteFill`, etc.
- Length and Mode Constants: Such as `kMaxLengthNormalHeader`, `kNintendoMode1`, etc.

### Data Structures:

#### 1. CompressionCommand:
   - **arguments**: 2D array representing the command arguments for each possible command.
   - **cmd_size**: Array storing the size of each possible command.
   - **data_size**: Array storing the size of the data processed by each possible command.

#### 2. CompressionPiece:
   - **command**: Represents the compression command.
   - **length**: Length of the compressed data piece.
   - **argument_length**: Length of the argument.
   - **argument**: Argument as a string.
   - **next**: Pointer to the next compression piece.

#### 3. CompressionContext (for Compression V3):
   - Contains vectors to store raw and compressed data, compression pieces, and compression string.
   - Various counters and flags for compression control.
   - Current compression command details.

## Compression Functions

### Version 1:
- **Byte Repeat**: `CheckByteRepeat`
- **Word Repeat**: `CheckWordRepeat`
- **Increasing Byte**: `CheckIncByte`
- **Intra Copy**: `CheckIntraCopy`
- **Validation and Alternatives**: `ValidateForByteGain` & `CompressionCommandAlternative`

### Version 2:
- **Byte Repeat**: `CheckByteRepeatV2`
- **Word Repeat**: `CheckWordRepeatV2`
- **Increasing Byte**: `CheckIncByteV2`
- **Intra Copy**: `CheckIntraCopyV2`
- **Validation and Alternatives**: `ValidateForByteGainV2` & `CompressionCommandAlternativeV2`

### Version 3:
Using `CompressionContext` to handle compression.
- **Initialization**: `InitializeCompression`
- **Command Checks**: Such as `CheckByteRepeatV3`
- **Determining Best Compression**: `DetermineBestCompression`
- **Handling Direct Copy**: `HandleDirectCopy`
- **Adding Compression to Chain**: `AddCompressionToChain`

## Decompression Functions:
- `SetBuffer`: Prepares a buffer from data.
- `memfill`: Fills memory.
- **Decompression**: Such as `DecompressV2`, `DecompressGraphics`, and `DecompressOverworld`.

## Utility Functions:
- **Printing**: Such as `PrintCompressionPiece` and `PrintCompressionChain`.
- **Compression String Creation**: `CreateCompressionString`
- **Compression Result Validation**: Such as `ValidateCompressionResult` and its V3 variant.
- **Compression Piece Manipulation**: Like `SplitCompressionPiece` and its V3 variant.
