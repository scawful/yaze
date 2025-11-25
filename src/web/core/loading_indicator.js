/**
 * @fileoverview Loading indicator system for yaze WASM build
 *
 * Provides a browser-side loading UI with progress bars, status messages,
 * and cancellation support for long-running operations.
 */

(function() {
  'use strict';

  // Store active loading indicators
  const activeIndicators = new Map();

  /**
   * Create a new loading indicator
   * @param {number} id - Unique identifier for this loading operation
   * @param {string} taskName - Name of the task being performed
   */
  window.createLoadingIndicator = function(id, taskName) {
    // Remove any existing indicator with the same ID
    if (activeIndicators.has(id)) {
      removeLoadingIndicator(id);
    }

    // Create the loading overlay
    const overlay = document.createElement('div');
    overlay.className = 'yaze-loading-overlay';
    overlay.id = `yaze-loading-${id}`;

    // Create the loading container
    const container = document.createElement('div');
    container.className = 'yaze-loading-container';

    // Add task name
    const taskLabel = document.createElement('h3');
    taskLabel.className = 'yaze-loading-task';
    taskLabel.textContent = taskName;
    container.appendChild(taskLabel);

    // Add progress bar container
    const progressContainer = document.createElement('div');
    progressContainer.className = 'yaze-progress-container';

    const progressBar = document.createElement('div');
    progressBar.className = 'yaze-progress-bar';

    const progressFill = document.createElement('div');
    progressFill.className = 'yaze-progress-fill';
    progressFill.style.width = '0%';

    const progressText = document.createElement('span');
    progressText.className = 'yaze-progress-text';
    progressText.textContent = '0%';

    progressBar.appendChild(progressFill);
    progressContainer.appendChild(progressBar);
    progressContainer.appendChild(progressText);
    container.appendChild(progressContainer);

    // Add status message
    const statusMessage = document.createElement('p');
    statusMessage.className = 'yaze-loading-message';
    statusMessage.textContent = 'Initializing...';
    container.appendChild(statusMessage);

    // Add cancel button (hidden by default)
    const cancelButton = document.createElement('button');
    cancelButton.className = 'yaze-cancel-button';
    cancelButton.textContent = 'Cancel';
    cancelButton.style.display = 'none';
    container.appendChild(cancelButton);

    overlay.appendChild(container);
    document.body.appendChild(overlay);

    // Store indicator data
    activeIndicators.set(id, {
      overlay,
      progressFill,
      progressText,
      statusMessage,
      cancelButton,
      cancelled: false,
      startTime: Date.now()
    });

    // Fade in animation
    requestAnimationFrame(() => {
      overlay.classList.add('yaze-loading-visible');
    });
  };

  /**
   * Update loading progress
   * @param {number} id - Loading operation identifier
   * @param {number} progress - Progress value (0.0 to 1.0)
   * @param {string} message - Status message to display
   */
  window.updateLoadingProgress = function(id, progress, message) {
    const indicator = activeIndicators.get(id);
    if (!indicator) return;

    // Clamp progress between 0 and 1
    progress = Math.max(0, Math.min(1, progress));

    const percentage = Math.round(progress * 100);
    indicator.progressFill.style.width = `${percentage}%`;
    indicator.progressText.textContent = `${percentage}%`;

    if (message && message.length > 0) {
      indicator.statusMessage.textContent = message;
    }

    // Add animation class for smooth transitions
    if (!indicator.progressFill.classList.contains('yaze-progress-animated')) {
      indicator.progressFill.classList.add('yaze-progress-animated');
    }

    // Show estimated time remaining after 2 seconds
    const elapsed = Date.now() - indicator.startTime;
    if (elapsed > 2000 && progress > 0.01 && progress < 0.99) {
      const estimatedTotal = elapsed / progress;
      const remaining = estimatedTotal - elapsed;
      const remainingSeconds = Math.round(remaining / 1000);

      if (remainingSeconds > 0) {
        const timeStr = formatTime(remainingSeconds);
        indicator.statusMessage.textContent += ` (${timeStr} remaining)`;
      }
    }
  };

  /**
   * Show cancel button for a loading operation
   * @param {number} id - Loading operation identifier
   * @param {function} callback - Function to call when cancel is clicked
   */
  window.showCancelButton = function(id, callback) {
    const indicator = activeIndicators.get(id);
    if (!indicator) return;

    indicator.cancelButton.style.display = 'inline-block';
    indicator.cancelButton.onclick = function() {
      indicator.cancelled = true;
      indicator.cancelButton.disabled = true;
      indicator.cancelButton.textContent = 'Cancelling...';

      if (callback) {
        callback();
      }
    };
  };

  /**
   * Check if loading operation was cancelled
   * @param {number} id - Loading operation identifier
   * @returns {boolean} True if cancelled
   */
  window.isLoadingCancelled = function(id) {
    const indicator = activeIndicators.get(id);
    return indicator ? indicator.cancelled : false;
  };

  /**
   * Remove a loading indicator
   * @param {number} id - Loading operation identifier
   */
  window.removeLoadingIndicator = function(id) {
    const indicator = activeIndicators.get(id);
    if (!indicator) return;

    // Fade out animation
    indicator.overlay.classList.remove('yaze-loading-visible');

    // Remove after animation completes
    setTimeout(() => {
      if (indicator.overlay.parentNode) {
        indicator.overlay.parentNode.removeChild(indicator.overlay);
      }
      activeIndicators.delete(id);
    }, 300);
  };

  /**
   * Format time in seconds to human-readable string
   * @param {number} seconds - Time in seconds
   * @returns {string} Formatted time string
   */
  function formatTime(seconds) {
    if (seconds < 60) {
      return `${seconds}s`;
    } else if (seconds < 3600) {
      const minutes = Math.floor(seconds / 60);
      const secs = seconds % 60;
      return secs > 0 ? `${minutes}m ${secs}s` : `${minutes}m`;
    } else {
      const hours = Math.floor(seconds / 3600);
      const minutes = Math.floor((seconds % 3600) / 60);
      return minutes > 0 ? `${hours}h ${minutes}m` : `${hours}h`;
    }
  }

  /**
   * Emergency cleanup - remove all indicators
   * Called on page unload or error conditions
   */
  window.removeAllLoadingIndicators = function() {
    for (const [id, indicator] of activeIndicators.entries()) {
      if (indicator.overlay.parentNode) {
        indicator.overlay.parentNode.removeChild(indicator.overlay);
      }
    }
    activeIndicators.clear();
  };

  // Clean up on page unload
  window.addEventListener('beforeunload', function() {
    window.removeAllLoadingIndicators();
  });

  // Handle visibility changes (e.g., tab switching)
  document.addEventListener('visibilitychange', function() {
    if (document.hidden) {
      // Pause animations when tab is hidden to save resources
      for (const [id, indicator] of activeIndicators.entries()) {
        indicator.progressFill.style.transition = 'none';
      }
    } else {
      // Resume animations when tab is visible
      for (const [id, indicator] of activeIndicators.entries()) {
        indicator.progressFill.style.transition = '';
      }
    }
  });

})();