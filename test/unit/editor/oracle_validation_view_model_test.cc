// Unit tests for oracle_validation_view_model.h
//
// Covers: JSON parsing (smoke + preflight), command argument construction,
// CLI command reconstruction, and optional-boolean handling for the
// "ran" / "skipped" readiness-check fields.
//
// No ImGui or ROM fixture required.

#include "app/editor/oracle/panels/oracle_validation_view_model.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze::editor::oracle_validation {
namespace {

// ---------------------------------------------------------------------------
// BuildSmokeArgs
// ---------------------------------------------------------------------------

TEST(OracleValidationViewModelTest, BuildSmokeArgsDefaultMode) {
  SmokeOptions opts;
  opts.rom_path = "roms/oos168.sfc";
  opts.min_d6_track_rooms = 4;

  auto args = BuildSmokeArgs(opts);
  EXPECT_THAT(args, ::testing::Contains("--rom=roms/oos168.sfc"));
  EXPECT_THAT(args, ::testing::Contains("--min-d6-track-rooms=4"));
  EXPECT_THAT(args, ::testing::Contains("--format=json"));
  EXPECT_THAT(args, ::testing::Not(::testing::Contains("--strict-readiness")));
}

TEST(OracleValidationViewModelTest, BuildSmokeArgsStrictReadiness) {
  SmokeOptions opts;
  opts.rom_path = "roms/oos168.sfc";
  opts.min_d6_track_rooms = 4;
  opts.strict_readiness = true;

  auto args = BuildSmokeArgs(opts);
  EXPECT_THAT(args, ::testing::Contains("--strict-readiness"));
}

TEST(OracleValidationViewModelTest, BuildSmokeArgsWithReportPath) {
  SmokeOptions opts;
  opts.rom_path = "rom.sfc";
  opts.report_path = "/tmp/smoke.json";

  auto args = BuildSmokeArgs(opts);
  EXPECT_THAT(args, ::testing::Contains("--report=/tmp/smoke.json"));
}

TEST(OracleValidationViewModelTest, BuildSmokeArgsMinZeroOmitted) {
  // min_d6_track_rooms=0 should NOT add the flag (0 means disabled).
  SmokeOptions opts;
  opts.rom_path = "rom.sfc";
  opts.min_d6_track_rooms = 0;

  auto args = BuildSmokeArgs(opts);
  for (const auto& arg : args) {
    EXPECT_FALSE(arg.find("min-d6-track-rooms") != std::string::npos)
        << "unexpected --min-d6-track-rooms when value is 0: " << arg;
  }
}

// ---------------------------------------------------------------------------
// BuildPreflightArgs
// ---------------------------------------------------------------------------

TEST(OracleValidationViewModelTest, BuildPreflightArgsRequiredRooms) {
  PreflightOptions opts;
  opts.rom_path = "rom.sfc";
  opts.required_collision_rooms = "0x25,0x27";

  auto args = BuildPreflightArgs(opts);
  EXPECT_THAT(args,
              ::testing::Contains("--required-collision-rooms=0x25,0x27"));
  EXPECT_THAT(args, ::testing::Contains("--format=json"));
}

TEST(OracleValidationViewModelTest, BuildPreflightArgsSkipCollisionMaps) {
  PreflightOptions opts;
  opts.rom_path = "rom.sfc";
  opts.skip_collision_maps = true;

  auto args = BuildPreflightArgs(opts);
  EXPECT_THAT(args, ::testing::Contains("--skip-collision-maps"));
}

// ---------------------------------------------------------------------------
// BuildCliCommand
// ---------------------------------------------------------------------------

TEST(OracleValidationViewModelTest, BuildCliCommandReconstructsCorrectly) {
  const std::string cmd = BuildCliCommand(
      "oracle-smoke-check",
      {"--rom=rom.sfc", "--min-d6-track-rooms=4", "--format=json"});
  EXPECT_EQ(cmd,
            "z3ed oracle-smoke-check --rom=rom.sfc "
            "--min-d6-track-rooms=4 --format=json");
}

// ---------------------------------------------------------------------------
// ParseSmokeCheckOutput — pass case (expanded ROM, real-like output)
// ---------------------------------------------------------------------------

static const char* kSmokePassJson = R"json({
  "Oracle Smoke Check": {
    "ok": true,
    "status": "pass",
    "strict_readiness": false,
    "checks": {
      "d4_zora_temple": {
        "structural_ok": true,
        "required_rooms_check": "ran",
        "required_rooms_ok": false
      },
      "d6_goron_mines": {
        "ok": true,
        "track_rooms_found": 4,
        "min_track_rooms": 4,
        "meets_min_track_rooms": true
      },
      "d3_kalyxo_castle": {
        "readiness_check": "ran",
        "ok": false
      }
    }
  }
})json";

TEST(OracleValidationViewModelTest, ParseSmokePassCasePopulatesFields) {
  auto result = ParseSmokeCheckOutput(kSmokePassJson);
  ASSERT_TRUE(result.ok()) << result.status().message();

  EXPECT_TRUE(result->ok);
  EXPECT_EQ(result->status, "pass");
  EXPECT_FALSE(result->strict_readiness);

  // D4
  EXPECT_TRUE(result->d4.structural_ok);
  EXPECT_EQ(result->d4.required_rooms_check, "ran");
  ASSERT_TRUE(result->d4.required_rooms_ok.has_value());
  EXPECT_FALSE(*result->d4.required_rooms_ok);  // rooms not yet authored

  // D6
  EXPECT_TRUE(result->d6.ok);
  EXPECT_EQ(result->d6.track_rooms_found, 4);
  EXPECT_EQ(result->d6.min_track_rooms, 4);
  EXPECT_TRUE(result->d6.meets_min_track_rooms);

  // D3
  EXPECT_EQ(result->d3.readiness_check, "ran");
  ASSERT_TRUE(result->d3.ok.has_value());
  EXPECT_FALSE(*result->d3.ok);
}

// ---------------------------------------------------------------------------
// ParseSmokeCheckOutput — "skipped" fields on non-expanded ROM
// ---------------------------------------------------------------------------

static const char* kSmokeSkippedJson = R"json({
  "Oracle Smoke Check": {
    "ok": false,
    "status": "fail",
    "strict_readiness": false,
    "checks": {
      "d4_zora_temple": {
        "structural_ok": false,
        "required_rooms_check": "skipped"
      },
      "d6_goron_mines": {
        "ok": true,
        "track_rooms_found": 0,
        "min_track_rooms": 0,
        "meets_min_track_rooms": true
      },
      "d3_kalyxo_castle": {
        "readiness_check": "skipped"
      }
    }
  }
})json";

TEST(OracleValidationViewModelTest, ParseSmokeSkippedFieldsAreAbsent) {
  auto result = ParseSmokeCheckOutput(kSmokeSkippedJson);
  ASSERT_TRUE(result.ok()) << result.status().message();

  EXPECT_FALSE(result->ok);
  EXPECT_FALSE(result->d4.structural_ok);
  EXPECT_EQ(result->d4.required_rooms_check, "skipped");
  // required_rooms_ok must NOT be present when check was skipped.
  EXPECT_FALSE(result->d4.required_rooms_ok.has_value())
      << "required_rooms_ok must be absent when check is skipped";

  EXPECT_EQ(result->d3.readiness_check, "skipped");
  // d3.ok must NOT be present when check was skipped.
  EXPECT_FALSE(result->d3.ok.has_value())
      << "d3.ok must be absent when check is skipped";
}

// ---------------------------------------------------------------------------
// ParseSmokeCheckOutput — error cases
// ---------------------------------------------------------------------------

TEST(OracleValidationViewModelTest, ParseSmokeInvalidJsonReturnsError) {
  auto result = ParseSmokeCheckOutput("{not valid json}}}");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(OracleValidationViewModelTest, ParseSmokeMissingRootKeyReturnsError) {
  auto result = ParseSmokeCheckOutput(R"json({"wrong_key": {}})json");
  EXPECT_FALSE(result.ok());
}

// ---------------------------------------------------------------------------
// ParsePreflightOutput — pass case
// ---------------------------------------------------------------------------

static const char* kPreflightPassJson = R"json({
  "Dungeon Oracle Preflight": {
    "ok": true,
    "error_count": 0,
    "water_fill_region_ok": true,
    "water_fill_table_ok": true,
    "custom_collision_maps_ok": true,
    "required_rooms_check": "ran",
    "required_rooms_ok": false,
    "errors": [],
    "status": "pass"
  }
})json";

TEST(OracleValidationViewModelTest, ParsePreflightPassCasePopulatesFields) {
  auto result = ParsePreflightOutput(kPreflightPassJson);
  ASSERT_TRUE(result.ok()) << result.status().message();

  EXPECT_TRUE(result->ok);
  EXPECT_EQ(result->error_count, 0);
  EXPECT_TRUE(result->water_fill_region_ok);
  EXPECT_TRUE(result->water_fill_table_ok);
  EXPECT_TRUE(result->custom_collision_maps_ok);
  EXPECT_EQ(result->required_rooms_check, "ran");
  ASSERT_TRUE(result->required_rooms_ok.has_value());
  EXPECT_FALSE(*result->required_rooms_ok);
  EXPECT_TRUE(result->errors.empty());
  EXPECT_EQ(result->status, "pass");
}

// ---------------------------------------------------------------------------
// ParsePreflightOutput — fail case with errors
// ---------------------------------------------------------------------------

static const char* kPreflightFailJson = R"json({
  "Dungeon Oracle Preflight": {
    "ok": false,
    "error_count": 2,
    "water_fill_region_ok": false,
    "water_fill_table_ok": false,
    "custom_collision_maps_ok": true,
    "required_rooms_check": "skipped",
    "errors": [
      {
        "code": "ORACLE_WATER_FILL_REGION_MISSING",
        "message": "WaterFill reserved region not present in this ROM",
        "status": "FAILED_PRECONDITION"
      },
      {
        "code": "ORACLE_REQUIRED_ROOM_MISSING_COLLISION",
        "message": "Room 0x32 has no custom collision data",
        "status": "FAILED_PRECONDITION",
        "room_id": "0x32"
      }
    ],
    "status": "fail"
  }
})json";

TEST(OracleValidationViewModelTest, ParsePreflightFailCaseHasErrors) {
  auto result = ParsePreflightOutput(kPreflightFailJson);
  ASSERT_TRUE(result.ok()) << result.status().message();

  EXPECT_FALSE(result->ok);
  EXPECT_EQ(result->error_count, 2);
  EXPECT_FALSE(result->water_fill_region_ok);
  EXPECT_EQ(result->required_rooms_check, "skipped");
  EXPECT_FALSE(result->required_rooms_ok.has_value());

  ASSERT_EQ(result->errors.size(), 2u);
  EXPECT_EQ(result->errors[0].code, "ORACLE_WATER_FILL_REGION_MISSING");
  EXPECT_FALSE(result->errors[0].room_id.has_value());

  EXPECT_EQ(result->errors[1].code, "ORACLE_REQUIRED_ROOM_MISSING_COLLISION");
  ASSERT_TRUE(result->errors[1].room_id.has_value());
  EXPECT_EQ(*result->errors[1].room_id, "0x32");
}

TEST(OracleValidationViewModelTest, ParsePreflightMissingRootKeyReturnsError) {
  auto result = ParsePreflightOutput(R"json({"other_key": {}})json");
  EXPECT_FALSE(result.ok());
}

// ---------------------------------------------------------------------------
// Additional targeted tests (Step 1 hardening)
// ---------------------------------------------------------------------------

// BuildSmokeArgs: strict_readiness=true AND min_d6=0 — no --min-d6-track-rooms
// emitted, but --strict-readiness IS emitted.
TEST(OracleValidationViewModelTest, BuildSmokeArgsStrictReadinessWithMinZero) {
  SmokeOptions opts;
  opts.rom_path = "rom.sfc";
  opts.strict_readiness = true;
  opts.min_d6_track_rooms = 0;

  auto args = BuildSmokeArgs(opts);
  EXPECT_THAT(args, ::testing::Contains("--strict-readiness"));
  for (const auto& arg : args) {
    EXPECT_FALSE(arg.find("min-d6-track-rooms") != std::string::npos)
        << "unexpected --min-d6-track-rooms when value is 0: " << arg;
  }
  // --format=json must still be present
  EXPECT_THAT(args, ::testing::Contains("--format=json"));
}

// BuildCliCommand: empty args list → output is just "z3ed <command>" with no
// trailing space.
TEST(OracleValidationViewModelTest, BuildCliCommandEmptyArgsList) {
  const std::string cmd = BuildCliCommand("oracle-smoke-check", {});
  EXPECT_EQ(cmd, "z3ed oracle-smoke-check");
}

// ParseSmokeCheckOutput: D6 ok=false / meets_min_track_rooms=false (the case
// where fewer track rooms were found than the minimum required).
static const char* kSmokeD6FailJson = R"json({
  "Oracle Smoke Check": {
    "ok": false,
    "status": "fail",
    "strict_readiness": false,
    "checks": {
      "d4_zora_temple": {
        "structural_ok": true,
        "required_rooms_check": "skipped"
      },
      "d6_goron_mines": {
        "ok": false,
        "track_rooms_found": 2,
        "min_track_rooms": 4,
        "meets_min_track_rooms": false
      },
      "d3_kalyxo_castle": {
        "readiness_check": "skipped"
      }
    }
  }
})json";

TEST(OracleValidationViewModelTest, ParseSmokeD6FailMeetsMinIsFalse) {
  auto result = ParseSmokeCheckOutput(kSmokeD6FailJson);
  ASSERT_TRUE(result.ok()) << result.status().message();

  EXPECT_FALSE(result->ok);
  EXPECT_EQ(result->status, "fail");

  EXPECT_FALSE(result->d6.ok);
  EXPECT_EQ(result->d6.track_rooms_found, 2);
  EXPECT_EQ(result->d6.min_track_rooms, 4);
  EXPECT_FALSE(result->d6.meets_min_track_rooms);
}

// ParsePreflightOutput: required_rooms_ok is present AND true — the happy path
// where required rooms have been authored and the check passed.
static const char* kPreflightRoomsOkTrueJson = R"json({
  "Dungeon Oracle Preflight": {
    "ok": true,
    "error_count": 0,
    "water_fill_region_ok": true,
    "water_fill_table_ok": true,
    "custom_collision_maps_ok": true,
    "required_rooms_check": "ran",
    "required_rooms_ok": true,
    "errors": [],
    "status": "pass"
  }
})json";

TEST(OracleValidationViewModelTest, ParsePreflightRequiredRoomsOkTrue) {
  auto result = ParsePreflightOutput(kPreflightRoomsOkTrueJson);
  ASSERT_TRUE(result.ok()) << result.status().message();

  EXPECT_TRUE(result->ok);
  EXPECT_EQ(result->required_rooms_check, "ran");
  ASSERT_TRUE(result->required_rooms_ok.has_value());
  EXPECT_TRUE(*result->required_rooms_ok);
  EXPECT_EQ(result->status, "pass");
}

// ParseSmokeCheckOutput: strict_readiness=true in the JSON payload — verify
// the field is propagated correctly into SmokeResult.
static const char* kSmokeStrictReadinessTrueJson = R"json({
  "Oracle Smoke Check": {
    "ok": true,
    "status": "pass",
    "strict_readiness": true,
    "checks": {
      "d4_zora_temple": {
        "structural_ok": true,
        "required_rooms_check": "ran",
        "required_rooms_ok": true
      },
      "d6_goron_mines": {
        "ok": true,
        "track_rooms_found": 4,
        "min_track_rooms": 4,
        "meets_min_track_rooms": true
      },
      "d3_kalyxo_castle": {
        "readiness_check": "ran",
        "ok": true
      }
    }
  }
})json";

TEST(OracleValidationViewModelTest, ParseSmokeStrictReadinessTrueIsPopulated) {
  auto result = ParseSmokeCheckOutput(kSmokeStrictReadinessTrueJson);
  ASSERT_TRUE(result.ok()) << result.status().message();

  EXPECT_TRUE(result->strict_readiness);
  EXPECT_TRUE(result->ok);
  EXPECT_EQ(result->status, "pass");

  // D4 required_rooms_ok is present and true in strict mode
  ASSERT_TRUE(result->d4.required_rooms_ok.has_value());
  EXPECT_TRUE(*result->d4.required_rooms_ok);

  // D3 ok is present and true in strict mode
  ASSERT_TRUE(result->d3.ok.has_value());
  EXPECT_TRUE(*result->d3.ok);
}

}  // namespace
}  // namespace yaze::editor::oracle_validation
