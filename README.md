# Payer Analysis Pipeline

`Payer Analysis Pipeline` is a C++ data-processing agent that reads payer source documents and writes normalized route output JSON.

The agent runs continuously, retries on failure, and persists basic state between runs.

## What It Does

- Reads payer source files under `AnalysisPipeline/payer_sources/<payer_key>/`
- Extracts and composes structured route data
- Writes output JSON files under `AnalysisPipeline/outputs/`
- Stores agent runtime state in `AnalysisPipeline/outputs/agent_state.txt`

## Prerequisites

Use either Docker (recommended) or native C++ tooling.

- Docker Desktop (running), with `docker compose`
- OR `g++` with C++17 support

## Quick Start (Docker, Recommended)

From the repository root:

```bash
docker compose up -d --build
```

Check status:

```bash
docker compose ps
```

View logs:

```bash
docker compose logs -f
```

Stop:

```bash
docker compose down
```

## Custom Test Data (Bind Mounts)

If you want to run the image against your own local payer source files, mount your input and output folders:

```bash
docker run --rm \
  -v "/absolute/path/to/your/payer_sources:/app/AnalysisPipeline/payer_sources" \
  -v "/absolute/path/to/your/output_folder:/app/AnalysisPipeline/outputs" \
  payer-analysis-agent:latest
```

Example:

```bash
docker run --rm \
  -v "/Users/utibeejike/Downloads/my-custom-payer-sources:/app/AnalysisPipeline/payer_sources" \
  -v "/Users/utibeejike/Downloads/my-agent-outputs:/app/AnalysisPipeline/outputs" \
  payer-analysis-agent:latest
```

Your source folder should contain payer directories (for example `aetna/`, `cigna/`, `humana/`) with source files, and generated outputs will be written to the mounted output folder.

## Run Locally (Native)

From `AnalysisPipeline/`:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic \
  main.cpp DenialLetter.cpp WebPage.cpp PhoneTranscript.cpp Payers.cpp ProviderManual.cpp \
  -o auth_pipeline_agent
./auth_pipeline_agent
```

## Expected Runtime Behavior

- Agent starts and logs payer processing (Aetna, UnitedHealthcare, etc.)
- Agent auto-discovers payer directories in `AnalysisPipeline/payer_sources/` on every cycle
- On success, it waits 5 minutes before the next cycle
- On failure, it retries after a short backoff
- After repeated failures, it exits (so orchestrators like Docker can restart it)

## Add a New Payer (Example: Anthem)

1. Create folder:
   - `AnalysisPipeline/payer_sources/anthem/`
2. Add one or more source files (same names used by existing payers):
   - `provider_manual.txt`
   - `phone_transcript.txt`
   - `web_page.txt`
   - `denial_letter.txt`
3. Wait for next cycle (or restart container) and the agent will include `anthem` automatically in `AnalysisPipeline/outputs/extracted_route_data.json`

## Project Layout

- `AnalysisPipeline/main.cpp` - agent loop and orchestration
- `AnalysisPipeline/Payers.*` - payer-level source aggregation
- `AnalysisPipeline/ProviderManual.*`, `PhoneTranscript.*`, `WebPage.*`, `DenialLetter.*` - source readers
- `AnalysisPipeline/payer_sources/` - source input data files only
- `AnalysisPipeline/outputs/` - generated outputs (`extracted_route_data.json`, `reconciled_routes.json`, `conflicts_report.json`, `best_answer.json`)
- `Dockerfile` - multi-stage container build
- `docker-compose.yml` - local service orchestration
