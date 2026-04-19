const fs = require('fs');
const path = require('path');
const vm = require('vm');

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
          model: 'gpt-4o-mini',
          openaiBaseUrl: 'https://api.openai.com/v1',
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
