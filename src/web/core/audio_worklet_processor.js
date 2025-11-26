/**
 * SNES Audio Processor - AudioWorklet implementation for low-latency audio
 *
 * This processor runs on a separate audio thread, providing better performance
 * than the deprecated ScriptProcessorNode. It uses a ring buffer for lock-free
 * communication with the main thread.
 */

class SNESAudioProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();

    // Ring buffer configuration
    this.bufferSize = options.processorOptions?.bufferSize || 8192;
    this.channels = options.processorOptions?.channels || 2;

    // Ring buffer for lock-free sample storage
    this.ringBuffer = new Float32Array(this.bufferSize * this.channels);
    this.writeIndex = 0;
    this.readIndex = 0;
    this.samplesAvailable = 0;

    // Volume control (0.0 to 1.0)
    this.volume = 1.0;

    // Statistics for debugging
    this.underruns = 0;
    this.totalFramesProcessed = 0;

    // Handle messages from main thread
    this.port.onmessage = (event) => {
      this.handleMessage(event.data);
    };

    // Notify main thread we're ready
    this.port.postMessage({ type: 'ready' });
  }

  handleMessage(data) {
    switch (data.type) {
      case 'samples':
        // Receive samples from main thread
        this.enqueueSamples(data.samples, data.frameCount);
        break;

      case 'volume':
        this.volume = Math.max(0, Math.min(1, data.value));
        break;

      case 'clear':
        this.writeIndex = 0;
        this.readIndex = 0;
        this.samplesAvailable = 0;
        break;

      case 'getStatus':
        this.port.postMessage({
          type: 'status',
          samplesAvailable: this.samplesAvailable,
          underruns: this.underruns,
          totalFrames: this.totalFramesProcessed
        });
        break;
    }
  }

  enqueueSamples(samples, frameCount) {
    const totalSamples = frameCount * this.channels;
    const bufferLength = this.ringBuffer.length;

    // Check if we have space
    const freeSpace = bufferLength - this.samplesAvailable;
    if (totalSamples > freeSpace) {
      // Buffer overflow - drop oldest samples to make room
      const overflow = totalSamples - freeSpace;
      this.readIndex = (this.readIndex + overflow) % bufferLength;
      this.samplesAvailable -= overflow;
    }

    // Copy samples into ring buffer
    for (let i = 0; i < totalSamples; i++) {
      this.ringBuffer[this.writeIndex] = samples[i];
      this.writeIndex = (this.writeIndex + 1) % bufferLength;
    }
    this.samplesAvailable += totalSamples;
  }

  process(inputs, outputs, parameters) {
    const output = outputs[0];
    if (!output || output.length === 0) {
      return true;
    }

    const numChannels = Math.min(output.length, this.channels);
    const frameCount = output[0].length;
    const samplesNeeded = frameCount * this.channels;

    this.totalFramesProcessed += frameCount;

    if (this.samplesAvailable < samplesNeeded) {
      // Underrun - fill with silence and report
      this.underruns++;
      for (let ch = 0; ch < numChannels; ch++) {
        output[ch].fill(0);
      }

      // Use whatever samples we do have
      const availableFrames = Math.floor(this.samplesAvailable / this.channels);
      for (let frame = 0; frame < availableFrames; frame++) {
        for (let ch = 0; ch < numChannels; ch++) {
          const sample = this.ringBuffer[this.readIndex] * this.volume;
          output[ch][frame] = sample;
          this.readIndex = (this.readIndex + 1) % this.ringBuffer.length;
        }
      }
      this.samplesAvailable = 0;

      return true;
    }

    // Read samples from ring buffer and deinterleave to output channels
    for (let frame = 0; frame < frameCount; frame++) {
      for (let ch = 0; ch < numChannels; ch++) {
        const sample = this.ringBuffer[this.readIndex] * this.volume;
        output[ch][frame] = sample;
        this.readIndex = (this.readIndex + 1) % this.ringBuffer.length;
      }
    }

    this.samplesAvailable -= samplesNeeded;

    // Keep processor alive
    return true;
  }
}

// Register the processor
registerProcessor('snes-audio-processor', SNESAudioProcessor);
