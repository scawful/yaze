/**
 * @file ai_manager.js
 * @brief Central manager for AI service authentication and requests
 *
 * Handles Google OAuth2 Device Flow for "Bring Your Own Key" (BYOK) scenarios,
 * specifically targeting users who want to use their Gemini CLI / Google One quotas.
 */

(function() {
  'use strict';

  // Wait for namespace
  if (!window.yaze) window.yaze = {};
  if (!window.yaze.core) window.yaze.core = {};

  class AiManager {
    constructor() {
      this.config = window.YAZE_CONFIG ? window.YAZE_CONFIG.ai : {};
      this.authConfig = this.config.auth || {};
      this.storageKeys = (window.yaze && window.yaze.core && window.yaze.core.constants)
        ? window.yaze.core.constants.storageKeys
        : {};
      
      // Check for stored Client ID override
      const storedClientId = localStorage.getItem('yaze_ai_client_id');
      if (storedClientId) {
        this.authConfig.clientId = storedClientId;
      }
      
      // Storage keys
      this.STORAGE_KEY_TOKENS = 'yaze_ai_tokens';
      this.STORAGE_KEY_API_KEY = this.storageKeys.GEMINI_API_KEY || 'z3ed_gemini_api_key';
      this.STORAGE_KEY_OPENAI_KEY = this.storageKeys.OPENAI_API_KEY || 'z3ed_openai_api_key';
      this.STORAGE_KEY_PROVIDER = this.storageKeys.AI_PROVIDER || 'yaze_ai_provider';
      this.STORAGE_KEY_MODEL = this.storageKeys.AI_MODEL || 'yaze_ai_model';
      this.STORAGE_KEY_OPENAI_BASE = this.storageKeys.OPENAI_BASE_URL || 'yaze_openai_base_url';
      
      this.tokens = this.loadTokens();
      this.deviceCodeData = null; // Temp storage for polling
      this.pollInterval = null;

      this.reloadConfigFromStorage();
    }

    /**
     * Load tokens from local storage
     */
    loadTokens() {
      try {
        const stored = localStorage.getItem(this.STORAGE_KEY_TOKENS);
        if (stored) {
          return JSON.parse(stored);
        }
      } catch (e) {
        console.warn('[AiManager] Failed to load tokens:', e);
      }
      return null;
    }

    /**
     * Save tokens to local storage
     */
    saveTokens(tokens) {
      this.tokens = tokens;
      // Add timestamp if missing for expiry calculation
      if (!this.tokens.created_at) {
        this.tokens.created_at = Date.now();
      }
      localStorage.setItem(this.STORAGE_KEY_TOKENS, JSON.stringify(this.tokens));
      console.log('[AiManager] Tokens saved');
    }

    /**
     * Clear all auth data
     */
    logout() {
      this.tokens = null;
      localStorage.removeItem(this.STORAGE_KEY_TOKENS);
      this.stopPolling();
      console.log('[AiManager] Logged out');
    }

    /**
     * Get the best available credential (Token > API Key)
     */
    getCredential() {
      const provider = this.config.provider || 'gemini';
      if (provider === 'openai') {
        const openaiKey = this.getStoredValue(this.STORAGE_KEY_OPENAI_KEY);
        if (openaiKey) {
          return { type: 'Bearer', value: openaiKey };
        }
        return null;
      }

      // 1. Check for OAuth Access Token
      if (this.tokens && this.tokens.access_token) {
        // TODO: Check expiry and refresh if needed
        return { type: 'Bearer', value: this.tokens.access_token };
      }

      // 2. Check for API Key (legacy/simple)
      const apiKey = sessionStorage.getItem(this.STORAGE_KEY_API_KEY) || 
                     localStorage.getItem(this.STORAGE_KEY_API_KEY);
      if (apiKey) {
        return { type: 'key', value: apiKey };
      }

      return null;
    }

    getStoredValue(key) {
      if (!key) return '';
      let value = '';
      try {
        value = sessionStorage.getItem(key) || localStorage.getItem(key) || '';
      } catch (e) {
        console.warn('[AiManager] Failed to read storage key:', key, e);
      }
      return value || '';
    }

    normalizeOpenAiBaseUrl(base) {
      const fallback = 'https://api.openai.com/v1';
      if (!base) return fallback;
      let normalized = base.trim().replace(/\/+$/, '');
      if (!normalized.endsWith('/v1')) {
        normalized += '/v1';
      }
      return normalized;
    }

    reloadConfigFromStorage() {
      const provider = this.getStoredValue(this.STORAGE_KEY_PROVIDER);
      const model = this.getStoredValue(this.STORAGE_KEY_MODEL);
      const openaiBase = this.getStoredValue(this.STORAGE_KEY_OPENAI_BASE);

      if (provider) this.config.provider = provider;
      if (model) this.config.model = model;
      if (openaiBase) this.config.openaiBaseUrl = openaiBase;

      if (!this.config.provider) {
        this.config.provider = 'gemini';
      }
      if (!this.config.model) {
        this.config.model = this.config.provider === 'openai' ? 'gpt-4o-mini' : 'gemini-2.5-flash';
      }
      if (!this.config.openaiBaseUrl) {
        this.config.openaiBaseUrl = 'https://api.openai.com/v1';
      }
    }

    getConfigSnapshot() {
      return {
        provider: this.config.provider || 'gemini',
        model: this.config.model || '',
        openaiBaseUrl: this.normalizeOpenAiBaseUrl(this.config.openaiBaseUrl || ''),
      };
    }

    /**
     * Start the Device Authorization Flow
     * Returns an object with verification_url and user_code to display to the user.
     */
    async startDeviceAuth() {
      const clientId = this.authConfig.clientId;
      if (!clientId) {
        throw new Error('Client ID not configured. Please set window.YAZE_CONFIG.ai.auth.clientId.');
      }

      const endpoint = this.authConfig.deviceEndpoint || 'https://oauth2.googleapis.com/device/code';
      const scope = this.authConfig.scopes || 'https://www.googleapis.com/auth/generative-language.retriever';

      console.log('[AiManager] Starting Device Auth with Client ID:', clientId);

      const params = new URLSearchParams();
      params.append('client_id', clientId);
      params.append('scope', scope);

      try {
        const response = await fetch(endpoint, {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: params
        });

        if (!response.ok) {
          const errText = await response.text();
          throw new Error(`Device auth request failed: ${response.status} ${errText}`);
        }

        const data = await response.json();
        // Expected response:
        // {
        //   "device_code": "...",
        //   "user_code": "...",
        //   "verification_url": "...",
        //   "expires_in": 1800,
        //   "interval": 5
        // }
        
        this.deviceCodeData = data;
        return {
          user_code: data.user_code,
          verification_url: data.verification_url,
          expires_in: data.expires_in
        };

      } catch (e) {
        console.error('[AiManager] Auth error:', e);
        throw e;
      }
    }

    /**
     * Poll for the token after user has been shown the code.
     * Returns a promise that resolves when auth is complete.
     */
    async pollForToken() {
      if (!this.deviceCodeData) {
        throw new Error('No pending device auth session.');
      }

      const { device_code, interval } = this.deviceCodeData;
      const clientId = this.authConfig.clientId;
      const tokenEndpoint = this.authConfig.tokenEndpoint || 'https://oauth2.googleapis.com/token';
      const pollIntervalMs = (interval || 5) * 1000;

      return new Promise((resolve, reject) => {
        this.stopPolling(); // Clear existing

        this.pollInterval = setInterval(async () => {
          try {
            const params = new URLSearchParams();
            params.append('client_id', clientId);
            params.append('device_code', device_code);
            params.append('grant_type', 'urn:ietf:params:oauth:grant-type:device_code');

            const response = await fetch(tokenEndpoint, {
              method: 'POST',
              headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
              body: params
            });

            const data = await response.json();

            if (response.ok) {
              // Success!
              this.stopPolling();
              this.saveTokens(data);
              resolve(data);
            } else {
              // Handle standard polling errors
              if (data.error === 'authorization_pending') {
                // Keep waiting
              } else if (data.error === 'slow_down') {
                // We should increase interval, but for simplicity we just ignore this poll
              } else if (data.error === 'expired_token') {
                this.stopPolling();
                reject(new Error('Auth session expired. Please try again.'));
              } else {
                this.stopPolling();
                reject(new Error(`Auth error: ${data.error}`));
              }
            }
          } catch (e) {
            this.stopPolling();
            reject(e);
          }
        }, pollIntervalMs);
      });
    }

    stopPolling() {
      if (this.pollInterval) {
        clearInterval(this.pollInterval);
        this.pollInterval = null;
      }
    }

    /**
     * Process a request from the C++ Agent Service
     * @param {string} historyJson - JSON string of chat history
     */
    async processAgentRequest(historyJson) {
      console.log('[AiManager] Processing agent request from C++');
      
      try {
        const history = JSON.parse(historyJson);
        
        // Construct prompt from history
        // Simple concatenation for now
        let fullPrompt = "";
        
        // System prompt injection (if needed)
        // fullPrompt += "You are a helpful assistant...\n";
        
        for (const msg of history) {
          const role = msg.role === 'user' ? 'User' : 'Model';
          const text = msg.parts[0].text;
          fullPrompt += `${role}: ${text}\n`;
        }
        fullPrompt += "Model:";
        
        // Call Gemini
        const responseText = await this.generateContent(fullPrompt);
        
        // TODO: Real function calling parsing
        // For now, we just return text. 
        // To support tools, we'd need to parse structured output or use Gemini's function calling API.
        
        // Construct response object for C++
        const response = {
          text: responseText,
          tool_calls: [] // No tools for now in this simple text-based pass
        };
        
        // Send back to C++
        if (window.Module && window.Module.onExternalAiResponse) {
          window.Module.onExternalAiResponse(JSON.stringify(response));
        } else {
          console.warn('[AiManager] Module.onExternalAiResponse not available');
        }
        
      } catch (e) {
        console.error('[AiManager] Agent request failed:', e);
        // Send error back?
        const errorResponse = {
          text: `Error: ${e.message}`,
          tool_calls: []
        };
        if (window.Module && window.Module.onExternalAiResponse) {
          window.Module.onExternalAiResponse(JSON.stringify(errorResponse));
        }
      }
    }

    /**
     * Generate content using the Gemini API
     * @param {string} prompt The user's prompt
     * @returns {Promise<string>} The generated text
     */
    async generateContent(prompt) {
      const provider = this.config.provider || 'gemini';
      const maxTokens = this.config.maxResponseLength || 4096;

      if (provider === 'openai') {
        const openaiKey = this.getStoredValue(this.STORAGE_KEY_OPENAI_KEY);
        const baseUrl = this.normalizeOpenAiBaseUrl(this.config.openaiBaseUrl || '');
        if (!openaiKey && baseUrl === 'https://api.openai.com/v1') {
          throw new Error('OpenAI API key missing. Set one or use a local OpenAI-compatible endpoint.');
        }
        const headers = { 'Content-Type': 'application/json' };
        if (openaiKey) {
          headers['Authorization'] = `Bearer ${openaiKey}`;
        }
        const body = {
          model: this.config.model || 'gpt-4o-mini',
          messages: [
            { role: 'system', content: 'You are the YAZE web assistant. Keep replies concise and actionable.' },
            { role: 'user', content: prompt }
          ],
          max_tokens: Math.min(maxTokens, 4096),
          temperature: 0.5
        };
        const response = await fetch(`${baseUrl}/chat/completions`, {
          method: 'POST',
          headers,
          body: JSON.stringify(body)
        });
        if (!response.ok) {
          const errText = await response.text();
          throw new Error(`OpenAI API Error (${response.status}): ${errText}`);
        }
        const json = await response.json();
        const choice = json?.choices?.[0];
        const text = choice?.message?.content || '';
        return text || '(No content returned)';
      }

      const cred = this.getCredential();
      if (!cred) {
        throw new Error('No credentials available. Please use /login or /apikey.');
      }

      const model = this.config.model || 'gemini-2.5-flash';

      // Construct URL based on auth type
      let url = `https://generativelanguage.googleapis.com/v1beta/models/${model}:generateContent`;
      const headers = { 'Content-Type': 'application/json' };

      if (cred.type === 'key') {
        url += `?key=${cred.value}`;
      } else if (cred.type === 'Bearer') {
        headers['Authorization'] = `Bearer ${cred.value}`;
      }

      const body = {
        contents: [{
          parts: [{ text: prompt }]
        }],
        generationConfig: {
          maxOutputTokens: maxTokens
        }
      };

      try {
        const response = await fetch(url, {
          method: 'POST',
          headers: headers,
          body: JSON.stringify(body)
        });

        if (!response.ok) {
          const errText = await response.text();
          throw new Error(`Gemini API Error (${response.status}): ${errText}`);
        }

        const json = await response.json();

        // Parse standard Gemini response
        if (json.candidates && json.candidates.length > 0) {
          const parts = json.candidates[0].content.parts;
          return parts.map(p => p.text).join('');
        }

        return '(No content returned)';

      } catch (e) {
        console.error('[AiManager] Generate error:', e);
        throw e;
      }
    }
  }

  // Export
  window.yaze.core.AiManager = AiManager;
  
  // Create singleton instance
  window.yaze.ai = new AiManager();
  console.log('[AiManager] Initialized');

})();
