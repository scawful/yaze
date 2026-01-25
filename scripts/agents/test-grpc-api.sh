#!/usr/bin/env bash
# Basic smoke test for the gRPC automation API.
# Usage: scripts/agents/test-grpc-api.sh [host] [port]

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-50052}"
ADDR="${HOST}:${PORT}"
PROTO_DIR="src/protos"

if ! command -v grpcurl >/dev/null 2>&1; then
  echo "error: grpcurl is required to test the gRPC API" >&2
  echo "Install from https://github.com/fullstorydev/grpcurl" >&2
  exit 1
fi

if [[ ! -d "${PROTO_DIR}" ]]; then
  echo "error: proto directory not found at ${PROTO_DIR}" >&2
  exit 1
fi

echo "Checking gRPC API Ping at ${ADDR}"
if ! grpcurl -plaintext \
  -import-path "${PROTO_DIR}" \
  -proto imgui_test_harness.proto \
  -d '{"message":"ping"}' \
  "${ADDR}" yaze.test.ImGuiTestHarness/Ping >/dev/null; then
  echo "error: gRPC Ping failed at ${ADDR}" >&2
  echo "Ensure yaze is running with --enable_test_harness --test_harness_port=${PORT}" >&2
  exit 1
fi

echo "Checking gRPC API ListTests at ${ADDR}"
if ! grpcurl -plaintext \
  -import-path "${PROTO_DIR}" \
  -proto imgui_test_harness.proto \
  -d '{"page_size":1}' \
  "${ADDR}" yaze.test.ImGuiTestHarness/ListTests >/dev/null; then
  echo "error: gRPC ListTests failed at ${ADDR}" >&2
  echo "Ensure yaze is running with --enable_test_harness --test_harness_port=${PORT}" >&2
  exit 1
fi

echo "gRPC API responded successfully"
