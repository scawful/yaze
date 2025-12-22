/**
 * @fileoverview Async operation queue for WASM Asyncify
 *
 * Emscripten's Asyncify only supports one async operation at a time.
 * This queue serializes all IndexedDB and other async operations to prevent
 * the "only one async operation in flight at a time" crash.
 *
 * Usage: In EM_JS functions, wrap async operations like:
 *   if (window.yazeAsyncQueue) {
 *     return window.yazeAsyncQueue.enqueue(asyncOperation);
 *   }
 *   return asyncOperation();
 */

window.yazeAsyncQueue = {
  queue: [],
  isRunning: false,
  operationCount: 0,
  errorCount: 0,

  /**
   * Enqueue an async operation to be executed serially
   * @param {Function} asyncFn - Function that returns a Promise
   * @returns {Promise} Promise that resolves with the operation result
   */
  enqueue: function(asyncFn) {
    var self = this;
    return new Promise(function(resolve, reject) {
      self.queue.push({ fn: asyncFn, resolve: resolve, reject: reject });
      self.processNext();
    });
  },

  /**
   * Process the next operation in the queue
   * Only runs if no operation is currently in progress
   */
  processNext: function() {
    var self = this;

    if (self.isRunning || self.queue.length === 0) {
      return;
    }

    self.isRunning = true;
    self.operationCount++;

    var item = self.queue.shift();
    var startTime = performance.now();

    // Execute the async operation
    Promise.resolve()
      .then(function() {
        return item.fn();
      })
      .then(function(result) {
        var duration = performance.now() - startTime;
        if (duration > 100) {
          console.log('[AsyncQueue] Operation completed in ' + duration.toFixed(1) + 'ms');
        }
        item.resolve(result);
      })
      .catch(function(error) {
        self.errorCount++;
        console.error('[AsyncQueue] Operation failed:', error);
        item.reject(error);
      })
      .finally(function() {
        self.isRunning = false;
        // Process next item in queue
        self.processNext();
      });
  },

  /**
   * Check if an async operation is currently in progress
   * @returns {boolean}
   */
  isOperationInProgress: function() {
    return this.isRunning;
  },

  /**
   * Get queue statistics
   * @returns {Object} Statistics about the queue
   */
  getStats: function() {
    return {
      queueLength: this.queue.length,
      isRunning: this.isRunning,
      totalOperations: this.operationCount,
      errorCount: this.errorCount
    };
  },

  /**
   * Clear the pending queue (doesn't affect currently running operation)
   */
  clear: function() {
    var cleared = this.queue.length;
    this.queue.forEach(function(item) {
      item.reject(new Error('Queue cleared'));
    });
    this.queue = [];
    return cleared;
  }
};

// Make available globally for debugging
if (typeof window !== 'undefined') {
  window.getAsyncQueueStats = function() {
    return window.yazeAsyncQueue.getStats();
  };
}

console.log('[AsyncQueue] Initialized');
