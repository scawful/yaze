#!/usr/bin/env bash
# Basic health check for the HTTP API server.
# Usage: scripts/agents/test-http-api.sh [host] [port]

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
BASE_URL="http://${HOST}:${PORT}/api/v1"
HEALTH_URL="${BASE_URL}/health"
MODELS_URL="${BASE_URL}/models"
MODELS_REFRESH_URL="${BASE_URL}/models?refresh=1"
SYMBOLS_URL="${BASE_URL}/symbols?format=mesen"
INVALID_SYMBOLS_URL="${BASE_URL}/symbols?format=bogus"
NAVIGATE_URL="${BASE_URL}/navigate"
BREAKPOINT_URL="${BASE_URL}/breakpoint/hit"
STATE_URL="${BASE_URL}/state/update"
WINDOW_SHOW_URL="${BASE_URL}/window/show"
WINDOW_HIDE_URL="${BASE_URL}/window/hide"
UNKNOWN_URL="${BASE_URL}/does-not-exist"

if ! command -v curl >/dev/null 2>&1; then
  echo "error: curl is required to test the HTTP API" >&2
  exit 1
fi

check_http_code() {
  local url="$1"
  shift
  local expected=("$@")
  local code
  code=$(curl -s -o /dev/null -w "%{http_code}" "${url}" || true)
  if [[ -z "${code}" || "${code}" == "000" ]]; then
    return 1
  fi
  for exp in "${expected[@]}"; do
    if [[ "${code}" == "${exp}" ]]; then
      return 0
    fi
  done
  echo "Unexpected status ${code} for ${url} (expected: ${expected[*]})"
  return 1
}

get_content_type() {
  local url="$1"
  curl -s -D - -o /dev/null "${url}" | awk -F': ' 'tolower($1)=="content-type"{print $2; exit}' | tr -d '\r'
}

get_content_type_with_accept() {
  local url="$1"
  local accept="$2"
  curl -s -D - -o /dev/null -H "Accept: ${accept}" "${url}" | awk -F': ' 'tolower($1)=="content-type"{print $2; exit}' | tr -d '\r'
}

get_header_value_from_block() {
  local block="$1"
  local header="$2"
  echo "${block}" | awk -F': ' -v key="${header}" 'tolower($1)==tolower(key){print $2; exit}' | tr -d '\r'
}

get_options_headers() {
  local url="$1"
  local method="$2"
  curl -s -D - -o /dev/null -X OPTIONS \
    -H "Origin: http://localhost" \
    -H "Access-Control-Request-Method: ${method}" \
    -H "Access-Control-Request-Headers: Content-Type" \
    "${url}"
}

poll_endpoint() {
  local label="$1"
  local url="$2"
  shift 2
  local expected=("$@")
  echo "Checking HTTP API ${label} endpoint at ${url}"
  for attempt in {1..10}; do
    if check_http_code "${url}" "${expected[@]}"; then
      echo "HTTP API ${label} responded successfully (attempt ${attempt})"
      return 0
    fi
    echo "Attempt ${attempt} failed; retrying..."
    sleep 1
  done
  echo "error: HTTP API ${label} did not respond at ${url}" >&2
  return 1
}

post_endpoint() {
  local label="$1"
  local url="$2"
  local payload="$3"
  shift 3
  local expected=("$@")
  echo "Checking HTTP API ${label} endpoint at ${url}"
  local code
  code=$(curl -s -o /dev/null -w "%{http_code}" \
    -H "Content-Type: application/json" \
    -d "${payload}" \
    "${url}" || true)
  if [[ -z "${code}" || "${code}" == "000" ]]; then
    echo "error: HTTP API ${label} did not respond at ${url}" >&2
    return 1
  fi
  for exp in "${expected[@]}"; do
    if [[ "${code}" == "${exp}" ]]; then
      echo "HTTP API ${label} responded successfully"
      return 0
    fi
  done
  echo "error: HTTP API ${label} returned status ${code} at ${url}" >&2
  return 1
}

options_endpoint() {
  local label="$1"
  local url="$2"
  local method="$3"
  shift 3
  local expected=("$@")
  echo "Checking HTTP API ${label} OPTIONS endpoint at ${url}"
  local code
  code=$(curl -s -o /dev/null -w "%{http_code}" -X OPTIONS \
    -H "Origin: http://localhost" \
    -H "Access-Control-Request-Method: ${method}" \
    -H "Access-Control-Request-Headers: Content-Type" \
    "${url}" || true)
  if [[ -z "${code}" || "${code}" == "000" ]]; then
    echo "error: HTTP API ${label} did not respond at ${url}" >&2
    return 1
  fi
  for exp in "${expected[@]}"; do
    if [[ "${code}" == "${exp}" ]]; then
      local headers
      headers=$(get_options_headers "${url}" "${method}" || true)
      local allow_origin
      allow_origin=$(get_header_value_from_block "${headers}" "Access-Control-Allow-Origin")
      if [[ -z "${allow_origin}" ]]; then
        echo "error: HTTP API ${label} OPTIONS missing Access-Control-Allow-Origin" >&2
        return 1
      fi
      local allow_methods
      allow_methods=$(get_header_value_from_block "${headers}" "Access-Control-Allow-Methods")
      if [[ -z "${allow_methods}" || "${allow_methods}" != *"${method}"* ]]; then
        echo "error: HTTP API ${label} OPTIONS missing ${method} in Access-Control-Allow-Methods" >&2
        return 1
      fi
      local allow_headers
      allow_headers=$(get_header_value_from_block "${headers}" "Access-Control-Allow-Headers")
      if [[ -z "${allow_headers}" || "${allow_headers}" != *"Content-Type"* ]]; then
        echo "error: HTTP API ${label} OPTIONS missing Content-Type in Access-Control-Allow-Headers" >&2
        return 1
      fi
      local max_age
      max_age=$(get_header_value_from_block "${headers}" "Access-Control-Max-Age")
      if [[ -z "${max_age}" ]]; then
        echo "error: HTTP API ${label} OPTIONS missing Access-Control-Max-Age" >&2
        return 1
      fi
      echo "HTTP API ${label} OPTIONS responded successfully"
      return 0
    fi
  done
  echo "error: HTTP API ${label} OPTIONS returned status ${code} at ${url}" >&2
  return 1
}

poll_endpoint "health" "${HEALTH_URL}" 200
poll_endpoint "models" "${MODELS_URL}" 200
poll_endpoint "models refresh" "${MODELS_REFRESH_URL}" 200
poll_endpoint "symbols" "${SYMBOLS_URL}" 200 503

health_type=$(get_content_type "${HEALTH_URL}")
if [[ "${health_type}" != application/json* ]]; then
  echo "error: HTTP API health content-type was ${health_type}" >&2
  exit 1
fi
health_body=$(curl -s "${HEALTH_URL}" || true)
if [[ "${health_body}" != *"\"status\""* ]]; then
  echo "error: HTTP API health response missing status field" >&2
  exit 1
fi
health_headers=$(curl -s -D - -o /dev/null "${HEALTH_URL}" || true)
health_cors=$(get_header_value_from_block "${health_headers}" "Access-Control-Allow-Origin")
if [[ -z "${health_cors}" ]]; then
  echo "error: HTTP API health response missing Access-Control-Allow-Origin" >&2
  exit 1
fi

models_type=$(get_content_type "${MODELS_URL}")
if [[ "${models_type}" != application/json* ]]; then
  echo "error: HTTP API models content-type was ${models_type}" >&2
  exit 1
fi
models_body=$(curl -s "${MODELS_URL}" || true)
if [[ "${models_body}" != *"\"models\""* || "${models_body}" != *"\"count\""* ]]; then
  echo "error: HTTP API models response missing models/count fields" >&2
  exit 1
fi
models_headers=$(curl -s -D - -o /dev/null "${MODELS_URL}" || true)
models_cors=$(get_header_value_from_block "${models_headers}" "Access-Control-Allow-Origin")
if [[ -z "${models_cors}" ]]; then
  echo "error: HTTP API models response missing Access-Control-Allow-Origin" >&2
  exit 1
fi

symbols_code=$(curl -s -o /dev/null -w "%{http_code}" "${SYMBOLS_URL}" || true)
symbols_type=$(get_content_type "${SYMBOLS_URL}")
if [[ "${symbols_code}" == "200" ]]; then
  if [[ "${symbols_type}" != text/plain* ]]; then
    echo "error: HTTP API symbols content-type was ${symbols_type}" >&2
    exit 1
  fi
elif [[ "${symbols_code}" == "503" ]]; then
  if [[ "${symbols_type}" != application/json* ]]; then
    echo "error: HTTP API symbols error content-type was ${symbols_type}" >&2
    exit 1
  fi
fi

symbols_json_code=$(curl -s -o /dev/null -w "%{http_code}" \
  -H "Accept: application/json" "${SYMBOLS_URL}" || true)
symbols_json_type=$(get_content_type_with_accept "${SYMBOLS_URL}" "application/json")
if [[ "${symbols_json_code}" == "200" ]]; then
  if [[ "${symbols_json_type}" != application/json* ]]; then
    echo "error: HTTP API symbols JSON content-type was ${symbols_json_type}" >&2
    exit 1
  fi
  symbols_json_body=$(curl -s -H "Accept: application/json" "${SYMBOLS_URL}" || true)
  if [[ "${symbols_json_body}" != *"\"symbols\""* ]]; then
    echo "error: HTTP API symbols JSON response missing symbols field" >&2
    exit 1
  fi
elif [[ "${symbols_json_code}" == "503" ]]; then
  if [[ "${symbols_json_type}" != application/json* ]]; then
    echo "error: HTTP API symbols JSON error content-type was ${symbols_json_type}" >&2
    exit 1
  fi
  symbols_json_body=$(curl -s -H "Accept: application/json" "${SYMBOLS_URL}" || true)
  if [[ "${symbols_json_body}" != *"\"error\""* ]]; then
    echo "error: HTTP API symbols JSON response missing error field" >&2
    exit 1
  fi
else
  echo "error: HTTP API symbols JSON returned status ${symbols_json_code}" >&2
  exit 1
fi

invalid_symbols_code=$(curl -s -o /dev/null -w "%{http_code}" \
  -H "Accept: application/json" "${INVALID_SYMBOLS_URL}" || true)
invalid_symbols_type=$(get_content_type_with_accept "${INVALID_SYMBOLS_URL}" "application/json")
if [[ "${invalid_symbols_code}" != "400" ]]; then
  echo "error: HTTP API symbols invalid format returned status ${invalid_symbols_code}" >&2
  exit 1
fi
if [[ "${invalid_symbols_type}" != application/json* ]]; then
  echo "error: HTTP API symbols invalid format content-type was ${invalid_symbols_type}" >&2
  exit 1
fi
invalid_symbols_body=$(curl -s -H "Accept: application/json" "${INVALID_SYMBOLS_URL}" || true)
if [[ "${invalid_symbols_body}" != *"\"error\""* || "${invalid_symbols_body}" != *"\"supported\""* ]]; then
  echo "error: HTTP API symbols invalid format response missing error/supported fields" >&2
  exit 1
fi

echo "Checking HTTP API unknown endpoint at ${UNKNOWN_URL}"
if check_http_code "${UNKNOWN_URL}" 404; then
  echo "HTTP API unknown endpoint responded successfully"
else
  echo "error: HTTP API unknown endpoint check failed" >&2
  exit 1
fi
unknown_type=$(get_content_type "${UNKNOWN_URL}")
if [[ "${unknown_type}" != application/json* ]]; then
  echo "error: HTTP API unknown endpoint content-type was ${unknown_type}" >&2
  exit 1
fi
unknown_body=$(curl -s "${UNKNOWN_URL}" || true)
if [[ "${unknown_body}" != *"\"error\""* ]]; then
  echo "error: HTTP API unknown endpoint response missing error field" >&2
  exit 1
fi
unknown_headers=$(curl -s -D - -o /dev/null "${UNKNOWN_URL}" || true)
unknown_cors=$(get_header_value_from_block "${unknown_headers}" "Access-Control-Allow-Origin")
if [[ -z "${unknown_cors}" ]]; then
  echo "error: HTTP API unknown endpoint response missing Access-Control-Allow-Origin" >&2
  exit 1
fi

options_endpoint "health" "${HEALTH_URL}" GET 204
options_endpoint "models" "${MODELS_URL}" GET 204
options_endpoint "symbols" "${SYMBOLS_URL}" GET 204
options_endpoint "navigate" "${NAVIGATE_URL}" POST 204
options_endpoint "breakpoint hit" "${BREAKPOINT_URL}" POST 204
options_endpoint "state update" "${STATE_URL}" POST 204
options_endpoint "window show" "${WINDOW_SHOW_URL}" POST 204
options_endpoint "window hide" "${WINDOW_HIDE_URL}" POST 204

post_endpoint "navigate" "${NAVIGATE_URL}" '{"address":4660,"source":"test-http-api"}' 200
post_endpoint "breakpoint hit" "${BREAKPOINT_URL}" \
  '{"address":4660,"source":"test-http-api","cpu_state":{"a":1}}' 200
post_endpoint "state update" "${STATE_URL}" \
  '{"source":"test-http-api","state":{"status":"ok"}}' 200

post_endpoint "window show" "${WINDOW_SHOW_URL}" '{}' 200 501
post_endpoint "window hide" "${WINDOW_HIDE_URL}" '{}' 200 501
