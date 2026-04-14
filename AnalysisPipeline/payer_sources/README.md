# Payer Source Data

This folder contains synthetic source materials for 5 insurance payers. The data simulates what a discovery system would collect from different channels when trying to figure out how to submit a prior authorization.

## Folder Structure

```
payer_sources/
├── aetna/
│   ├── provider_manual.txt      — Extracted text from the payer's official provider manual
│   ├── phone_transcript.txt     — Transcript of a phone call to the payer's PA department
│   ├── web_page.txt             — Scraped content from the payer's provider-facing website
│   └── denial_letter.txt        — A denial letter from our internal database
├── unitedhealthcare/
│   └── (same 4 source types)
├── cigna/
│   └── (same 4 source types)
├── blue_cross_blue_shield/
│   └── (same 4 source types)
├── humana/
│   └── (same 4 source types)
├── extracted_route_data.json    — Pre-extracted structured data from all sources (for Option A)
└── README.md                    — This file
```

## Source Types

Each payer folder has 4 files representing different discovery channels:

- **provider_manual.txt** — Text extracted from the payer's official provider manual or medical policy document. These are typically PDFs that get updated annually. The content may be out of date.
- **phone_transcript.txt** — A transcript of a phone call to the payer's prior authorization department. These represent the most real-time information but are only as reliable as the individual rep.
- **web_page.txt** — Content scraped from the payer's provider-facing website. Includes page metadata like the URL and last-updated date. Web pages may lag behind actual policy changes.
- **denial_letter.txt** — A denial letter received for one of our cases. These often reveal current requirements and updated processes because they cite the specific policy that was applied.

Each file includes a metadata header showing the source, date of the source material, and when it was retrieved.

## Pre-Extracted Data (for Option A candidates)

`extracted_route_data.json` contains structured data already extracted from the raw sources above. Each payer has an array of source records with the extracted fields and metadata. **The conflicts between sources are preserved** — your job is to reconcile them and present them clearly, not to re-extract from the raw text.

## Important Notes

- **Sources intentionally conflict.** Fax numbers, form versions, documentation windows, and turnaround times differ across sources for the same payer. This is realistic — payer information changes frequently and different channels reflect updates at different speeds.
- **Some sources are stale.** Pay attention to the dates. A provider manual from 2024 may have been superseded by a policy change announced in a 2026 phone call.
- **All data is synthetic.** Phone numbers, member IDs, case numbers, and patient information are fabricated.
