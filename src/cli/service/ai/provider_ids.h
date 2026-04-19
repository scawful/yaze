#ifndef YAZE_SRC_CLI_SERVICE_AI_PROVIDER_IDS_H_
#define YAZE_SRC_CLI_SERVICE_AI_PROVIDER_IDS_H_

namespace yaze::cli {

inline constexpr char kProviderAuto[] = "auto";
inline constexpr char kProviderMock[] = "mock";
inline constexpr char kProviderOllama[] = "ollama";
inline constexpr char kProviderGemini[] = "gemini";
inline constexpr char kProviderAnthropic[] = "anthropic";
inline constexpr char kProviderOpenAi[] = "openai";
inline constexpr char kProviderHalext[] = "halext";
inline constexpr char kProviderAfsBridge[] = "afs-bridge";
inline constexpr char kProviderExternal[] = "external";

inline constexpr char kProviderClaude[] = "claude";
inline constexpr char kProviderAnthropicClaude[] = "anthropic-claude";
inline constexpr char kProviderSonnet[] = "sonnet";
inline constexpr char kProviderOpus[] = "opus";
inline constexpr char kProviderChatGpt[] = "chatgpt";
inline constexpr char kProviderGpt[] = "gpt";
inline constexpr char kProviderGoogle[] = "google";
inline constexpr char kProviderGoogleGemini[] = "google-gemini";
inline constexpr char kProviderLmStudio[] = "lmstudio";
inline constexpr char kProviderLmStudioDashed[] = "lm-studio";
inline constexpr char kProviderCustomOpenAi[] = "custom-openai";
inline constexpr char kProviderOpenAiCompatible[] = "openai-compatible";
inline constexpr char kProviderGeminiCli[] = "gemini-cli";
inline constexpr char kProviderLocalGemini[] = "local-gemini";

}  // namespace yaze::cli

#endif  // YAZE_SRC_CLI_SERVICE_AI_PROVIDER_IDS_H_
