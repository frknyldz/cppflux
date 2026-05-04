#!/usr/bin/env bash
set -euo pipefail

DURATION=${DURATION:-10}
CONNECTIONS=${CONNECTIONS:-100}
THREADS=${THREADS:-4}
BINARY=${BINARY:-../build/examples/server}
RUST_BIN=${RUST_BIN:-./rust_server/target/release/server_rs}

WRK="wrk -t${THREADS} -c${CONNECTIONS} -d${DURATION}s --latency"

cleanup() {
    kill "$CPP_PID" "$GO_PID" "$NODE_PID" "$RUST_PID" 2>/dev/null || true
    wait 2>/dev/null || true
}
trap cleanup EXIT

# ── Build servers if needed ───────────────────────────────────────────────────
[ -f server_go ] || go build -o server_go server.go
[ -f "$RUST_BIN" ] || (cd rust_server && cargo build --release -q)

# ── Start servers ─────────────────────────────────────────────────────────────
CPPFLUX_QUIET=1 "$BINARY" > /dev/null 2>&1 & CPP_PID=$!
./server_go                > /dev/null 2>&1 & GO_PID=$!
node server.js             > /dev/null 2>&1 & NODE_PID=$!
"$RUST_BIN"                > /dev/null 2>&1 & RUST_PID=$!

sleep 0.5
until curl -sf http://localhost:8080/ping > /dev/null; do sleep 0.1; done
until curl -sf http://localhost:8081/ping > /dev/null; do sleep 0.1; done
until curl -sf http://localhost:8082/ping > /dev/null; do sleep 0.1; done
until curl -sf http://localhost:8083/ping > /dev/null; do sleep 0.1; done

# ── /ping — pure throughput ───────────────────────────────────────────────────
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  /ping  (${CONNECTIONS} conns, ${THREADS} wrk threads, ${DURATION}s)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo ""
echo "── cppflux  :8080 ───────────────────────────────────────"
$WRK http://localhost:8080/ping

echo ""
echo "── Go       :8081 ───────────────────────────────────────"
$WRK http://localhost:8081/ping

echo ""
echo "── Node.js  :8082 ───────────────────────────────────────"
$WRK http://localhost:8082/ping

echo ""
echo "── Rust     :8083 ───────────────────────────────────────"
$WRK http://localhost:8083/ping

# ── /pipeline — async latency chain (20 ms of async I/O per request) ─────────
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  /pipeline  (${CONNECTIONS} conns, ${THREADS} wrk threads, ${DURATION}s)"
echo "  Each request: 2 × async_sleep(10ms) = ~20ms latency"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo ""
echo "── cppflux  :8080 ───────────────────────────────────────"
$WRK http://localhost:8080/pipeline

echo ""
echo "── Go       :8081 ───────────────────────────────────────"
$WRK http://localhost:8081/pipeline

echo ""
echo "── Node.js  :8082 ───────────────────────────────────────"
$WRK http://localhost:8082/pipeline

echo ""
echo "── Rust     :8083 ───────────────────────────────────────"
$WRK http://localhost:8083/pipeline
