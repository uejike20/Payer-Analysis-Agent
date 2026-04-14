#!/usr/bin/env bash
set -euo pipefail

MODE="${AUTHPIPELINE_MODE:-oneshot}"

if [[ "$MODE" == "continuous" ]]; then
  exec /usr/local/bin/authpipeline
fi

# One-shot mode: extract once, then immediately generate reconciled outputs.
AUTHPIPELINE_RUN_ONCE=1 /usr/local/bin/authpipeline

python3 /app/AnalysisPipeline/reconcile_latest.py \
  --input /app/AnalysisPipeline/outputs/extracted_route_data.json \
  --routes-output /app/AnalysisPipeline/outputs/reconciled_routes.json \
  --conflicts-output /app/AnalysisPipeline/outputs/conflicts_report.json
