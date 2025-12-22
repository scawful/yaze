/**
 * @fileoverview Touch gesture handling system for yaze WASM build
 *
 * Provides comprehensive touch gesture recognition for iPad and tablet browsers.
 * Handles multi-touch gestures (pinch, pan, rotate) and translates them to
 * both ImGui mouse events and custom C++ gesture callbacks.
 *
 * Features:
 * - Single tap: Click/select
 * - Double tap: Context menu or zoom to fit
 * - Long press (500ms): Context menu
 * - Two-finger pan: Scroll/pan canvas
 * - Pinch to zoom: Zoom canvas in/out
 * - Two-finger rotate: Optional rotation for sprites
 * - Smooth inertia scrolling after swipe
 *
 * Usage:
 * Include this script in your HTML, then call YazeTouchGestures.initialize()
 * after the Emscripten Module is ready.
 */

(function() {
  'use strict';

  // Gesture types (must match TouchGesture enum in C++)
  const GestureType = {
    NONE: 0,
    TAP: 1,
    DOUBLE_TAP: 2,
    LONG_PRESS: 3,
    PAN: 4,
    PINCH_ZOOM: 5,
    ROTATE: 6
  };

  // Gesture phases (must match TouchPhase enum in C++)
  const GesturePhase = {
    BEGAN: 0,
    CHANGED: 1,
    ENDED: 2,
    CANCELLED: 3
  };

  // Configuration
  const config = {
    // Timing thresholds (milliseconds)
    tapMaxDuration: 300,
    doubleTapMaxDelay: 300,
    longPressDuration: 500,

    // Distance thresholds (pixels) - increased for better touch tolerance
    tapMaxMovement: 20,      // Was 10, increased for finger jitter
    panThreshold: 15,        // Was 10, reduced sensitivity
    pinchThreshold: 8,       // Was 5, reduced sensitivity
    rotationThreshold: 0.15, // Was 0.1, reduced sensitivity

    // Feature toggles
    enablePanZoom: true,
    enableRotation: false,
    enableInertia: true,

    // Inertia settings - reduced for less jumpy behavior
    inertiaDeceleration: 0.92,   // Was 0.95, faster deceleration
    inertiaMinVelocity: 1.0,     // Was 0.5, higher threshold to start inertia

    // Scale limits
    minZoom: 0.25,
    maxZoom: 4.0,

    // Touch visual feedback
    showTouchRipples: true,
    rippleDuration: 300,

    // Smoothing settings (new)
    positionSmoothing: 0.3,      // Interpolation factor (0 = max smooth, 1 = no smooth)
    velocitySmoothing: 0.5,      // Velocity averaging factor

    // Edge gesture prevention (for iOS swipe-back)
    edgeGestureThreshold: 25,    // Pixels from screen edge to detect edge gestures
    preventEdgeGestures: true,   // Prevent iOS swipe-back and other edge gestures

    // Mobile-specific settings
    mobileDoubleTapZoom: true,   // Enable double-tap to zoom
    mobilePanSensitivity: 1.5    // Increased pan sensitivity for mobile
  };

  // State tracking
  const state = {
    initialized: false,
    canvas: null,
    touchPoints: new Map(),
    activeTouchCount: 0,

    // Gesture recognition
    currentGesture: GestureType.NONE,
    gesturePhase: GesturePhase.BEGAN,

    // Tap detection
    lastTapTime: 0,
    lastTapPosition: { x: 0, y: 0 },
    potentialTap: false,
    potentialDoubleTap: false,
    longPressTimeout: null,
    longPressDetected: false,

    // Two-finger tracking
    initialPinchDistance: 0,
    initialRotationAngle: 0,
    initialPanCenter: { x: 0, y: 0 },
    currentScale: 1.0,
    currentRotation: 0.0,

    // Inertia
    velocityX: 0,
    velocityY: 0,
    inertiaActive: false,
    inertiaAnimationId: null,

    // Touch start info
    touchStartTime: 0,
    touchStartPosition: { x: 0, y: 0 }
  };

  // Utility functions
  function distance(p1, p2) {
    const dx = p2.x - p1.x;
    const dy = p2.y - p1.y;
    return Math.sqrt(dx * dx + dy * dy);
  }

  function angle(p1, p2) {
    return Math.atan2(p2.y - p1.y, p2.x - p1.x);
  }

  function midpoint(p1, p2) {
    return {
      x: (p1.x + p2.x) / 2,
      y: (p1.y + p2.y) / 2
    };
  }

  function clamp(value, min, max) {
    return Math.min(Math.max(value, min), max);
  }

  function getCanvasCoordinates(touch) {
    if (!state.canvas) return { x: 0, y: 0 };

    const rect = state.canvas.getBoundingClientRect();
    const scaleX = state.canvas.width / rect.width;
    const scaleY = state.canvas.height / rect.height;

    return {
      x: (touch.clientX - rect.left) * scaleX,
      y: (touch.clientY - rect.top) * scaleY
    };
  }

  // Send touch event to C++
  function sendTouchEvent(type, id, x, y, pressure, timestamp) {
    if (typeof Module === 'undefined') return;

    try {
      if (typeof Module._OnTouchEvent === 'function') {
        Module._OnTouchEvent(type, id, x, y, pressure, timestamp);
      } else if (typeof Module.ccall === 'function') {
        Module.ccall('OnTouchEvent', null,
          ['number', 'number', 'number', 'number', 'number', 'number'],
          [type, id, x, y, pressure, timestamp]);
      }
    } catch (e) {
      console.warn('Failed to send touch event to C++:', e);
    }
  }

  // Send gesture event to C++
  function sendGestureEvent(type, phase, x, y, scale, rotation) {
    if (typeof Module === 'undefined') return;

    try {
      if (typeof Module._OnGestureEvent === 'function') {
        Module._OnGestureEvent(type, phase, x, y, scale, rotation);
      } else if (typeof Module.ccall === 'function') {
        Module.ccall('OnGestureEvent', null,
          ['number', 'number', 'number', 'number', 'number', 'number'],
          [type, phase, x, y, scale, rotation]);
      }
    } catch (e) {
      console.warn('Failed to send gesture event to C++:', e);
    }
  }

  // Visual feedback - touch ripple effect
  function showTouchRipple(x, y) {
    if (!config.showTouchRipples || !state.canvas) return;

    const rect = state.canvas.getBoundingClientRect();
    const ripple = document.createElement('div');
    ripple.className = 'yaze-touch-ripple';
    ripple.style.left = (x / (state.canvas.width / rect.width) + rect.left) + 'px';
    ripple.style.top = (y / (state.canvas.height / rect.height) + rect.top) + 'px';
    document.body.appendChild(ripple);

    // Animate and remove
    requestAnimationFrame(() => {
      ripple.classList.add('yaze-touch-ripple-active');
    });

    setTimeout(() => {
      ripple.remove();
    }, config.rippleDuration);
  }

  // Clear long press timeout
  function clearLongPressTimeout() {
    if (state.longPressTimeout) {
      clearTimeout(state.longPressTimeout);
      state.longPressTimeout = null;
    }
  }

  // Start long press detection
  function startLongPressDetection(x, y) {
    clearLongPressTimeout();

    state.longPressTimeout = setTimeout(() => {
      if (state.activeTouchCount === 1 && state.potentialTap) {
        // Check if finger hasn't moved much
        const touches = Array.from(state.touchPoints.values());
        if (touches.length === 1) {
          const movement = distance(touches[0].current, touches[0].start);
          if (movement <= config.tapMaxMovement) {
            state.longPressDetected = true;
            state.potentialTap = false;
            state.currentGesture = GestureType.LONG_PRESS;
            state.gesturePhase = GesturePhase.BEGAN;

            // Show visual feedback
            showTouchRipple(x, y);

            // Send to C++
            sendGestureEvent(GestureType.LONG_PRESS, GesturePhase.BEGAN, x, y, 1.0, 0.0);

            // Trigger context menu via right-click simulation
            if (typeof Module !== 'undefined' && Module.canvas) {
              const rect = Module.canvas.getBoundingClientRect();
              const scaleX = Module.canvas.width / rect.width;
              const scaleY = Module.canvas.height / rect.height;
              const screenX = x / scaleX + rect.left;
              const screenY = y / scaleY + rect.top;

              // Simulate right mouse button for ImGui
              const io = ImGui_GetIO ? ImGui_GetIO() : null;
              if (io) {
                // Right-click simulation handled by C++ side
              }
            }
          }
        }
      }
    }, config.longPressDuration);
  }

  // Process inertia animation
  function processInertia() {
    if (!state.inertiaActive) return;

    const velocityMag = Math.sqrt(
      state.velocityX * state.velocityX +
      state.velocityY * state.velocityY
    );

    if (velocityMag < config.inertiaMinVelocity) {
      state.inertiaActive = false;
      state.velocityX = 0;
      state.velocityY = 0;
      return;
    }

    // Send pan event with velocity
    const center = state.initialPanCenter;
    sendGestureEvent(
      GestureType.PAN,
      GesturePhase.CHANGED,
      center.x + state.velocityX,
      center.y + state.velocityY,
      1.0,
      0.0
    );

    // Update center for next frame
    state.initialPanCenter.x += state.velocityX;
    state.initialPanCenter.y += state.velocityY;

    // Apply deceleration
    state.velocityX *= config.inertiaDeceleration;
    state.velocityY *= config.inertiaDeceleration;

    // Continue animation
    state.inertiaAnimationId = requestAnimationFrame(processInertia);
  }

  // Stop inertia
  function stopInertia() {
    state.inertiaActive = false;
    if (state.inertiaAnimationId) {
      cancelAnimationFrame(state.inertiaAnimationId);
      state.inertiaAnimationId = null;
    }
  }

  // Get first two touches
  function getFirstTwoTouches() {
    const touches = Array.from(state.touchPoints.values());
    return [touches[0] || null, touches[1] || null];
  }

  // Check if touch is near screen edge (for iOS swipe-back prevention)
  function isTouchNearEdge(touch) {
    if (!config.preventEdgeGestures) return false;
    const threshold = config.edgeGestureThreshold;
    return touch.clientX < threshold ||
           touch.clientX > window.innerWidth - threshold;
  }

  // Handle touch start
  function handleTouchStart(event) {
    // Prevent iOS swipe-back gesture when touching near edges
    if (config.preventEdgeGestures) {
      for (let i = 0; i < event.touches.length; i++) {
        if (isTouchNearEdge(event.touches[i])) {
          event.preventDefault();
          break;
        }
      }
    }

    event.preventDefault();
    stopInertia();

    const timestamp = event.timeStamp / 1000.0;

    for (let i = 0; i < event.changedTouches.length; i++) {
      const touch = event.changedTouches[i];
      const pos = getCanvasCoordinates(touch);

      state.touchPoints.set(touch.identifier, {
        id: touch.identifier,
        start: { ...pos },
        current: { ...pos },
        previous: { ...pos },
        pressure: touch.force || 1.0,
        timestamp: timestamp
      });

      // Send to C++
      sendTouchEvent(0, touch.identifier, pos.x, pos.y, touch.force || 1.0, timestamp);
    }

    state.activeTouchCount = state.touchPoints.size;

    // Single touch - start tap/long-press detection
    if (state.activeTouchCount === 1) {
      const touch = state.touchPoints.values().next().value;
      state.touchStartTime = timestamp;
      state.touchStartPosition = { ...touch.start };
      state.potentialTap = true;
      state.longPressDetected = false;

      startLongPressDetection(touch.start.x, touch.start.y);
    }

    // Two+ touches - cancel tap detection, prepare for gestures
    if (state.activeTouchCount >= 2) {
      clearLongPressTimeout();
      state.potentialTap = false;
      state.longPressDetected = false;

      // Initialize two-finger gesture tracking
      const [t1, t2] = getFirstTwoTouches();
      if (t1 && t2) {
        state.initialPinchDistance = distance(t1.current, t2.current);
        state.initialRotationAngle = angle(t1.current, t2.current);
        state.initialPanCenter = midpoint(t1.current, t2.current);
        state.currentScale = 1.0;
        state.currentRotation = 0.0;
      }
    }
  }

  // Smooth position interpolation to reduce jitter
  function smoothPosition(current, target, factor) {
    return {
      x: current.x + (target.x - current.x) * factor,
      y: current.y + (target.y - current.y) * factor
    };
  }

  // Handle touch move
  function handleTouchMove(event) {
    event.preventDefault();

    const timestamp = event.timeStamp / 1000.0;

    for (let i = 0; i < event.changedTouches.length; i++) {
      const touch = event.changedTouches[i];
      const touchData = state.touchPoints.get(touch.identifier);

      if (touchData) {
        const rawPos = getCanvasCoordinates(touch);

        // Apply position smoothing to reduce jitter
        const smoothedPos = smoothPosition(
          touchData.current,
          rawPos,
          config.positionSmoothing
        );

        touchData.previous = { ...touchData.current };
        touchData.current = smoothedPos;
        touchData.rawCurrent = rawPos; // Keep raw for gesture detection
        touchData.pressure = touch.force || 1.0;

        // Send smoothed position to C++
        sendTouchEvent(1, touch.identifier, smoothedPos.x, smoothedPos.y, touch.force || 1.0, timestamp);
      }
    }

    // Check for gesture recognition
    if (state.activeTouchCount === 1 && state.potentialTap) {
      const touch = state.touchPoints.values().next().value;
      const movement = distance(touch.current, touch.start);

      if (movement > config.tapMaxMovement) {
        // Too much movement - no longer a tap
        clearLongPressTimeout();
        state.potentialTap = false;
      }
    }

    // Two-finger gestures
    if (state.activeTouchCount >= 2 && config.enablePanZoom) {
      const [t1, t2] = getFirstTwoTouches();
      if (!t1 || !t2) return;

      const currentDistance = distance(t1.current, t2.current);
      const currentAngle = angle(t1.current, t2.current);
      const currentCenter = midpoint(t1.current, t2.current);

      // Calculate changes
      const scaleRatio = state.initialPinchDistance > 0
        ? currentDistance / state.initialPinchDistance
        : 1.0;

      let rotationDelta = currentAngle - state.initialRotationAngle;
      // Normalize to [-PI, PI]
      while (rotationDelta > Math.PI) rotationDelta -= 2 * Math.PI;
      while (rotationDelta < -Math.PI) rotationDelta += 2 * Math.PI;

      const panDelta = {
        x: currentCenter.x - state.initialPanCenter.x,
        y: currentCenter.y - state.initialPanCenter.y
      };

      // Determine dominant gesture
      const scaleChange = Math.abs(scaleRatio - 1.0);
      const panDistance = distance(currentCenter, state.initialPanCenter);
      const rotationChange = Math.abs(rotationDelta);

      const isPinch = scaleChange > config.pinchThreshold / 100.0;
      const isPan = panDistance > config.panThreshold;
      const isRotate = config.enableRotation && rotationChange > config.rotationThreshold;

      // Prioritize: pinch > rotate > pan
      let gestureType = GestureType.NONE;

      if (isPinch) {
        gestureType = GestureType.PINCH_ZOOM;
        state.currentScale = clamp(
          state.currentScale * scaleRatio,
          config.minZoom,
          config.maxZoom
        );
        state.initialPinchDistance = currentDistance;  // Reset for continuous
      } else if (isRotate) {
        gestureType = GestureType.ROTATE;
        state.currentRotation += rotationDelta;
        state.initialRotationAngle = currentAngle;
      } else if (isPan) {
        gestureType = GestureType.PAN;

        // Track velocity for inertia with smoothing to reduce jitter
        const prevCenter = midpoint(t1.previous, t2.previous);
        const rawVelocityX = currentCenter.x - prevCenter.x;
        const rawVelocityY = currentCenter.y - prevCenter.y;

        // Smooth velocity to reduce jumpy inertia
        state.velocityX = state.velocityX * (1 - config.velocitySmoothing) +
                          rawVelocityX * config.velocitySmoothing;
        state.velocityY = state.velocityY * (1 - config.velocitySmoothing) +
                          rawVelocityY * config.velocitySmoothing;

        state.initialPanCenter = { ...currentCenter };
      }

      if (gestureType !== GestureType.NONE) {
        const phase = state.currentGesture === gestureType
          ? GesturePhase.CHANGED
          : GesturePhase.BEGAN;

        state.currentGesture = gestureType;
        state.gesturePhase = phase;

        sendGestureEvent(
          gestureType,
          phase,
          currentCenter.x,
          currentCenter.y,
          scaleRatio,
          rotationDelta
        );
      }
    }
  }

  // Handle touch end
  function handleTouchEnd(event) {
    event.preventDefault();
    clearLongPressTimeout();

    const timestamp = event.timeStamp / 1000.0;

    for (let i = 0; i < event.changedTouches.length; i++) {
      const touch = event.changedTouches[i];
      const pos = getCanvasCoordinates(touch);

      // Send to C++
      sendTouchEvent(2, touch.identifier, pos.x, pos.y, touch.force || 1.0, timestamp);

      state.touchPoints.delete(touch.identifier);
    }

    const prevTouchCount = state.activeTouchCount;
    state.activeTouchCount = state.touchPoints.size;

    // Handle tap detection
    if (state.activeTouchCount === 0 && prevTouchCount === 1) {
      if (state.potentialTap && !state.longPressDetected) {
        const duration = timestamp - state.touchStartTime;

        if (duration <= config.tapMaxDuration / 1000.0) {
          // Check for double tap
          const timeSinceLastTap = timestamp - state.lastTapTime;

          if (state.potentialDoubleTap &&
              timeSinceLastTap <= config.doubleTapMaxDelay / 1000.0) {
            // Double tap!
            state.currentGesture = GestureType.DOUBLE_TAP;
            state.gesturePhase = GesturePhase.ENDED;
            state.potentialDoubleTap = false;

            showTouchRipple(state.touchStartPosition.x, state.touchStartPosition.y);
            sendGestureEvent(
              GestureType.DOUBLE_TAP,
              GesturePhase.ENDED,
              state.touchStartPosition.x,
              state.touchStartPosition.y,
              1.0,
              0.0
            );
          } else {
            // Single tap
            state.currentGesture = GestureType.TAP;
            state.gesturePhase = GesturePhase.ENDED;
            state.lastTapTime = timestamp;
            state.lastTapPosition = { ...state.touchStartPosition };
            state.potentialDoubleTap = true;

            showTouchRipple(state.touchStartPosition.x, state.touchStartPosition.y);
            sendGestureEvent(
              GestureType.TAP,
              GesturePhase.ENDED,
              state.touchStartPosition.x,
              state.touchStartPosition.y,
              1.0,
              0.0
            );
          }
        }
      }

      state.potentialTap = false;
    }

    // Handle gesture end
    if (state.activeTouchCount === 0 && prevTouchCount >= 2) {
      // End any active two-finger gesture
      if (state.currentGesture === GestureType.PAN ||
          state.currentGesture === GestureType.PINCH_ZOOM ||
          state.currentGesture === GestureType.ROTATE) {

        sendGestureEvent(
          state.currentGesture,
          GesturePhase.ENDED,
          state.initialPanCenter.x,
          state.initialPanCenter.y,
          state.currentScale,
          state.currentRotation
        );

        // Start inertia for pan gesture
        if (state.currentGesture === GestureType.PAN && config.enableInertia) {
          const velocityMag = Math.sqrt(
            state.velocityX * state.velocityX +
            state.velocityY * state.velocityY
          );

          if (velocityMag > config.inertiaMinVelocity) {
            state.inertiaActive = true;
            state.inertiaAnimationId = requestAnimationFrame(processInertia);
          }
        }

        state.currentGesture = GestureType.NONE;
      }
    }

    // Clear double-tap potential after timeout
    if (state.potentialDoubleTap) {
      setTimeout(() => {
        state.potentialDoubleTap = false;
      }, config.doubleTapMaxDelay);
    }

    // End long press gesture
    if (state.longPressDetected && state.activeTouchCount === 0) {
      sendGestureEvent(
        GestureType.LONG_PRESS,
        GesturePhase.ENDED,
        state.touchStartPosition.x,
        state.touchStartPosition.y,
        1.0,
        0.0
      );
      state.longPressDetected = false;
      state.currentGesture = GestureType.NONE;
    }
  }

  // Handle touch cancel
  function handleTouchCancel(event) {
    event.preventDefault();
    clearLongPressTimeout();
    stopInertia();

    const timestamp = event.timeStamp / 1000.0;

    for (let i = 0; i < event.changedTouches.length; i++) {
      const touch = event.changedTouches[i];
      const pos = getCanvasCoordinates(touch);

      // Send to C++
      sendTouchEvent(3, touch.identifier, pos.x, pos.y, touch.force || 1.0, timestamp);

      state.touchPoints.delete(touch.identifier);
    }

    state.activeTouchCount = state.touchPoints.size;
    state.potentialTap = false;
    state.longPressDetected = false;

    // Cancel any active gesture
    if (state.currentGesture !== GestureType.NONE) {
      sendGestureEvent(
        state.currentGesture,
        GesturePhase.CANCELLED,
        state.initialPanCenter.x,
        state.initialPanCenter.y,
        state.currentScale,
        state.currentRotation
      );
      state.currentGesture = GestureType.NONE;
    }
  }

  // Public API
  window.YazeTouchGestures = {
    /**
     * Initialize touch gesture handling
     */
    initialize: function() {
      if (state.initialized) {
        console.warn('YazeTouchGestures already initialized');
        return;
      }

      // Find canvas
      state.canvas = document.getElementById('canvas');
      if (!state.canvas) {
        console.error('Canvas element not found for touch handling');
        return;
      }

      // Prevent default touch behaviors on canvas
      state.canvas.style.touchAction = 'none';
      state.canvas.style.userSelect = 'none';
      state.canvas.style.webkitUserSelect = 'none';
      state.canvas.style.webkitTouchCallout = 'none';

      // Add touch event listeners
      state.canvas.addEventListener('touchstart', handleTouchStart, { passive: false });
      state.canvas.addEventListener('touchmove', handleTouchMove, { passive: false });
      state.canvas.addEventListener('touchend', handleTouchEnd, { passive: false });
      state.canvas.addEventListener('touchcancel', handleTouchCancel, { passive: false });

      // Prevent document-level gestures that might interfere
      document.addEventListener('gesturestart', (e) => e.preventDefault(), { passive: false });
      document.addEventListener('gesturechange', (e) => e.preventDefault(), { passive: false });
      document.addEventListener('gestureend', (e) => e.preventDefault(), { passive: false });

      state.initialized = true;
      console.log('YazeTouchGestures initialized');
    },

    /**
     * Shutdown touch gesture handling
     */
    shutdown: function() {
      if (!state.initialized) return;

      if (state.canvas) {
        state.canvas.removeEventListener('touchstart', handleTouchStart);
        state.canvas.removeEventListener('touchmove', handleTouchMove);
        state.canvas.removeEventListener('touchend', handleTouchEnd);
        state.canvas.removeEventListener('touchcancel', handleTouchCancel);
      }

      stopInertia();
      clearLongPressTimeout();
      state.touchPoints.clear();
      state.initialized = false;

      console.log('YazeTouchGestures shutdown');
    },

    /**
     * Get current configuration
     */
    getConfig: function() {
      return { ...config };
    },

    /**
     * Update configuration
     */
    setConfig: function(newConfig) {
      Object.assign(config, newConfig);
    },

    /**
     * Get current state (for debugging)
     */
    getState: function() {
      return {
        initialized: state.initialized,
        activeTouchCount: state.activeTouchCount,
        currentGesture: state.currentGesture,
        gesturePhase: state.gesturePhase,
        currentScale: state.currentScale,
        currentRotation: state.currentRotation,
        inertiaActive: state.inertiaActive
      };
    },

    /**
     * Check if touch input is active
     */
    isTouchActive: function() {
      return state.activeTouchCount > 0;
    },

    /**
     * Reset gesture state
     */
    reset: function() {
      stopInertia();
      clearLongPressTimeout();
      state.touchPoints.clear();
      state.activeTouchCount = 0;
      state.currentGesture = GestureType.NONE;
      state.potentialTap = false;
      state.potentialDoubleTap = false;
      state.longPressDetected = false;
      state.currentScale = 1.0;
      state.currentRotation = 0.0;
      state.velocityX = 0;
      state.velocityY = 0;
    },

    // Constants for external use
    GestureType: GestureType,
    GesturePhase: GesturePhase
  };

  // Auto-initialize when Module is ready (if loading order allows)
  if (typeof Module !== 'undefined' && Module.onRuntimeInitialized) {
    const originalOnRuntimeInitialized = Module.onRuntimeInitialized;
    Module.onRuntimeInitialized = function() {
      originalOnRuntimeInitialized.call(Module);
      // Wait a frame for canvas to be ready
      requestAnimationFrame(() => {
        window.YazeTouchGestures.initialize();
      });
    };
  }

})();
