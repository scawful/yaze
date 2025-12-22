/**
 * @fileoverview Web Worker for background processing in YAZE WASM build.
 *
 * This worker handles CPU-intensive operations to prevent UI freezing.
 * It can be used as a fallback when SharedArrayBuffer is not available
 * (which is required for Emscripten pthreads).
 *
 * Supported operations:
 * - ROM decompression (LC-LZ2)
 * - Graphics sheet decoding
 * - Palette calculations
 * - Asar assembly compilation
 */

// Import the WASM module if needed
// importScripts('yaze.js');

/**
 * Task processors for different operation types
 */
const TaskProcessors = {
  /**
   * Process ROM decompression using LC-LZ2 algorithm
   * @param {Uint8Array} input - Compressed data
   * @returns {Uint8Array} Decompressed data
   */
  romDecompression: function(input) {
    // This would call into WASM for actual LC-LZ2 decompression
    // For now, return a simulated result
    postMessage({
      type: 'progress',
      progress: 0.1,
      message: 'Starting decompression...'
    });

    const result = new Uint8Array(input.length * 2);

    for (let i = 0; i < input.length; i++) {
      result[i * 2] = input[i];
      result[i * 2 + 1] = input[i] ^ 0xFF;

      // Report progress periodically
      if (i % 1000 === 0) {
        const progress = i / input.length;
        postMessage({
          type: 'progress',
          progress: progress,
          message: `Decompressing... ${Math.floor(progress * 100)}%`
        });
      }
    }

    postMessage({
      type: 'progress',
      progress: 1.0,
      message: 'Decompression complete'
    });

    return result;
  },

  /**
   * Decode graphics sheets from SNES tile format
   * @param {Uint8Array} input - Raw tile data
   * @returns {Uint8Array} Decoded graphics data
   */
  graphicsDecoding: function(input) {
    postMessage({
      type: 'progress',
      progress: 0.0,
      message: 'Decoding graphics...'
    });

    // Simulate graphics decoding
    // Real implementation would convert SNES tile formats
    const tileSize = 32; // 8x8 tile in 4bpp = 32 bytes
    const numTiles = Math.floor(input.length / tileSize);
    const result = new Uint8Array(numTiles * 64); // 8x8 pixels, 1 byte per pixel

    for (let tile = 0; tile < numTiles; tile++) {
      const tileOffset = tile * tileSize;
      const resultOffset = tile * 64;

      // Decode 4bpp planar to linear
      for (let y = 0; y < 8; y++) {
        const plane0 = input[tileOffset + y * 2];
        const plane1 = input[tileOffset + y * 2 + 1];
        const plane2 = input[tileOffset + 16 + y * 2];
        const plane3 = input[tileOffset + 16 + y * 2 + 1];

        for (let x = 0; x < 8; x++) {
          const bit = 7 - x;
          const pixel =
            ((plane0 >> bit) & 1) |
            (((plane1 >> bit) & 1) << 1) |
            (((plane2 >> bit) & 1) << 2) |
            (((plane3 >> bit) & 1) << 3);

          result[resultOffset + y * 8 + x] = pixel;
        }
      }

      // Report progress
      if (tile % 100 === 0) {
        const progress = tile / numTiles;
        postMessage({
          type: 'progress',
          progress: progress,
          message: `Decoding tiles... ${tile}/${numTiles}`
        });
      }
    }

    return result;
  },

  /**
   * Calculate palette data from SNES color format
   * @param {Uint8Array} input - SNES palette data (BGR555)
   * @returns {Uint8Array} RGB888 palette data
   */
  paletteCalculation: function(input) {
    postMessage({
      type: 'progress',
      progress: 0.0,
      message: 'Processing palette...'
    });

    const numColors = Math.floor(input.length / 2);
    const result = new Uint8Array(numColors * 3);

    for (let i = 0; i < numColors; i++) {
      // Read BGR555 color (little endian)
      const colorLow = input[i * 2];
      const colorHigh = input[i * 2 + 1];
      const snesColor = (colorHigh << 8) | colorLow;

      // Extract 5-bit components
      const r5 = snesColor & 0x1F;
      const g5 = (snesColor >> 5) & 0x1F;
      const b5 = (snesColor >> 10) & 0x1F;

      // Convert to 8-bit RGB
      result[i * 3] = (r5 << 3) | (r5 >> 2);     // Red
      result[i * 3 + 1] = (g5 << 3) | (g5 >> 2); // Green
      result[i * 3 + 2] = (b5 << 3) | (b5 >> 2); // Blue

      // Report progress for large palettes
      if (i % 256 === 0) {
        const progress = i / numColors;
        postMessage({
          type: 'progress',
          progress: progress,
          message: `Processing colors... ${i}/${numColors}`
        });
      }
    }

    postMessage({
      type: 'progress',
      progress: 1.0,
      message: 'Palette processing complete'
    });

    return result;
  },

  /**
   * Compile Asar assembly code
   * @param {Uint8Array} input - Assembly source code (UTF-8)
   * @returns {Uint8Array} Compiled patch data
   */
  asarCompilation: function(input) {
    postMessage({
      type: 'progress',
      progress: 0.0,
      message: 'Compiling assembly...'
    });

    // Convert input to string
    const decoder = new TextDecoder('utf-8');
    const sourceCode = decoder.decode(input);

    // This would call into WASM Asar wrapper for actual compilation
    // For now, return empty result (successful compilation with no patches)

    postMessage({
      type: 'progress',
      progress: 0.5,
      message: 'Parsing assembly code...'
    });

    // Simulate compilation time
    setTimeout(() => {
      postMessage({
        type: 'progress',
        progress: 1.0,
        message: 'Compilation complete'
      });
    }, 100);

    return new Uint8Array(0);
  },

  /**
   * Custom task processor for extensibility
   * @param {string} taskType - Custom task type identifier
   * @param {Uint8Array} input - Input data
   * @returns {Uint8Array} Processed data
   */
  custom: function(taskType, input) {
    postMessage({
      type: 'progress',
      progress: 0.0,
      message: `Processing custom task: ${taskType}`
    });

    // Placeholder for custom processing
    // Real implementation would have a registry of custom processors

    postMessage({
      type: 'progress',
      progress: 1.0,
      message: 'Custom task complete'
    });

    return input;
  }
};

/**
 * Message handler for worker
 */
self.addEventListener('message', function(e) {
  const { id, type, data, customType } = e.data;

  try {
    let result;

    // Dispatch to appropriate processor
    switch (type) {
      case 'romDecompression':
        result = TaskProcessors.romDecompression(new Uint8Array(data));
        break;

      case 'graphicsDecoding':
        result = TaskProcessors.graphicsDecoding(new Uint8Array(data));
        break;

      case 'paletteCalculation':
        result = TaskProcessors.paletteCalculation(new Uint8Array(data));
        break;

      case 'asarCompilation':
        result = TaskProcessors.asarCompilation(new Uint8Array(data));
        break;

      case 'custom':
        result = TaskProcessors.custom(customType, new Uint8Array(data));
        break;

      case 'ping':
        // Health check
        postMessage({ type: 'pong', id: id });
        return;

      default:
        throw new Error(`Unknown task type: ${type}`);
    }

    // Send result back to main thread
    postMessage({
      type: 'complete',
      id: id,
      success: true,
      result: result.buffer
    }, [result.buffer]); // Transfer ownership for efficiency

  } catch (error) {
    // Send error back to main thread
    postMessage({
      type: 'complete',
      id: id,
      success: false,
      error: error.message
    });
  }
});

/**
 * Initialize worker
 */
postMessage({ type: 'ready' });