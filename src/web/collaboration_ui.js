/**
 * @fileoverview Real-time collaboration UI for WASM yaze
 * Provides user list, session management, and cursor visualization
 */

(function() {
  'use strict';

  // Collaboration state
  const collabState = {
    isConnected: false,
    roomCode: '',
    sessionName: '',
    users: [],
    cursors: new Map(),
    selfUserId: '',
    selfColor: '#4ECDC4'
  };

  // UI Elements
  let collabPanel = null;
  let userList = null;
  let statusBadge = null;
  let roomCodeDisplay = null;
  let cursorOverlay = null;
  let collabBindings = null;
  let bindingPoll = null;

  function ensureCollabBindings() {
    if (collabBindings) return collabBindings;
    if (typeof Module === 'undefined' || typeof Module.cwrap !== 'function') {
      return null;
    }
    collabBindings = {
      create: Module.cwrap('WasmCollaborationCreate', 'string', ['string', 'string', 'string']),
      join: Module.cwrap('WasmCollaborationJoin', 'number', ['string', 'string', 'string']),
      leave: Module.cwrap('WasmCollaborationLeave', 'number', []),
      sendCursor: Module.cwrap('WasmCollaborationSendCursor', 'number', ['string', 'number', 'number', 'number']),
      broadcastChange: Module.cwrap('WasmCollaborationBroadcastChange', 'number', ['number', 'number', 'number']),
      setServer: Module.cwrap('WasmCollaborationSetServerUrl', null, ['string']),
      isConnected: Module.cwrap('WasmCollaborationIsConnected', 'number', []),
      getRoomCode: Module.cwrap('WasmCollaborationGetRoomCode', 'string', []),
      getUserId: Module.cwrap('WasmCollaborationGetUserId', 'string', [])
    };

    if (typeof window !== 'undefined' &&
        window.YAZE_CONFIG &&
        window.YAZE_CONFIG.collaborationServerUrl) {
      collabBindings.setServer(window.YAZE_CONFIG.collaborationServerUrl);
    }
    return collabBindings;
  }

  function startBindingPoll() {
    if (bindingPoll) return;
    bindingPoll = setInterval(() => {
      if (ensureCollabBindings()) {
        clearInterval(bindingPoll);
        bindingPoll = null;
      }
    }, 500);
  }

  /**
   * Initialize collaboration UI
   */
  function initCollaborationUI() {
    createCollabPanel();
    createCursorOverlay();
    attachEventHandlers();
    startBindingPoll();

    // Register global handler for C++ callbacks
    window.updateCollaborationUI = handleCollaborationUpdate;
  }

  /**
   * Create the collaboration panel
   */
  function createCollabPanel() {
    // Main panel container
    collabPanel = document.createElement('div');
    collabPanel.id = 'collab-panel';
    collabPanel.className = 'collab-panel collapsed';
    collabPanel.innerHTML = `
      <div class="collab-header">
        <button class="collab-toggle" title="Toggle Collaboration Panel">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"
                  stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
            <circle cx="9" cy="7" r="4" stroke="currentColor" stroke-width="2"/>
            <path d="M23 21v-2a4 4 0 0 0-3-3.87"
                  stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
            <path d="M16 3.13a4 4 0 0 1 0 7.75"
                  stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
          </svg>
        </button>
        <span class="collab-title">Collaboration</span>
        <span class="collab-status-badge disconnected">Offline</span>
      </div>

      <div class="collab-content">
        <div class="collab-session-info">
          <div class="session-controls">
            <button id="create-session-btn" class="collab-btn primary">
              Create Session
            </button>
            <button id="join-session-btn" class="collab-btn">
              Join Session
            </button>
          </div>

          <div class="room-code-section" style="display: none;">
            <label>Room Code:</label>
            <div class="room-code-display">
              <span id="room-code-text">------</span>
              <button id="copy-code-btn" class="icon-btn" title="Copy code">
                <svg width="16" height="16" viewBox="0 0 24 24" fill="none">
                  <rect x="9" y="9" width="13" height="13" rx="2"
                        stroke="currentColor" stroke-width="2"/>
                  <path d="M5 15H4a2 2 0 01-2-2V4a2 2 0 012-2h9a2 2 0 012 2v1"
                        stroke="currentColor" stroke-width="2"/>
                </svg>
              </button>
            </div>
            <div class="session-name" id="session-name-display"></div>
          </div>
        </div>

        <div class="collab-users">
          <h3>Connected Users</h3>
          <ul id="user-list" class="user-list"></ul>
        </div>

        <div class="collab-actions">
          <button id="disconnect-btn" class="collab-btn danger" style="display: none;">
            Disconnect
          </button>
        </div>
      </div>
    `;

    document.body.appendChild(collabPanel);

    // Cache references to elements
    userList = document.getElementById('user-list');
    statusBadge = collabPanel.querySelector('.collab-status-badge');
    roomCodeDisplay = document.getElementById('room-code-text');
  }

  /**
   * Create cursor overlay for remote cursors
   */
  function createCursorOverlay() {
    cursorOverlay = document.createElement('div');
    cursorOverlay.id = 'cursor-overlay';
    cursorOverlay.className = 'cursor-overlay';
    document.body.appendChild(cursorOverlay);
  }

  /**
   * Attach event handlers
   */
  function attachEventHandlers() {
    // Toggle panel visibility
    const toggleBtn = collabPanel.querySelector('.collab-toggle');
    toggleBtn.addEventListener('click', () => {
      collabPanel.classList.toggle('collapsed');
    });

    // Create session button
    const createBtn = document.getElementById('create-session-btn');
    createBtn.addEventListener('click', () => {
      showCreateSessionDialog();
    });

    // Join session button
    const joinBtn = document.getElementById('join-session-btn');
    joinBtn.addEventListener('click', () => {
      showJoinSessionDialog();
    });

    // Copy room code button
    const copyBtn = document.getElementById('copy-code-btn');
    copyBtn.addEventListener('click', () => {
      copyRoomCode();
    });

    // Disconnect button
    const disconnectBtn = document.getElementById('disconnect-btn');
    disconnectBtn.addEventListener('click', () => {
      if (confirm('Are you sure you want to leave the collaboration session?')) {
        const api = ensureCollabBindings();
        if (api && api.leave) {
          api.leave();
        }
        updateConnectionStatus(false);
      }
    });
  }

  /**
   * Show create session dialog
   */
  function showCreateSessionDialog() {
    const dialog = createDialog('Create Collaboration Session', `
      <div class="dialog-form">
        <div class="form-group">
          <label for="session-name">Session Name:</label>
          <input type="text" id="session-name" placeholder="My ROM Edit Session"
                 maxlength="50" autocomplete="off">
        </div>
        <div class="form-group">
          <label for="user-name">Your Name:</label>
          <input type="text" id="user-name" placeholder="Enter your name"
                 maxlength="30" autocomplete="off">
        </div>
        <div class="form-group">
          <label for="session-password">Session Password (optional):</label>
          <input type="password" id="session-password" placeholder="Leave blank for open session"
                 maxlength="64" autocomplete="off">
        </div>
      </div>
    `);

    const sessionInput = dialog.querySelector('#session-name');
    const userInput = dialog.querySelector('#user-name');
    const passwordInput = dialog.querySelector('#session-password');

    // Load saved username
    userInput.value = localStorage.getItem('collab-username') || '';

    dialog.querySelector('.dialog-ok').addEventListener('click', () => {
      const sessionName = sessionInput.value.trim() || 'Unnamed Session';
      const userName = userInput.value.trim();
      const sessionPassword = passwordInput.value.trim();

      if (!userName) {
        alert('Please enter your name');
        return;
      }

      // Save username
      localStorage.setItem('collab-username', userName);

      const api = ensureCollabBindings();
      if (!api || !api.create) {
        alert('Collaboration runtime not ready');
        return;
      }

      const roomCode = api.create(sessionName, userName, sessionPassword);
      if (roomCode) {
        collabState.roomCode = roomCode;
        collabState.sessionName = sessionName;
        collabState.selfUserId = api.getUserId ? api.getUserId() : '';
        updateConnectionStatus(true);
      }

      dialog.remove();
    });
  }

  /**
   * Show join session dialog
   */
  function showJoinSessionDialog() {
    const dialog = createDialog('Join Collaboration Session', `
      <div class="dialog-form">
        <div class="form-group">
          <label for="room-code">Room Code:</label>
          <input type="text" id="room-code" placeholder="ABC123"
                 maxlength="6" autocomplete="off" style="text-transform: uppercase;">
        </div>
        <div class="form-group">
          <label for="user-name">Your Name:</label>
          <input type="text" id="user-name" placeholder="Enter your name"
                 maxlength="30" autocomplete="off">
        </div>
        <div class="form-group">
          <label for="session-password">Session Password (if required):</label>
          <input type="password" id="session-password" placeholder="Enter session password"
                 maxlength="64" autocomplete="off">
        </div>
      </div>
    `);

    const codeInput = dialog.querySelector('#room-code');
    const userInput = dialog.querySelector('#user-name');
    const passwordInput = dialog.querySelector('#session-password');

    // Load saved username
    userInput.value = localStorage.getItem('collab-username') || '';

    dialog.querySelector('.dialog-ok').addEventListener('click', () => {
      const roomCode = codeInput.value.trim().toUpperCase();
      const userName = userInput.value.trim();

      if (roomCode.length !== 6) {
        alert('Room code must be 6 characters');
        return;
      }

      if (!userName) {
        alert('Please enter your name');
        return;
      }

      // Save username
      localStorage.setItem('collab-username', userName);

      const api = ensureCollabBindings();
      if (!api || !api.join) {
        alert('Collaboration runtime not ready');
        return;
      }

      const sessionPassword = passwordInput.value.trim();
      const success = api.join(roomCode, userName, sessionPassword);
      if (success) {
        collabState.roomCode = roomCode;
        collabState.selfUserId = api.getUserId ? api.getUserId() : '';
        updateConnectionStatus(true);
      }

      dialog.remove();
    });
  }

  /**
   * Create a modal dialog
   */
  function createDialog(title, content) {
    const dialog = document.createElement('div');
    dialog.className = 'collab-dialog-overlay';
    dialog.innerHTML = `
      <div class="collab-dialog">
        <div class="dialog-header">
          <h3>${title}</h3>
          <button class="dialog-close">&times;</button>
        </div>
        <div class="dialog-content">
          ${content}
        </div>
        <div class="dialog-footer">
          <button class="dialog-btn dialog-cancel">Cancel</button>
          <button class="dialog-btn dialog-ok primary">OK</button>
        </div>
      </div>
    `;

    document.body.appendChild(dialog);

    // Close handlers
    const closeDialog = () => dialog.remove();
    dialog.querySelector('.dialog-close').addEventListener('click', closeDialog);
    dialog.querySelector('.dialog-cancel').addEventListener('click', closeDialog);
    dialog.addEventListener('click', (e) => {
      if (e.target === dialog) closeDialog();
    });

    // Focus first input
    const firstInput = dialog.querySelector('input');
    if (firstInput) {
      setTimeout(() => firstInput.focus(), 50);
    }

    return dialog;
  }

  /**
   * Copy room code to clipboard
   */
  function copyRoomCode() {
    const code = collabState.roomCode;
    if (!code) return;

    navigator.clipboard.writeText(code).then(() => {
      // Show success feedback
      const copyBtn = document.getElementById('copy-code-btn');
      const originalHTML = copyBtn.innerHTML;
      copyBtn.innerHTML = '✓';
      copyBtn.classList.add('success');

      setTimeout(() => {
        copyBtn.innerHTML = originalHTML;
        copyBtn.classList.remove('success');
      }, 2000);
    }).catch(err => {
      console.error('Failed to copy:', err);
      // Fallback: select text
      const range = document.createRange();
      range.selectNode(roomCodeDisplay);
      window.getSelection().removeAllRanges();
      window.getSelection().addRange(range);
    });
  }

  /**
   * Update connection status UI
   */
  function updateConnectionStatus(connected) {
    collabState.isConnected = connected;

    if (connected) {
      statusBadge.textContent = 'Connected';
      statusBadge.className = 'collab-status-badge connected';

      // Show room code section
      const roomSection = collabPanel.querySelector('.room-code-section');
      roomSection.style.display = 'block';
      roomCodeDisplay.textContent = collabState.roomCode;

      if (collabState.sessionName) {
        const nameDisplay = document.getElementById('session-name-display');
        nameDisplay.textContent = collabState.sessionName;
        nameDisplay.style.display = 'block';
      }

      // Show disconnect button, hide connect buttons
      document.getElementById('create-session-btn').style.display = 'none';
      document.getElementById('join-session-btn').style.display = 'none';
      document.getElementById('disconnect-btn').style.display = 'block';

      // Expand panel
      collabPanel.classList.remove('collapsed');
    } else {
      statusBadge.textContent = 'Offline';
      statusBadge.className = 'collab-status-badge disconnected';

      // Hide room code section
      const roomSection = collabPanel.querySelector('.room-code-section');
      roomSection.style.display = 'none';

      // Show connect buttons, hide disconnect button
      document.getElementById('create-session-btn').style.display = 'block';
      document.getElementById('join-session-btn').style.display = 'block';
      document.getElementById('disconnect-btn').style.display = 'none';

      // Clear user list
      userList.innerHTML = '';
      collabState.users = [];

      // Clear cursors
      collabState.cursors.clear();
      updateCursors();
    }
  }

  /**
   * Handle collaboration updates from C++
   */
  function handleCollaborationUpdate(type, data) {
    console.log('[Collaboration UI]', type, data);

    try {
      const parsedData = data ? JSON.parse(data) : {};

      switch (type) {
        case 'session_created':
        case 'session_joined':
          collabState.roomCode = data; // data is room code string
          if (!collabState.selfUserId) {
            const api = ensureCollabBindings();
            collabState.selfUserId = api && api.getUserId ? api.getUserId() : '';
          }
          updateConnectionStatus(true);
          break;

        case 'session_left':
        case 'disconnected':
          updateConnectionStatus(false);
          break;

        case 'users_update':
          updateUserList(parsedData.users || []);
          break;

        case 'cursor_update':
          updateRemoteCursor(parsedData);
          break;

        case 'change_applied':
          showChangeNotification(parsedData);
          break;

        default:
          console.log('Unknown collaboration update type:', type);
      }
    } catch (e) {
      console.error('Error handling collaboration update:', e);
    }
  }

  /**
   * Update user list display
   */
  function updateUserList(users) {
    collabState.users = users;
    userList.innerHTML = '';

    users.forEach(user => {
      const li = document.createElement('li');
      li.className = 'user-item';
      li.innerHTML = `
        <span class="user-indicator" style="background-color: ${user.color}"></span>
        <span class="user-name">${user.name}</span>
        ${user.id === collabState.selfUserId ? '<span class="user-you">(You)</span>' : ''}
      `;
      userList.appendChild(li);
    });

    // Update user count in header
    const userCount = users.length;
    const title = collabPanel.querySelector('.collab-title');
    title.textContent = `Collaboration (${userCount})`;
  }

  /**
   * Update remote cursor position
   */
  function updateRemoteCursor(cursorData) {
    const { user_id, editor, x, y } = cursorData;

    // Find user info
    const user = collabState.users.find(u => u.id === user_id);
    if (!user) return;

    // Update cursor position
    collabState.cursors.set(user_id, {
      ...cursorData,
      color: user.color,
      name: user.name
    });

    updateCursors();
  }

  /**
   * Render all remote cursors
   */
  function updateCursors() {
    cursorOverlay.innerHTML = '';

    collabState.cursors.forEach((cursor, userId) => {
      if (userId === collabState.selfUserId) return;

      const cursorEl = document.createElement('div');
      cursorEl.className = 'remote-cursor';
      cursorEl.style.left = cursor.x + 'px';
      cursorEl.style.top = cursor.y + 'px';
      cursorEl.style.borderColor = cursor.color;

      const label = document.createElement('span');
      label.className = 'cursor-label';
      label.style.backgroundColor = cursor.color;
      label.textContent = cursor.name;

      cursorEl.appendChild(label);
      cursorOverlay.appendChild(cursorEl);
    });
  }

  /**
   * Show change notification
   */
  function showChangeNotification(changeData) {
    const user = collabState.users.find(u => u.id === changeData.user_id);
    if (!user) return;

    const notification = document.createElement('div');
    notification.className = 'collab-notification';
    notification.innerHTML = `
      <span class="notif-icon" style="background-color: ${user.color}"></span>
      <span>${user.name} modified offset 0x${changeData.offset.toString(16)}</span>
    `;

    document.body.appendChild(notification);

    // Animate in
    setTimeout(() => notification.classList.add('show'), 10);

    // Remove after delay
    setTimeout(() => {
      notification.classList.remove('show');
      setTimeout(() => notification.remove(), 300);
    }, 3000);
  }

  // Initialize when DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initCollaborationUI);
  } else {
    initCollaborationUI();
  }

  // Export for debugging
  window.yazeCollab = {
    state: collabState,
    updateStatus: updateConnectionStatus,
    showNotification: showChangeNotification
  };

})();
