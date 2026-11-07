const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');

function makeStorage() {
  const store = new Map();
  return {
    getItem(key) {
      return store.has(key) ? store.get(key) : null;
    },
    setItem(key, value) {
      store.set(key, String(value));
    },
    removeItem(key) {
      store.delete(key);
    }
  };
}

function loadScript(relativePath) {
  return fs.readFileSync(path.join(__dirname, '..', relativePath), 'utf8');
}

async function main() {
  const localStorage = makeStorage();
  const sessionStorage = makeStorage();
  let capturedResponse = null;

  const context = {
    console,
    setInterval,
    clearInterval,
    URLSearchParams,
    localStorage,
    sessionStorage,
    document: {
      createElement() {
        return { textContent: '', innerHTML: '' };
      }
    },
    window: {
      YAZE_CONFIG: {
        ai: {
          provider: 'openai',
          model: 'site-model',
          openaiBaseUrl: 'https://site.example/api/v1',
          auth: {}
        }
      }
    }
  };

  context.window.window = context.window;
  context.window.document = context.document;
  context.window.localStorage = localStorage;
  context.window.sessionStorage = sessionStorage;
  context.window.console = console;
  context.window.Module = {
    onExternalAiResponse(json) {
      capturedResponse = JSON.parse(json);
    }
  };

  context.fetch = async () => ({
    ok: true,
    json: async () => ({}),
    text: async () => 'ok'
  });
  context.window.fetch = context.fetch;

  vm.createContext(context);
  vm.runInContext(loadScript('core/namespace.js'), context);
  vm.runInContext(loadScript('core/ai_manager.js'), context);

  const ai = context.window.yaze.ai;
  if (!ai) {
    throw new Error('AiManager was not initialized');
  }

  assert.strictEqual(ai.getConfigSnapshot().provider, 'openai');
  assert.strictEqual(ai.getConfigSnapshot().openaiBaseUrl, 'https://site.example/api/v1');
  assert.strictEqual(ai.getConfigSnapshot().model, 'site-model');
  localStorage.setItem('yaze_ai_provider', 'lmstudio');
  localStorage.setItem('yaze_openai_base_url', 'http://localhost:8123');
  ai.reloadConfigFromStorage();
  assert.strictEqual(ai.getConfigSnapshot().openaiBaseUrl, 'http://localhost:8123/v1');

  localStorage.removeItem('yaze_openai_base_url');
  ai.reloadConfigFromStorage();
  assert.strictEqual(ai.getConfigSnapshot().openaiBaseUrl, 'http://localhost:1234/v1');

  localStorage.setItem('yaze_ai_provider', 'halext');
  ai.reloadConfigFromStorage();
  assert.strictEqual(ai.getConfigSnapshot().openaiBaseUrl, 'https://halext.org/v1');

  localStorage.setItem('yaze_openai_base_url', 'https://bridge.example/custom/');
  localStorage.setItem('yaze_ai_model', 'custom-model');
  ai.reloadConfigFromStorage();
  assert.strictEqual(ai.getConfigSnapshot().openaiBaseUrl, 'https://bridge.example/custom/v1');
  assert.strictEqual(ai.getConfigSnapshot().model, 'custom-model');

  localStorage.removeItem('yaze_ai_provider');
  localStorage.removeItem('yaze_openai_base_url');
  localStorage.removeItem('yaze_ai_model');
  ai.reloadConfigFromStorage();
  assert.strictEqual(ai.getConfigSnapshot().provider, 'openai');
  assert.strictEqual(ai.getConfigSnapshot().openaiBaseUrl, 'https://site.example/api/v1');
  assert.strictEqual(ai.getConfigSnapshot().model, 'site-model');

  ai.config.provider = 'openai';
  ai.config.model = 'gpt-4o-mini';
  ai.config.openaiBaseUrl = 'https://api.openai.com/v1';
  sessionStorage.setItem('z3ed_openai_api_key', 'test-key');
  context.fetch = async () => ({
    ok: true,
    json: async () => ({
      choices: [{
        message: {
          content: JSON.stringify({
            text: 'Need to inspect the room first.',
            tool_calls: [{
              tool_name: 'room-inspect',
              args: { room_id: 5, verbose: true }
            }]
          })
        }
      }]
    }),
    text: async () => 'ok'
  });
  context.window.fetch = context.fetch;

  await ai.processAgentRequest(JSON.stringify([
    { role: 'user', parts: [{ text: 'inspect room 5' }] }
  ]));

  if (!capturedResponse ||
      !Array.isArray(capturedResponse.tool_calls) ||
      capturedResponse.tool_calls.length !== 1) {
    throw new Error('Structured OpenAI tool call was not forwarded');
  }
  if (capturedResponse.tool_calls[0].name !== 'room-inspect') {
    throw new Error('OpenAI tool name mismatch');
  }
  if (capturedResponse.tool_calls[0].args.room_id !== 5 ||
      capturedResponse.tool_calls[0].args.verbose !== true) {
    throw new Error('OpenAI tool args mismatch');
  }

  capturedResponse = null;
  ai.config.provider = 'gemini';
  ai.config.model = 'gemini-2.5-flash';
  sessionStorage.setItem('z3ed_gemini_api_key', 'gem-key');
  context.fetch = async () => ({
    ok: true,
    json: async () => ({
      candidates: [{
        content: {
          parts: [
            { functionCall: { name: 'resource-list', args: { type: 'dungeon' } } },
            { text: JSON.stringify({ text: 'Listed dungeon resources.' }) }
          ]
        }
      }]
    }),
    text: async () => 'ok'
  });
  context.window.fetch = context.fetch;

  await ai.processAgentRequest(JSON.stringify([
    { role: 'user', parts: [{ text: 'list dungeon resources' }] }
  ]));

  if (!capturedResponse ||
      !Array.isArray(capturedResponse.tool_calls) ||
      capturedResponse.tool_calls.length !== 1 ||
      capturedResponse.tool_calls[0].name !== 'resource-list') {
    throw new Error('Gemini function call was not forwarded');
  }
  if (capturedResponse.text !== 'Listed dungeon resources.') {
    throw new Error('Gemini text payload mismatch');
  }

  console.log('ai_manager_driver_test passed');
}

main().catch(error => {
  console.error(error);
  process.exit(1);
});
