#include "util/i18n/translator.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "gtest/gtest.h"
#include "util/i18n/language_manager.h"

namespace yaze::test {
namespace {

using ::yaze::i18n::LanguageManager;
using ::yaze::i18n::tr;

// Restores the shared LanguageManager singleton to English between tests so
// cases stay independent regardless of ordering.
class I18nTest : public ::testing::Test {
 protected:
  void SetUp() override { LanguageManager::Get().SetLanguage("en"); }
  void TearDown() override { LanguageManager::Get().SetLanguage("en"); }
};

// --- English (identity) behaviour: deterministic, no catalog files ----------

TEST_F(I18nTest, FallsBackToSourceWhenUntranslated) {
  EXPECT_STREQ(tr("Totally Unknown String 12345"),
               "Totally Unknown String 12345");
}

TEST_F(I18nTest, NullSourceReturnsEmptyString) {
  EXPECT_STREQ(tr(static_cast<const char*>(nullptr)), "");
}

TEST_F(I18nTest, PreservesFormatSpecifiersInIdentity) {
  EXPECT_STREQ(tr("Lines: %d"), "Lines: %d");
  EXPECT_STREQ(tr("Version %s (%d)"), "Version %s (%d)");
}

TEST_F(I18nTest, PureIdLabelIsReturnedUnchanged) {
  EXPECT_STREQ(tr("##hidden_id"), "##hidden_id");
}

// --- Catalog-backed behaviour via the in-memory test seam -------------------

TEST_F(I18nTest, TranslatesVisibleTextWhenCatalogLoaded) {
  ASSERT_TRUE(LanguageManager::Get().LoadCatalogFromStringForTesting(
      "fr", R"({"Save":"Enregistrer","File":"Fichier"})"));
  LanguageManager::Get().SetLanguage("fr");
  EXPECT_STREQ(tr("Save"), "Enregistrer");
  EXPECT_STREQ(tr("File"), "Fichier");
}

TEST_F(I18nTest, PreservesImGuiIdSuffixWhileTranslatingPrefix) {
  ASSERT_TRUE(LanguageManager::Get().LoadCatalogFromStringForTesting(
      "fr", R"({"Mode":"Mode FR"})"));
  LanguageManager::Get().SetLanguage("fr");
  // "##id" and "###id" suffixes must survive byte-for-byte so widget state
  // (focus/expansion/storage) is stable across languages.
  EXPECT_STREQ(tr("Mode##dungeon_selector"), "Mode FR##dungeon_selector");
  EXPECT_STREQ(tr("Mode###fixed_id"), "Mode FR###fixed_id");
}

TEST_F(I18nTest, UntranslatedKeyInLoadedCatalogFallsBackToSource) {
  ASSERT_TRUE(LanguageManager::Get().LoadCatalogFromStringForTesting(
      "fr", R"({"Save":"Enregistrer"})"));
  LanguageManager::Get().SetLanguage("fr");
  EXPECT_STREQ(tr("Nonexistent Key"), "Nonexistent Key");
}

TEST_F(I18nTest, LiveSwitchReresolvesStrings) {
  ASSERT_TRUE(LanguageManager::Get().LoadCatalogFromStringForTesting(
      "fr", R"({"File":"Fichier"})"));
  EXPECT_STREQ(tr("File"), "File");  // en identity first
  LanguageManager::Get().SetLanguage("fr");
  EXPECT_STREQ(tr("File"),
               "Fichier");  // cache cleared on switch -> re-resolved
  LanguageManager::Get().SetLanguage("en");
  EXPECT_STREQ(tr("File"), "File");
}

// --- LanguageManager mechanics ---------------------------------------------

TEST_F(I18nTest, UnknownLocaleFallsBackToEnglish) {
  LanguageManager::Get().SetLanguage("zz_nonexistent");
  EXPECT_EQ(LanguageManager::Get().GetCurrentLocale(), "en");
}

TEST_F(I18nTest, AvailableLocalesAlwaysIncludeEnglish) {
  const auto locales = LanguageManager::Get().GetAvailableLocales();
  EXPECT_NE(std::find(locales.begin(), locales.end(), "en"), locales.end());
}

TEST_F(I18nTest, ChangedCallbackFiresWithNewLocale) {
  ASSERT_TRUE(
      LanguageManager::Get().LoadCatalogFromStringForTesting("fr", "{}"));
  std::string observed;
  LanguageManager::Get().SetOnLanguageChangedCallback(
      [&](const std::string& loc) { observed = loc; });
  LanguageManager::Get().SetLanguage("fr");
  EXPECT_EQ(observed, "fr");
  LanguageManager::Get().SetOnLanguageChangedCallback(nullptr);
}

// --- Flat-JSON catalog parser (escapes / UTF-8) -----------------------------

TEST_F(I18nTest, CatalogParserHandlesEscapesAndUnicode) {
  ASSERT_TRUE(LanguageManager::Get().LoadCatalogFromStringForTesting(
      "fr", R"({"Tab\tItem":"Onglet","Quote \"x\"":"Guillemet","Uni":"éœ"})"));
  LanguageManager::Get().SetLanguage("fr");
  EXPECT_STREQ(tr("Tab\tItem"), "Onglet");
  EXPECT_STREQ(tr("Quote \"x\""), "Guillemet");
  // é = 'é' (UTF-8 C3 A9), œ = 'œ' (UTF-8 C5 93).
  EXPECT_STREQ(tr("Uni"), "\xC3\xA9\xC5\x93");
}

TEST_F(I18nTest, CatalogParserRejectsNonObject) {
  EXPECT_FALSE(LanguageManager::Get().LoadCatalogFromStringForTesting(
      "fr", R"(["not","an","object"])"));
}

// --- Transient sources: menus are rebuilt every frame, so a label's
// std::string reuses stack/vector addresses across frames. tr() must resolve by
// CONTENT, not by source pointer, or a reused address returns a stale
// translation (the menu-label flicker). Regression guard.

TEST_F(I18nTest, TransientSourceReusedAddressResolvesByContent) {
  ASSERT_TRUE(LanguageManager::Get().LoadCatalogFromStringForTesting(
      "fr", R"({"File":"Fichier","Edit":"Édition"})"));
  LanguageManager::Get().SetLanguage("fr");
  char buf[8];
  std::strcpy(buf, "File");
  EXPECT_STREQ(tr(buf), "Fichier");
  std::strcpy(buf, "Edit");  // same address, different content
  EXPECT_STREQ(tr(buf), "Édition");
}

TEST_F(I18nTest, TranslationIsIdempotent) {
  ASSERT_TRUE(LanguageManager::Get().LoadCatalogFromStringForTesting(
      "fr", R"({"File":"Fichier"})"));
  LanguageManager::Get().SetLanguage("fr");
  // Re-translating an already-translated string must be a no-op (catalog values
  // must never themselves be keys), otherwise repeated draws cycle.
  EXPECT_STREQ(tr(tr("File")), tr("File"));
}

TEST_F(I18nTest, PreservesFormatSpecifiersFromCatalogValue) {
  ASSERT_TRUE(LanguageManager::Get().LoadCatalogFromStringForTesting(
      "fr",
      R"j({"Lines: %d":"Lignes : %d","Version %s (%d)":"Version %s (%d)"})j"));
  LanguageManager::Get().SetLanguage("fr");
  EXPECT_STREQ(tr("Lines: %d"), "Lignes : %d");
  EXPECT_STREQ(tr("Version %s (%d)"), "Version %s (%d)");
}

}  // namespace
}  // namespace yaze::test
