/**
 * @fileoverview Browser-side error handling and notification system for yaze WASM
 * This file provides modal dialogs, toast notifications, progress indicators,
 * and confirmation dialogs for the WASM application.
 */

(function() {
  'use strict';

  // Utility function to escape HTML
  function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }

  // Utility function to create DOM element with classes
  function createElement(tag, classes, content) {
    const element = document.createElement(tag);
    if (classes) {
      if (Array.isArray(classes)) {
        element.classList.add(...classes);
      } else {
        element.className = classes;
      }
    }
    // Only append when content is defined and not null; this avoids
    // `appendChild(null)` crashes when callers pass null/undefined.
    if (content !== undefined && content !== null) {
      if (typeof content === 'string') {
        element.innerHTML = content;
      } else if (content instanceof Node) {
        element.appendChild(content);
      } else {
        element.textContent = String(content);
      }
    }
    return element;
  }

  /**
   * Show a modal dialog
   * @param {string} title - Dialog title
   * @param {string} message - Dialog message
   * @param {string} type - Dialog type (error, warning, info)
   */
  window.showYazeModal = function(title, message, type) {
    // Remove any existing modal
    const existingModal = document.querySelector('.yaze-modal-overlay');
    if (existingModal) {
      existingModal.remove();
    }

    // Create modal overlay
    const overlay = createElement('div', 'yaze-modal-overlay');

    // Create modal content container
    const content = createElement('div', `yaze-modal-content yaze-modal-${type}`);

    // Add icon based on type
    const iconMap = {
      'error': '❌',
      'warning': '⚠️',
      'info': 'ℹ️'
    };
    const icon = iconMap[type] || 'ℹ️';

    // Create header with icon
    const header = createElement('div', 'yaze-modal-header');
    header.innerHTML = `
      <span class="yaze-modal-icon">${icon}</span>
      <h2 class="yaze-modal-title">${escapeHtml(title)}</h2>
    `;

    // Create message body
    const body = createElement('div', 'yaze-modal-body');
    body.innerHTML = `<p>${escapeHtml(message)}</p>`;

    // Create footer with OK button
    const footer = createElement('div', 'yaze-modal-footer');
    const okButton = createElement('button', 'yaze-modal-button', 'OK');
    okButton.onclick = function() {
      overlay.remove();
    };
    footer.appendChild(okButton);

    // Assemble modal
    content.appendChild(header);
    content.appendChild(body);
    content.appendChild(footer);
    overlay.appendChild(content);

    // Add to document
    document.body.appendChild(overlay);

    // Focus the OK button
    okButton.focus();

    // Close on Escape key
    overlay.addEventListener('keydown', function(e) {
      if (e.key === 'Escape') {
        overlay.remove();
      }
    });

    // Close on overlay click (outside modal)
    overlay.addEventListener('click', function(e) {
      if (e.target === overlay) {
        overlay.remove();
      }
    });
  };

  /**
   * Show a toast notification
   * @param {string} message - Toast message
   * @param {string} type - Toast type (info, success, warning, error)
   * @param {number} duration - Duration in milliseconds
   */
  window.showYazeToast = function(message, type, duration) {
    duration = duration || 3000;

    // Create toast container if it doesn't exist
    let container = document.getElementById('yaze-toast-container');
    if (!container) {
      container = createElement('div', null, null);
      container.id = 'yaze-toast-container';
      container.className = 'yaze-toast-container';
      document.body.appendChild(container);
    }

    // Create toast element
    const toast = createElement('div', ['yaze-toast', `yaze-toast-${type}`]);

    // Add icon based on type
    const iconMap = {
      'success': '✅',
      'warning': '⚠️',
      'error': '❌',
      'info': 'ℹ️'
    };
    const icon = iconMap[type] || 'ℹ️';

    toast.innerHTML = `
      <span class="yaze-toast-icon">${icon}</span>
      <span class="yaze-toast-message">${escapeHtml(message)}</span>
    `;

    // Add close button
    const closeButton = createElement('button', 'yaze-toast-close', '×');
    closeButton.onclick = function() {
      toast.classList.add('yaze-toast-hiding');
      setTimeout(() => toast.remove(), 300);
    };
    toast.appendChild(closeButton);

    // Add to container
    container.appendChild(toast);

    // Auto-remove after duration
    setTimeout(function() {
      toast.classList.add('yaze-toast-hiding');
      setTimeout(() => toast.remove(), 300);
    }, duration);
  };

  /**
   * Show a progress indicator
   * @param {string} task - Task description
   * @param {number} progress - Progress value (0.0 to 1.0)
   */
  window.showYazeProgress = function(task, progress) {
    let container = document.getElementById('yaze-progress-container');

    if (!container) {
      // Create progress container
      container = createElement('div', null, null);
      container.id = 'yaze-progress-container';
      container.className = 'yaze-progress-container';

      // Create progress content
      const content = createElement('div', 'yaze-progress-content');

      // Task label
      const label = createElement('div', 'yaze-progress-label');
      label.id = 'yaze-progress-label';

      // Progress bar
      const barContainer = createElement('div', 'yaze-progress-bar-container');
      const bar = createElement('div', 'yaze-progress-bar');
      bar.id = 'yaze-progress-bar';
      barContainer.appendChild(bar);

      // Percentage text
      const percentage = createElement('div', 'yaze-progress-percentage');
      percentage.id = 'yaze-progress-percentage';

      // Cancel button
      const cancelButton = createElement('button', 'yaze-progress-cancel', 'Cancel');
      cancelButton.onclick = function() {
        // Trigger cancellation in WASM
        if (window.Module && window.Module._cancelCurrentOperation) {
          window.Module._cancelCurrentOperation();
        }
        window.hideYazeProgress();
      };

      // Assemble
      content.appendChild(label);
      content.appendChild(barContainer);
      content.appendChild(percentage);
      content.appendChild(cancelButton);
      container.appendChild(content);
      document.body.appendChild(container);
    }

    // Update progress
    const label = document.getElementById('yaze-progress-label');
    const bar = document.getElementById('yaze-progress-bar');
    const percentage = document.getElementById('yaze-progress-percentage');

    if (label) label.textContent = task;
    if (bar) bar.style.width = (progress * 100) + '%';
    if (percentage) percentage.textContent = Math.round(progress * 100) + '%';

    // Show container
    container.style.display = 'flex';
  };

  /**
   * Hide the progress indicator
   */
  window.hideYazeProgress = function() {
    const container = document.getElementById('yaze-progress-container');
    if (container) {
      container.style.display = 'none';
    }
  };

  /**
   * Show a confirmation dialog
   * @param {string} message - Confirmation message
   * @param {function} callback - Callback function(result: boolean)
   */
  window.showYazeConfirm = function(message, callback) {
    // Remove any existing modal
    const existingModal = document.querySelector('.yaze-confirm-overlay');
    if (existingModal) {
      existingModal.remove();
    }

    // Create modal overlay
    const overlay = createElement('div', 'yaze-modal-overlay yaze-confirm-overlay');

    // Create modal content container
    const content = createElement('div', 'yaze-modal-content yaze-modal-confirm');

    // Create header
    const header = createElement('div', 'yaze-modal-header');
    header.innerHTML = `
      <span class="yaze-modal-icon">❓</span>
      <h2 class="yaze-modal-title">Confirm</h2>
    `;

    // Create message body
    const body = createElement('div', 'yaze-modal-body');
    body.innerHTML = `<p>${escapeHtml(message)}</p>`;

    // Create footer with buttons
    const footer = createElement('div', 'yaze-modal-footer');

    const cancelButton = createElement('button', 'yaze-modal-button yaze-modal-button-secondary', 'Cancel');
    cancelButton.onclick = function() {
      overlay.remove();
      if (callback) callback(false);
    };

    const confirmButton = createElement('button', 'yaze-modal-button yaze-modal-button-primary', 'Confirm');
    confirmButton.onclick = function() {
      overlay.remove();
      if (callback) callback(true);
    };

    footer.appendChild(cancelButton);
    footer.appendChild(confirmButton);

    // Assemble modal
    content.appendChild(header);
    content.appendChild(body);
    content.appendChild(footer);
    overlay.appendChild(content);

    // Add to document
    document.body.appendChild(overlay);

    // Focus the confirm button
    confirmButton.focus();

    // Handle keyboard events
    overlay.addEventListener('keydown', function(e) {
      if (e.key === 'Escape') {
        overlay.remove();
        if (callback) callback(false);
      } else if (e.key === 'Enter') {
        overlay.remove();
        if (callback) callback(true);
      }
    });

    // Close on overlay click (outside modal)
    overlay.addEventListener('click', function(e) {
      if (e.target === overlay) {
        overlay.remove();
        if (callback) callback(false);
      }
    });
  };

  // Initialize on DOM ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', function() {
      console.log('Yaze error handler initialized');
    });
  } else {
    console.log('Yaze error handler initialized');
  }

})();
