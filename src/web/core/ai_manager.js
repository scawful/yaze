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
      this.aiConstants = (window.yaze && window.yaze.core &&
                          window.yaze.core.constants &&
                          window.yaze.core.constants.ai)
        ? window.yaze.core.constants.ai
        : {};
      this.providerIds = this.aiConstants.providers || {};
      this.endpointDefaults = this.aiConstants.endpoints || {};
      this.defaultModels = this.aiConstants.defaultModels || {};
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
      this.PROVIDERS = {
        AUTO: this.providerIds.AUTO || 'auto',
        GEMINI: this.providerIds.GEMINI || 'gemini',
        OPENAI: this.providerIds.OPENAI || 'openai',
        LMSTUDIO: this.providerIds.LMSTUDIO || 'lmstudio',
        HALEXT: this.providerIds.HALEXT || 'halext'
      };
      this.OPENAI_BASES = {
        OPENAI: this.endpointDefaults.OPENAI || 'https://api.openai.com/v1',
        LMSTUDIO: this.endpointDefaults.LMSTUDIO || 'http://localhost:1234/v1',
        HALEXT: this.endpointDefaults.HALEXT || 'https://halext.org/v1'
      };
      this.DEFAULT_MODELS = {
        GEMINI: this.defaultModels.GEMINI || 'gemini-2.5-flash',
        OPENAI: this.defaultModels.OPENAI || 'gpt-4o-mini',
        LMSTUDIO: this.defaultModels.LMSTUDIO || '',
        HALEXT: this.defaultModels.HALEXT || ''
      };
      
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
    getCredential(providerOverride) {
      const provider = this.normalizeProvider(providerOverride || this.config.provider);
      if (this.isOpenAiCompatibleProvider(provider)) {
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

    normalizeProvider(provider) {
      const normalized = (provider || '').toString().trim().toLowerCase();
      if (!normalized) return this.PROVIDERS.GEMINI;
      if (normalized === 'lm-studio') return this.PROVIDERS.LMSTUDIO;
      if (normalized === 'google' || normalized === 'google-gemini') {
        return this.PROVIDERS.GEMINI;
      }
      if (normalized === 'afs-bridge') {
        return this.PROVIDERS.HALEXT;
      }
      if (normalized === 'openai-compatible' || normalized === 'custom-openai') {
        return this.PROVIDERS.OPENAI;
      }
      if (normalized === this.PROVIDERS.GEMINI ||
          normalized === this.PROVIDERS.OPENAI ||
          normalized === this.PROVIDERS.LMSTUDIO ||
          normalized === this.PROVIDERS.HALEXT) {
        return normalized;
      }
      return this.PROVIDERS.GEMINI;
    }

    isOpenAiCompatibleProvider(providerOverride) {
      const provider = this.normalizeProvider(providerOverride || this.config.provider);
      return provider === this.PROVIDERS.OPENAI ||
             provider === this.PROVIDERS.LMSTUDIO ||
             provider === this.PROVIDERS.HALEXT;
    }

    isLikelyLocalBaseUrl(baseUrl) {
      const lower = (baseUrl || '').toLowerCase();
      return lower.includes('localhost') || lower.includes('127.0.0.1') ||
             lower.includes('0.0.0.0') || lower.includes('::1');
    }

    getProviderDisplayName(providerOverride) {
      const provider = this.normalizeProvider(providerOverride || this.config.provider);
      switch (provider) {
        case this.PROVIDERS.OPENAI:
          return 'OpenAI';
        case this.PROVIDERS.LMSTUDIO:
          return 'LM Studio';
        case this.PROVIDERS.HALEXT:
          return 'halext AFS bridge';
        default:
          return 'Gemini';
      }
    }

    getProviderDefaults(providerOverride) {
      const provider = this.normalizeProvider(providerOverride || this.config.provider);
      if (provider === this.PROVIDERS.OPENAI) {
        return {
          provider,
          openaiBaseUrl: this.OPENAI_BASES.OPENAI,
          model: this.DEFAULT_MODELS.OPENAI,
          requiresApiKey: true,
          apiKeyLabel: 'OpenAI API key',
          modelPlaceholder: this.DEFAULT_MODELS.OPENAI
        };
      }
      if (provider === this.PROVIDERS.LMSTUDIO) {
        return {
          provider,
          openaiBaseUrl: this.OPENAI_BASES.LMSTUDIO,
          model: this.DEFAULT_MODELS.LMSTUDIO,
          requiresApiKey: false,
          apiKeyLabel: 'LM Studio token (optional)',
          modelPlaceholder: 'Refresh models or enter a local model id'
        };
      }
      if (provider === this.PROVIDERS.HALEXT) {
        return {
          provider,
          openaiBaseUrl: this.OPENAI_BASES.HALEXT,
          model: this.DEFAULT_MODELS.HALEXT,
          requiresApiKey: true,
          apiKeyLabel: 'halext bridge token',
          modelPlaceholder: 'Refresh models from halext.org'
        };
      }
      return {
        provider: this.PROVIDERS.GEMINI,
        openaiBaseUrl: this.OPENAI_BASES.OPENAI,
        model: this.DEFAULT_MODELS.GEMINI,
        requiresApiKey: true,
        apiKeyLabel: 'Gemini API key',
        modelPlaceholder: this.DEFAULT_MODELS.GEMINI
      };
    }

    normalizeOpenAiBaseUrl(base, providerOverride) {
      const defaults = this.getProviderDefaults(providerOverride || this.config.provider);
      const fallback = defaults.openaiBaseUrl || this.OPENAI_BASES.OPENAI;
      if (!base) return fallback;
      let normalized = base.trim().replace(/\/+$/, '');
      if (!normalized.endsWith('/v1')) {
        normalized += '/v1';
      }
      return normalized;
    }

    reloadConfigFromStorage() {
      const provider = this.normalizeProvider(this.getStoredValue(this.STORAGE_KEY_PROVIDER));
      const model = this.getStoredValue(this.STORAGE_KEY_MODEL);
      const openaiBase = this.getStoredValue(this.STORAGE_KEY_OPENAI_BASE);

      if (provider) this.config.provider = provider;
      if (model) this.config.model = model;
      if (openaiBase) this.config.openaiBaseUrl = openaiBase;

      if (!this.config.provider) {
        this.config.provider = this.PROVIDERS.GEMINI;
      }
      if (this.isOpenAiCompatibleProvider(this.config.provider)) {
        this.config.openaiBaseUrl = this.normalizeOpenAiBaseUrl(
          this.config.openaiBaseUrl || openaiBase,
          this.config.provider
        );
      }
      const defaults = this.getProviderDefaults(this.config.provider);
      if (!this.config.model) {
        this.config.model = defaults.model || '';
      }
      if (!this.config.openaiBaseUrl) {
        this.config.openaiBaseUrl = defaults.openaiBaseUrl || this.OPENAI_BASES.OPENAI;
      }
    }

    getConfigSnapshot() {
      const provider = this.normalizeProvider(this.config.provider || this.PROVIDERS.GEMINI);
      const defaults = this.getProviderDefaults(provider);
      return {
        provider,
        model: this.config.model || defaults.model || '',
        openaiBaseUrl: this.normalizeOpenAiBaseUrl(this.config.openaiBaseUrl || '', provider),
      };
    }

    buildCuratedModels(providerOverride, currentModel) {
      const provider = this.normalizeProvider(providerOverride || this.config.provider);
      if (!this.isOpenAiCompatibleProvider(provider)) {
        return [
          { name: 'gemini-2.5-flash', displayName: 'Gemini 2.5 Flash', local: false },
          { name: 'gemini-2.5-pro', displayName: 'Gemini 2.5 Pro', local: false },
          { name: 'gemini-1.5-flash', displayName: 'Gemini 1.5 Flash', local: false }
        ];
      }

      const models = [];
      if (currentModel) {
        models.push({ name: currentModel, displayName: currentModel, local: provider === this.PROVIDERS.LMSTUDIO });
      }
      if (provider === this.PROVIDERS.OPENAI) {
        models.push(
          { name: 'gpt-4o-mini', displayName: 'GPT-4o Mini', local: false },
          { name: 'gpt-4o', displayName: 'GPT-4o', local: false },
          { name: 'gpt-4.1-mini', displayName: 'GPT-4.1 Mini', local: false }
        );
      }
      return models;
    }

    async listAvailableModels(overrides = {}) {
      const provider = this.normalizeProvider(overrides.provider || this.config.provider);
      const explicitModel = (overrides.model || '').trim();
      if (!this.isOpenAiCompatibleProvider(provider)) {
        return this.buildCuratedModels(provider, explicitModel || this.config.model);
      }

      const baseUrl = this.normalizeOpenAiBaseUrl(
        overrides.openaiBaseUrl || this.config.openaiBaseUrl || '',
        provider
      );
      const openaiKey = overrides.openaiApiKey !== undefined
        ? overrides.openaiApiKey
        : this.getStoredValue(this.STORAGE_KEY_OPENAI_KEY);
      if (provider === this.PROVIDERS.OPENAI && !openaiKey &&
          baseUrl === this.OPENAI_BASES.OPENAI) {
        return this.buildCuratedModels(provider, explicitModel || this.config.model);
      }

      const headers = { 'Content-Type': 'application/json' };
      if (openaiKey) {
        headers.Authorization = `Bearer ${openaiKey}`;
      }

      const response = await fetch(`${baseUrl}/models`, {
        method: 'GET',
        headers
      });
      if (!response.ok) {
        throw new Error(`Model discovery failed (${response.status})`);
      }

      const json = await response.json();
      const items = Array.isArray(json && json.data) ? json.data : [];
      const models = items
        .filter(entry => entry && typeof entry.id === 'string' && entry.id)
        .map(entry => ({
          name: entry.id,
          displayName: entry.id,
          provider,
          local: this.isLikelyLocalBaseUrl(baseUrl),
          owner: typeof entry.owned_by === 'string' ? entry.owned_by : ''
        }));
      if (!models.length) {
        return this.buildCuratedModels(provider, explicitModel || this.config.model);
      }
      return models;
    }

    async resolveModel(overrides = {}) {
      const provider = this.normalizeProvider(overrides.provider || this.config.provider);
      const explicitModel = (overrides.model || this.config.model || '').trim();
      if (explicitModel) {
        return explicitModel;
      }
      const defaults = this.getProviderDefaults(provider);
      if (defaults.model) {
        return defaults.model;
      }
      const models = await this.listAvailableModels(overrides);
      if (models.length > 0) {
        const resolved = models[0].name || '';
        if (resolved) {
          this.config.model = resolved;
        }
        return resolved;
      }
      return '';
    }

    async getWasmBridgeConfig(overrides = {}) {
      const snapshot = this.getConfigSnapshot();
      const provider = this.normalizeProvider(overrides.provider || snapshot.provider);
      const openaiApiKey = overrides.openaiApiKey !== undefined
        ? overrides.openaiApiKey
        : this.getStoredValue(this.STORAGE_KEY_OPENAI_KEY);
      const geminiApiKey = overrides.geminiApiKey !== undefined
        ? overrides.geminiApiKey
        : this.getStoredValue(this.STORAGE_KEY_API_KEY);
      return {
        provider,
        model: await this.resolveModel({
          provider,
          model: overrides.model || snapshot.model,
          openaiBaseUrl: overrides.openaiBaseUrl || snapshot.openaiBaseUrl,
          openaiApiKey
        }),
        apiBase: this.isOpenAiCompatibleProvider(provider)
          ? this.normalizeOpenAiBaseUrl(overrides.openaiBaseUrl || snapshot.openaiBaseUrl || '', provider)
          : '',
        apiKey: this.isOpenAiCompatibleProvider(provider) ? openaiApiKey : geminiApiKey
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

    buildAgentDriverSystemPrompt() {
      return [
        'You are the YAZE browser AI driver.',
        'Return only valid JSON with no markdown fences or commentary.',
        'Schema:',
        '{"text":"optional reply","tool_calls":[{"tool_name":"tool_name","args":{"arg":"value"}}]}',
        'Use tool_calls when the next step should invoke a YAZE tool.',
        'Use concise text when no tool call is needed.',
        'Arguments must be valid JSON object values.',
        'If both are useful, include both text and tool_calls.'
      ].join('\n');
    }

    getHistoryMessageText(message) {
      if (!message || typeof message !== 'object') return '';
      if (typeof message.message === 'string' && message.message.trim()) {
        return message.message.trim();
      }
      if (!Array.isArray(message.parts)) {
        return '';
      }
      return message.parts
        .map(part => {
          if (typeof part === 'string') return part;
          if (part && typeof part.text === 'string') return part.text;
          return '';
        })
        .filter(Boolean)
        .join('\n')
        .trim();
    }

    buildOpenAiAgentMessages(history) {
      const messages = [{
        role: 'system',
        content: this.buildAgentDriverSystemPrompt()
      }];
      history.forEach(message => {
        const content = this.getHistoryMessageText(message);
        if (!content) return;
        messages.push({
          role: message && message.role === 'user' ? 'user' : 'assistant',
          content
        });
      });
      return messages;
    }

    buildGeminiAgentContents(history) {
      return history
        .map(message => {
          const text = this.getHistoryMessageText(message);
          if (!text) return null;
          return {
            role: message && message.role === 'user' ? 'user' : 'model',
            parts: [{ text }]
          };
        })
        .filter(Boolean);
    }

    normalizeStructuredToolCall(call) {
      if (!call || typeof call !== 'object') {
        return null;
      }

      const name = call.tool_name || call.name || '';
      if (!name || typeof name !== 'string') {
        return null;
      }

      const normalizedArgs = {};
      const args = call.args && typeof call.args === 'object' && !Array.isArray(call.args)
        ? call.args
        : {};
      Object.entries(args).forEach(([key, value]) => {
        if (value === undefined) {
          return;
        }
        if (typeof value === 'string' || typeof value === 'number' ||
            typeof value === 'boolean') {
          normalizedArgs[key] = value;
          return;
        }
        if (value === null) {
          normalizedArgs[key] = null;
          return;
        }
        try {
          normalizedArgs[key] = JSON.stringify(value);
        } catch (e) {
          normalizedArgs[key] = String(value);
        }
      });

      return {
        name,
        args: normalizedArgs
      };
    }

    dedupeStructuredToolCalls(toolCalls) {
      const seen = new Set();
      const deduped = [];
      (toolCalls || []).forEach(call => {
        const normalized = this.normalizeStructuredToolCall(call);
        if (!normalized) return;
        const signature = `${normalized.name}:${JSON.stringify(normalized.args)}`;
        if (seen.has(signature)) return;
        seen.add(signature);
        deduped.push(normalized);
      });
      return deduped;
    }

    normalizeStructuredAgentPayload(payload) {
      const normalized = {
        text: '',
        tool_calls: []
      };

      if (!payload || typeof payload !== 'object') {
        return normalized;
      }

      if (typeof payload.text === 'string') {
        normalized.text = payload.text;
      } else if (typeof payload.text_response === 'string') {
        normalized.text = payload.text_response;
      }

      if (Array.isArray(payload.tool_calls)) {
        normalized.tool_calls = this.dedupeStructuredToolCalls(payload.tool_calls);
      }

      return normalized;
    }

    tryParseStructuredAgentText(text) {
      if (typeof text !== 'string') {
        return null;
      }

      const trimmed = text.trim();
      if (!trimmed) {
        return null;
      }

      const candidates = [trimmed];
      const fencedMatch = trimmed.match(/^```(?:json)?\s*([\s\S]*?)\s*```$/i);
      if (fencedMatch && fencedMatch[1]) {
        candidates.push(fencedMatch[1].trim());
      }
      const firstBrace = trimmed.indexOf('{');
      const lastBrace = trimmed.lastIndexOf('}');
      if (firstBrace >= 0 && lastBrace > firstBrace) {
        candidates.push(trimmed.slice(firstBrace, lastBrace + 1));
      }

      for (const candidate of candidates) {
        try {
          const parsed = JSON.parse(candidate);
          if (parsed && typeof parsed === 'object' && !Array.isArray(parsed)) {
            return this.normalizeStructuredAgentPayload(parsed);
          }
        } catch (e) {
          // Keep trying fallback candidates.
        }
      }

      return null;
    }

    mergeStructuredToolCalls(...toolCallSets) {
      const merged = [];
      toolCallSets.forEach(toolCalls => {
        if (Array.isArray(toolCalls)) {
          merged.push(...toolCalls);
        }
      });
      return this.dedupeStructuredToolCalls(merged);
    }

    extractOpenAiContentText(content) {
      if (typeof content === 'string') {
        return content;
      }
      if (!Array.isArray(content)) {
        return '';
      }
      return content
        .map(part => {
          if (!part || typeof part !== 'object') return '';
          if (typeof part.text === 'string') return part.text;
          if (part.type === 'text' && typeof part.text === 'string') return part.text;
          return '';
        })
        .filter(Boolean)
        .join('\n');
    }

    async requestOpenAiStructuredAgentResponse(history, provider) {
      const openaiKey = this.getStoredValue(this.STORAGE_KEY_OPENAI_KEY);
      const baseUrl = this.normalizeOpenAiBaseUrl(this.config.openaiBaseUrl || '', provider);
      if (!openaiKey && baseUrl === this.OPENAI_BASES.OPENAI) {
        throw new Error('OpenAI API key missing. Set one or use a local OpenAI-compatible endpoint.');
      }

      const model = await this.resolveModel({
        provider,
        model: this.config.model,
        openaiBaseUrl: baseUrl,
        openaiApiKey: openaiKey
      });
      if (!model) {
        throw new Error(`No model selected for ${this.getProviderDisplayName(provider)}. Refresh models or enter a model id.`);
      }

      const headers = { 'Content-Type': 'application/json' };
      if (openaiKey) {
        headers.Authorization = `Bearer ${openaiKey}`;
      }

      const body = {
        model,
        messages: this.buildOpenAiAgentMessages(history),
        max_tokens: Math.min(this.config.maxResponseLength || 4096, 4096),
        temperature: 0.2
      };
      if (provider === this.PROVIDERS.OPENAI && baseUrl === this.OPENAI_BASES.OPENAI) {
        body.response_format = { type: 'json_object' };
      }

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
      const choice = json && Array.isArray(json.choices) ? json.choices[0] : null;
      const message = choice && choice.message ? choice.message : {};
      const textPayload = this.extractOpenAiContentText(message.content);
      const parsedPayload = this.tryParseStructuredAgentText(textPayload);
      const nativeToolCalls = Array.isArray(message.tool_calls)
        ? message.tool_calls.map(call => {
            const fn = call && call.function ? call.function : {};
            let parsedArgs = {};
            if (typeof fn.arguments === 'string' && fn.arguments.trim()) {
              try {
                parsedArgs = JSON.parse(fn.arguments);
              } catch (e) {
                parsedArgs = {};
              }
            }
            return {
              name: fn.name || '',
              args: parsedArgs
            };
          })
        : [];

      return {
        text: parsedPayload ? parsedPayload.text : textPayload,
        tool_calls: this.mergeStructuredToolCalls(
          nativeToolCalls,
          parsedPayload ? parsedPayload.tool_calls : []
        )
      };
    }

    async requestGeminiStructuredAgentResponse(history) {
      const credential = this.getCredential(this.PROVIDERS.GEMINI);
      if (!credential) {
        throw new Error('No credentials available. Please use /login or /apikey.');
      }

      const model = this.config.model || this.DEFAULT_MODELS.GEMINI || 'gemini-2.5-flash';
      let url = `https://generativelanguage.googleapis.com/v1beta/models/${model}:generateContent`;
      const headers = { 'Content-Type': 'application/json' };
      if (credential.type === 'key') {
        url += `?key=${credential.value}`;
      } else if (credential.type === 'Bearer') {
        headers.Authorization = `Bearer ${credential.value}`;
      }

      const body = {
        systemInstruction: {
          parts: [{ text: this.buildAgentDriverSystemPrompt() }]
        },
        contents: this.buildGeminiAgentContents(history),
        generationConfig: {
          maxOutputTokens: this.config.maxResponseLength || 4096,
          responseMimeType: 'application/json'
        }
      };

      const response = await fetch(url, {
        method: 'POST',
        headers,
        body: JSON.stringify(body)
      });
      if (!response.ok) {
        const errText = await response.text();
        throw new Error(`Gemini API Error (${response.status}): ${errText}`);
      }

      const json = await response.json();
      const candidate = json && Array.isArray(json.candidates) ? json.candidates[0] : null;
      const parts = candidate && candidate.content && Array.isArray(candidate.content.parts)
        ? candidate.content.parts
        : [];
      const textPayload = parts
        .map(part => (part && typeof part.text === 'string') ? part.text : '')
        .filter(Boolean)
        .join('\n');
      const parsedPayload = this.tryParseStructuredAgentText(textPayload);
      const nativeToolCalls = parts
        .filter(part => part && part.functionCall && typeof part.functionCall.name === 'string')
        .map(part => ({
          name: part.functionCall.name,
          args: part.functionCall.args || {}
        }));

      return {
        text: parsedPayload ? parsedPayload.text : textPayload,
        tool_calls: this.mergeStructuredToolCalls(
          nativeToolCalls,
          parsedPayload ? parsedPayload.tool_calls : []
        )
      };
    }

    async generateStructuredAgentResponse(history) {
      const provider = this.normalizeProvider(this.config.provider || this.PROVIDERS.GEMINI);
      if (this.isOpenAiCompatibleProvider(provider)) {
        return this.requestOpenAiStructuredAgentResponse(history, provider);
      }
      return this.requestGeminiStructuredAgentResponse(history);
    }

    sendExternalAiResponse(response) {
      const payload = this.normalizeStructuredAgentPayload(response);
      if (window.Module && window.Module.onExternalAiResponse) {
        window.Module.onExternalAiResponse(JSON.stringify(payload));
      } else {
        console.warn('[AiManager] Module.onExternalAiResponse not available');
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
        const response = await this.generateStructuredAgentResponse(
          Array.isArray(history) ? history : []
        );
        this.sendExternalAiResponse(response);
      } catch (e) {
        console.error('[AiManager] Agent request failed:', e);
        this.sendExternalAiResponse({
          text: `Error: ${e.message}`,
          tool_calls: []
        });
      }
    }

    /**
     * Generate content using the Gemini API
     * @param {string} prompt The user's prompt
     * @returns {Promise<string>} The generated text
     */
    async generateContent(prompt) {
      const provider = this.normalizeProvider(this.config.provider || this.PROVIDERS.GEMINI);
      const maxTokens = this.config.maxResponseLength || 4096;

      if (this.isOpenAiCompatibleProvider(provider)) {
        const openaiKey = this.getStoredValue(this.STORAGE_KEY_OPENAI_KEY);
        const baseUrl = this.normalizeOpenAiBaseUrl(this.config.openaiBaseUrl || '', provider);
        if (!openaiKey && baseUrl === 'https://api.openai.com/v1') {
          throw new Error('OpenAI API key missing. Set one or use a local OpenAI-compatible endpoint.');
        }
        const resolvedModel = await this.resolveModel({
          provider,
          model: this.config.model,
          openaiBaseUrl: baseUrl,
          openaiApiKey: openaiKey
        });
        if (!resolvedModel) {
          throw new Error(`No model selected for ${this.getProviderDisplayName(provider)}. Refresh models or enter a model id.`);
        }
        const headers = { 'Content-Type': 'application/json' };
        if (openaiKey) {
          headers['Authorization'] = `Bearer ${openaiKey}`;
        }
        const body = {
          model: resolvedModel,
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
