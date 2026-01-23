(() => {
  'use strict';

  const state = {
    ws: null,
    connected: false,
    sessionCode: '',
    sessionName: '',
    username: localStorage.getItem('collab-username') || '',
    password: '',
    serverUrl: (() => {
      // Check correct config path first (collaboration.serverUrl)
      const cfg = window.YAZE_CONFIG?.collaboration?.serverUrl ||
                  window.YAZE_CONFIG?.collaborationServerUrl || ''; // Legacy fallback
      if (cfg) return cfg;
      const meta = document.querySelector('meta[name="yaze-collab-server"]');
      if (meta && meta.content) return meta.content;
      const saved = localStorage.getItem('collab-server-url');
      return saved || '';
    })(),
    consoleVisible: false,
    chatVisible: false,
    docked: true,
  };

  let ui = {};

  function logConsole(line) {
    if (!ui.consoleBody) return;
    const ts = new Date().toLocaleTimeString();
    const div = document.createElement('div');
    div.textContent = `[${ts}] ${line}`;
    ui.consoleBody.appendChild(div);
    ui.consoleBody.scrollTop = ui.consoleBody.scrollHeight;
  }

  function appendMsg(kind, sender, text) {
    if (!ui.chatBody) return;
    const row = document.createElement('div');
    row.className = `yaze-msg ${kind}`;
    const meta = document.createElement('div');
    meta.className = 'meta';
    meta.textContent = sender;
    const body = document.createElement('div');
    body.className = 'text';
    body.textContent = text;
    row.appendChild(meta);
    row.appendChild(body);
    ui.chatBody.appendChild(row);
    ui.chatBody.scrollTop = ui.chatBody.scrollHeight;
  }

  function setStatus() {
    if (!ui.status) return;
    const locked = !!state.password;
    const text = state.connected
      ? `Room ${state.sessionCode || '(unknown)'}`
      : 'Disconnected';
    ui.status.className = `yaze-badge ${locked ? 'locked' : 'unlocked'}`;
    const icon = ui.status.querySelector('.status-icon');
    const label = ui.status.querySelector('.status-text');
    if (icon) icon.textContent = locked ? 'lock' : 'lock_open';
    if (label) label.textContent = text;
  }

  function disconnect(reason = 'Client request') {
    if (state.ws) {
      try {
        state.ws.close();
      } catch (_) {}
    }
    state.ws = null;
    state.connected = false;
    setStatus();
    logConsole(`Disconnected: ${reason}`);
  }

  function connect() {
    if (!state.serverUrl) {
      alert('Set server URL first');
      return;
    }
    disconnect('Reconnecting');
    try {
      state.ws = new WebSocket(state.serverUrl);
      state.ws.onopen = () => {
        state.connected = true;
        setStatus();
        logConsole('Connected');
        const mode = ui.mode.value || 'join';
        if (mode === 'host') {
          sendHost();
        } else {
          sendJoin();
        }
      };
      state.ws.onmessage = (e) => handleServer(JSON.parse(e.data));
      state.ws.onclose = () => {
        state.connected = false;
        setStatus();
        logConsole('Connection closed');
      };
      state.ws.onerror = (err) => {
        logConsole(`Error: ${err?.message || err}`);
      };
    } catch (e) {
      alert(`Connect failed: ${e}`);
    }
  }

  function send(msg) {
    if (!state.ws || state.ws.readyState !== WebSocket.OPEN) {
      logConsole('Not connected');
      return;
    }
    state.ws.send(JSON.stringify(msg));
  }

  function sendHost() {
    const payload = {
      session_name: ui.sessionName.value || 'YAZE Session',
      username: ui.username.value || 'anon',
      rom_hash: null,
      ai_enabled: false,
      session_password: ui.password.value || undefined,
    };
    send({ type: 'host_session', payload });
  }

  function sendJoin() {
    const payload = {
      session_code: (ui.sessionCode.value || '').toUpperCase(),
      username: ui.username.value || 'anon',
      session_password: ui.password.value || undefined,
    };
    send({ type: 'join_session', payload });
  }

  function sendChat() {
    const text = ui.chatInput.value.trim();
    if (!text) return;
    const isAi = text.startsWith('/ai') || text.startsWith('/ask');
    if (isAi) {
      const prompt = text.replace(/^(\/ai|\/ask)\s*/, '').trim();
      if (!prompt) {
        appendMsg('system', 'error', 'Usage: /ai <prompt>');
        return;
      }

      // Check if we are the host
      const isHost = (ui.mode.value === 'host');

      if (isHost) {
        // Host: Execute locally and broadcast result
        ui.chatInput.value = '';
        appendMsg('system', 'system', 'AI thinking (Host)...');
        callAiAndBroadcast(prompt);
      } else {
        // Client: Send request to host
        ui.chatInput.value = '';
        appendMsg('system', 'system', 'Requesting AI response from Host...');
        send({
          type: 'ai_request',
          payload: {
            sender: state.username || 'anon',
            prompt: prompt
          }
        });
      }
      return;
    }

    send({
      type: 'chat_message',
      payload: {
        sender: state.username || 'anon',
        message: text,
        message_type: 'chat',
      },
    });
    ui.chatInput.value = '';
  }

  function getStoredApiConfig() {
    const stores = [sessionStorage, localStorage];
    let openaiKey = '';
    let geminiKey = '';
    let provider = '';
    let model = '';
    let apiBase = '';
    for (const s of stores) {
      try {
        openaiKey = openaiKey || s.getItem('z3ed_openai_api_key') || s.getItem('OPENAI_API_KEY') || '';
        geminiKey = geminiKey || s.getItem('z3ed_gemini_api_key') || s.getItem('GEMINI_API_KEY') || '';
        provider = provider || s.getItem('yaze_ai_provider') || '';
        model = model || s.getItem('yaze_ai_model') || '';
        apiBase = apiBase || s.getItem('yaze_openai_base_url') || '';
      } catch (_) {}
    }
    if (provider === 'openai') {
      return {
        provider: 'openai',
        key: openaiKey,
        model: model || 'gpt-4o-mini',
        api_base: apiBase || 'https://api.openai.com/v1'
      };
    }
    if (provider === 'gemini') {
      return {
        provider: 'gemini',
        key: geminiKey,
        model: model || 'gemini-1.5-flash'
      };
    }
    if (openaiKey) return { provider: 'openai', key: openaiKey, model: 'gpt-4o-mini', api_base: apiBase || 'https://api.openai.com/v1' };
    if (geminiKey) return { provider: 'gemini', key: geminiKey, model: 'gemini-1.5-flash' };
    return null;
  }

  async function callAi(prompt) {
    // Try to use central AiManager first (handles BYOK and refresh tokens)
    if (window.yaze && window.yaze.ai) {
      try {
        return await window.yaze.ai.generateContent(prompt);
      } catch (e) {
        console.warn('AiManager failed, falling back to local config:', e);
        // Fallthrough to local logic
      }
    }

    const cfg = getStoredApiConfig();
    if (!cfg) throw new Error('No AI key set. Add GEMINI_API_KEY or OPENAI_API_KEY in browser storage.');

    if (cfg.provider === 'openai') {
      if (!cfg.key && (!cfg.api_base || cfg.api_base === 'https://api.openai.com/v1')) {
        throw new Error('OpenAI API key missing. Use a local OpenAI-compatible endpoint or set a key.');
      }
      const body = {
        model: cfg.model,
        messages: [
          { role: 'system', content: 'You are the YAZE collab AI assistant. Keep replies concise and actionable.' },
          { role: 'user', content: prompt },
        ],
        max_tokens: 256,
        temperature: 0.5,
      };
      const headers = { 'Content-Type': 'application/json' };
      if (cfg.key) {
        headers.Authorization = 'Bearer ' + cfg.key;
      }
      const resp = await fetch((cfg.api_base || 'https://api.openai.com/v1') + '/chat/completions', {
        method: 'POST',
        headers,
        body: JSON.stringify(body),
      });
      if (!resp.ok) throw new Error(`OpenAI error ${resp.status}`);
      const json = await resp.json();
      const choice = json?.choices?.[0];
      const text = choice?.message?.content || '';
      if (!text) throw new Error('OpenAI returned empty response');
      return text;
    }

    // Gemini default
    const url = `https://generativelanguage.googleapis.com/v1beta/models/${cfg.model}:generateContent?key=${cfg.key}`;
    const body = {
      contents: [
        {
          parts: [
            {
              text:
                'You are the YAZE collab AI assistant. Keep replies concise and actionable.\n\n' +
                prompt,
            },
          ],
        },
      ],
      generationConfig: {
        temperature: 0.5,
        maxOutputTokens: 256,
      },
    };
    const resp = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    if (!resp.ok) throw new Error(`Gemini error ${resp.status}`);
    const json = await resp.json();
    const text =
      json?.candidates?.[0]?.content?.parts?.map((p) => p.text).join('') || '';
    if (!text) throw new Error('Gemini returned empty response');
    return text;
  }

  async function callAiAndBroadcast(prompt) {
    try {
      const reply = await callAi(prompt);
      appendMsg('chat', 'AI', reply);
      send({
        type: 'chat_message',
        payload: {
          sender: 'AI',
          message: reply,
          message_type: 'chat',
        },
      });
    } catch (err) {
      appendMsg('system', 'error', `AI error: ${err.message || err}`);
    }
  }

  function handleServer(msg) {
    switch (msg.type) {
      case 'session_hosted':
        state.sessionCode = msg.payload.session_code;
        state.sessionName = msg.payload.session_name;
        state.username = msg.payload.participants?.[0] || state.username;
        setStatus();
        appendMsg('system', 'system', `Hosted ${state.sessionCode} (${state.sessionName})`);
        break;
      case 'session_joined':
        state.sessionCode = msg.payload.session_code;
        state.sessionName = msg.payload.session_name;
        appendMsg(
          'system',
          'system',
          `Joined ${state.sessionCode} (${state.sessionName}) participants: ${msg.payload.participants.join(
            ', '
          )}`
        );
        setStatus();
        break;
      case 'chat_message':
        appendMsg('chat', msg.payload.sender, msg.payload.message || '');
        break;
      case 'ai_request':
        // Received a request for AI processing
        // Only the Host should respond to this
        if (ui.mode.value === 'host') {
          const prompt = msg.payload.prompt;
          const requestor = msg.payload.sender;
          logConsole(`AI Request from ${requestor}: ${prompt}`);
          // Acknowledge receipt
          send({
            type: 'chat_message',
            payload: {
              sender: 'System',
              message: `Processing AI request from ${requestor}...`,
              message_type: 'system'
            }
          });
          // Execute and broadcast
          callAiAndBroadcast(prompt);
        }
        break;
      case 'participant_joined':
      case 'participant_left':
        appendMsg(
          'system',
          'system',
          `${msg.payload.username} ${msg.type === 'participant_joined' ? 'joined' : 'left'}`
        );
        break;
      case 'error':
        appendMsg('system', 'error', msg.payload?.error || 'Unknown error');
        break;
      case 'server_shutdown':
        appendMsg('system', 'server', 'Server shutting down');
        break;
      default:
        logConsole(`Unhandled: ${msg.type}`);
    }
  }

  let container = null;

  // Global toggle implementation (called by app.js orchestrator)
  window.toggleCollabConsole_impl = function() {
    if (!container) return;
    state.chatVisible = !state.chatVisible;
    container.classList.toggle('active', state.chatVisible);
    document.body.classList.toggle('yaze-console-open', state.chatVisible);
  };

  function buildUI() {
    container = document.createElement('div');
    container.className = 'yaze-console-container';

    // Chat pane
    const chatPane = document.createElement('div');
    chatPane.className = 'yaze-chat-pane';

    const header = document.createElement('div');
    header.className = 'yaze-chat-header';
    header.innerHTML = `
      <label class="yaze-field">
        <span class="material-symbols-outlined">link</span>
        <input id="yaze-server-url" placeholder="ws://server:8765" />
      </label>
      <label class="yaze-field">
        <span class="material-symbols-outlined">qr_code</span>
        <input id="yaze-session-code" placeholder="Room code (ABC123)" maxlength="6" style="text-transform: uppercase;" />
      </label>
      <label class="yaze-field">
        <span class="material-symbols-outlined">meeting_room</span>
        <input id="yaze-session-name" placeholder="Session name (host)" />
      </label>
      <label class="yaze-field">
        <span class="material-symbols-outlined">person</span>
        <input id="yaze-username" placeholder="Username" />
      </label>
      <label class="yaze-field">
        <span class="material-symbols-outlined">vpn_key</span>
        <input id="yaze-password" type="password" placeholder="Password (optional)" />
      </label>
      <select id="yaze-mode">
        <option value="join">Join</option>
        <option value="host">Host</option>
      </select>
      <button id="yaze-connect" class="yaze-icon-btn"><span class="material-symbols-outlined">power</span><span>Connect</span></button>
      <button id="yaze-disconnect" class="yaze-icon-btn subtle"><span class="material-symbols-outlined">link_off</span><span>Disconnect</span></button>
      <div id="yaze-status" class="yaze-badge unlocked">
        <span class="material-symbols-outlined status-icon">lock_open</span>
        <span class="status-text">Disconnected</span>
      </div>
    `;

    const body = document.createElement('div');
    body.className = 'yaze-chat-body';

    const inputRow = document.createElement('div');
    inputRow.className = 'yaze-chat-input';
    inputRow.innerHTML = `
      <textarea id="yaze-chat-input" placeholder="Type a message. Enter to send, Shift+Enter for newline."></textarea>
      <button id="yaze-chat-ai" class="yaze-icon-btn subtle" title="Ask AI (/ai prompt)"><span class="material-symbols-outlined">psychology</span></button>
      <button id="yaze-chat-send" class="yaze-icon-btn primary"><span class="material-symbols-outlined">send</span></button>
    `;

    chatPane.appendChild(header);
    chatPane.appendChild(body);
    chatPane.appendChild(inputRow);

    // Console pane
    const consolePane = document.createElement('div');
    consolePane.className = 'yaze-console-pane';
    const consoleHeader = document.createElement('div');
    consoleHeader.className = 'yaze-console-header';
    consoleHeader.innerHTML = `
      <div class="title"><span class="material-symbols-outlined">terminal</span>Collab Console</div>
      <div class="controls">
        <button id="yaze-console-toggle" class="yaze-icon-btn subtle"><span class="material-symbols-outlined">visibility_off</span><span>Hide</span></button>
        <button id="yaze-console-clear" class="yaze-icon-btn subtle"><span class="material-symbols-outlined">backspace</span><span>Clear</span></button>
      </div>
    `;
    const consoleBody = document.createElement('div');
    consoleBody.className = 'yaze-console-body';
    consolePane.appendChild(consoleHeader);
    consolePane.appendChild(consoleBody);

    container.appendChild(chatPane);
    container.appendChild(consolePane);
    document.body.appendChild(container);

    // Wire refs
    ui = {
      container,
      chatBody: body,
      chatInput: inputRow.querySelector('#yaze-chat-input'),
      chatAI: inputRow.querySelector('#yaze-chat-ai'),
      chatSend: inputRow.querySelector('#yaze-chat-send'),
      serverUrl: header.querySelector('#yaze-server-url'),
      sessionCode: header.querySelector('#yaze-session-code'),
      sessionName: header.querySelector('#yaze-session-name'),
      username: header.querySelector('#yaze-username'),
      password: header.querySelector('#yaze-password'),
      mode: header.querySelector('#yaze-mode'),
      connectBtn: header.querySelector('#yaze-connect'),
      disconnectBtn: header.querySelector('#yaze-disconnect'),
      status: header.querySelector('#yaze-status'),
      consolePane,
      consoleBody,
      consoleToggle: consoleHeader.querySelector('#yaze-console-toggle'),
      consoleClear: consoleHeader.querySelector('#yaze-console-clear'),
    };

    ui.serverUrl.value = state.serverUrl;
    ui.username.value = state.username;

    ui.connectBtn.addEventListener('click', () => {
      state.serverUrl = ui.serverUrl.value.trim();
      localStorage.setItem('collab-server-url', state.serverUrl);
      state.username = ui.username.value.trim();
      state.password = ui.password.value.trim();
      state.sessionCode = ui.sessionCode.value.trim();
      state.sessionName = ui.sessionName.value.trim();
      connect();
    });

    ui.disconnectBtn.addEventListener('click', () => disconnect('User'));

    ui.chatSend.addEventListener('click', sendChat);
    if (ui.chatAI) {
      ui.chatAI.addEventListener('click', () => {
        const prompt = ui.chatInput.value.trim();
        if (!prompt) {
          appendMsg('system', 'error', 'Enter a prompt to ask the AI.');
          return;
        }
        ui.chatInput.value = `/ai ${prompt}`;
        sendChat();
      });
    }
    ui.chatInput.addEventListener('keydown', (e) => {
      // Keep input local but allow default typing
      e.stopPropagation();

      if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        sendChat();
      }
    }, true);
    ui.consoleToggle.addEventListener('click', () => {
      state.consoleVisible = !state.consoleVisible;
      ui.consolePane.style.display = state.consoleVisible ? 'flex' : 'none';
      ui.consoleToggle.textContent = state.consoleVisible ? 'Hide Log' : 'Show Log';
    });

    ui.consoleClear.addEventListener('click', () => {
      ui.consoleBody.innerHTML = '';
    });

    // Default visibility: hidden until toggled
    ui.consolePane.style.display = 'flex';
    state.consoleVisible = true;
    setStatus();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', buildUI);
  } else {
    buildUI();
  }
})();
