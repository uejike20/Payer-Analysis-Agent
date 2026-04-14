#!/usr/bin/env python3
import json
import re
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any

from format_best_answer import clean_route


ROOT = Path(__file__).resolve().parent
OUTPUT_DIR = ROOT / "outputs"
INPUT_PATH = OUTPUT_DIR / "extracted_route_data.json"
RECONCILED_PATH = OUTPUT_DIR / "reconciled_routes.json"
CONFLICTS_PATH = OUTPUT_DIR / "conflicts_report.json"
BEST_ANSWER_PATH = OUTPUT_DIR / "best_answer.json"


@dataclass(order=True)
class SourceStamp:
    rank: tuple[int, int, int]
    retrieved_rank: tuple[int, int, int]
    source_id: str


MONTHS = {
    "january": 1,
    "february": 2,
    "march": 3,
    "april": 4,
    "may": 5,
    "june": 6,
    "july": 7,
    "august": 8,
    "september": 9,
    "october": 10,
    "november": 11,
    "december": 12,
}


def parse_date(value: str | None) -> tuple[int, int, int]:
    if not value:
        return (0, 0, 0)

    value = value.strip().strip('"')
    for fmt in ("%Y-%m-%d", "%B %d, %Y", "%b %d, %Y"):
        try:
            parsed = datetime.strptime(value, fmt)
            return (parsed.year, parsed.month, parsed.day)
        except ValueError:
            pass

    month_year = re.fullmatch(r"([A-Za-z]+)\s+(\d{4})", value)
    if month_year:
        month = MONTHS.get(month_year.group(1).lower(), 0)
        return (int(month_year.group(2)), month, 1)

    return (0, 0, 0)


def normalize(value: Any) -> str:
    if isinstance(value, list):
        return json.dumps(value, sort_keys=True)
    if isinstance(value, dict):
        return json.dumps(value, sort_keys=True)
    if value is None:
        return "null"
    return str(value)


def normalize_drug_name(name: str) -> str:
    return " ".join(part.capitalize() for part in name.replace("_", " ").split())


def source_stamp(source: dict[str, Any]) -> SourceStamp:
    return SourceStamp(
        rank=parse_date(source.get("source_date")),
        retrieved_rank=parse_date(source.get("retrieved_date")),
        source_id=source.get("source_id", ""),
    )


def pick_latest(sources: list[dict[str, Any]], predicate) -> dict[str, Any] | None:
    matching = [source for source in sources if predicate(source)]
    if not matching:
        return None
    return max(matching, key=lambda source: (source_stamp(source).rank, source_stamp(source).retrieved_rank, source.get("source_id", "")))


def collect_scalar_conflicts(sources: list[dict[str, Any]], field: str) -> tuple[Any, dict[str, Any] | None, list[dict[str, Any]]]:
    candidates: list[tuple[dict[str, Any], Any]] = []
    for source in sources:
        data = source.get("data", {})
        if field in data and data[field] not in ("", None, [], {}):
            candidates.append((source, data[field]))

    if not candidates:
        return None, None, []

    latest_source, chosen_value = max(
        candidates,
        key=lambda item: (source_stamp(item[0]).rank, source_stamp(item[0]).retrieved_rank, item[0].get("source_id", "")),
    )

    distinct = {}
    for source, value in candidates:
        distinct.setdefault(normalize(value), []).append(
            {
                "source_id": source.get("source_id"),
                "source_type": source.get("source_type"),
                "source_date": source.get("source_date"),
                "retrieved_date": source.get("retrieved_date"),
                "value": value,
            }
        )

    conflicts = []
    if len(distinct) > 1:
        for group in distinct.values():
            conflicts.extend(group)

    return chosen_value, latest_source, conflicts


def collect_drug_conflicts(sources: list[dict[str, Any]]) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    all_drugs: dict[str, list[tuple[dict[str, Any], dict[str, Any]]]] = {}

    for source in sources:
        source_drugs = source.get("data", {}).get("drugs", {})
        if not isinstance(source_drugs, dict):
            continue
        for drug_name, details in source_drugs.items():
            all_drugs.setdefault(normalize_drug_name(drug_name), []).append((source, details))

    chosen_drugs: dict[str, Any] = {}
    conflicts: list[dict[str, Any]] = []

    for drug_name, candidates in sorted(all_drugs.items()):
        latest_source, latest_details = max(
            candidates,
            key=lambda item: (source_stamp(item[0]).rank, source_stamp(item[0]).retrieved_rank, item[0].get("source_id", "")),
        )
        chosen_drugs[drug_name] = latest_details

        distinct = {}
        for source, details in candidates:
            distinct.setdefault(normalize(details), []).append(
                {
                    "source_id": source.get("source_id"),
                    "source_type": source.get("source_type"),
                    "source_date": source.get("source_date"),
                    "retrieved_date": source.get("retrieved_date"),
                    "value": details,
                }
            )

        if len(distinct) > 1:
            alternatives = []
            for group in distinct.values():
                alternatives.extend(group)
            conflicts.append(
                {
                    "field": f"drugs.{drug_name}",
                    "chosen_value": latest_details,
                    "chosen_source_id": latest_source.get("source_id"),
                    "chosen_source_type": latest_source.get("source_type"),
                    "reasoning": "Selected the latest source mentioning this drug.",
                    "alternatives": alternatives,
                }
            )

    return chosen_drugs, conflicts


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    raw_bytes = INPUT_PATH.read_bytes()
    raw_text = raw_bytes.decode("utf-8", errors="replace")
    payload = json.loads(raw_text)

    reconciled: dict[str, Any] = {}
    conflicts_report: dict[str, Any] = {}

    tracked_fields = [
        "submission_methods",
        "fax_number",
        "fax_number_old",
        "fax_old_status",
        "fax_number_general",
        "fax_number_specialty",
        "fax_number_recommended",
        "portal_url",
        "portal_note",
        "phone_status_only",
        "phone_urgent",
        "phone_hours",
        "pa_form",
        "pa_form_old",
        "pa_form_note",
        "chart_note_window_days",
        "chart_note_policy_update",
        "lab_window_days",
        "turnaround_standard_days",
        "turnaround_portal_days",
        "turnaround_fax_days",
        "turnaround_urgent_hours",
        "appeal_fax",
        "appeal_phone",
        "appeal_deadline_days",
        "coverage_states",
        "denial_reason",
    ]

    for payer_key, payer_data in payload.items():
        sources = payer_data.get("sources", [])
        best_route: dict[str, Any] = {}
        field_sources: dict[str, Any] = {}
        payer_conflicts: list[dict[str, Any]] = []

        for field in tracked_fields:
            chosen_value, chosen_source, conflicts = collect_scalar_conflicts(sources, field)
            if chosen_source is None:
                continue

            best_route[field] = chosen_value
            field_sources[field] = {
                "source_id": chosen_source.get("source_id"),
                "source_type": chosen_source.get("source_type"),
                "source_date": chosen_source.get("source_date"),
                "retrieved_date": chosen_source.get("retrieved_date"),
                "reasoning": "Selected the latest available source containing this field.",
            }

            if conflicts:
                payer_conflicts.append(
                    {
                        "field": field,
                        "chosen_value": chosen_value,
                        "chosen_source_id": chosen_source.get("source_id"),
                        "chosen_source_type": chosen_source.get("source_type"),
                        "reasoning": "Selected the latest available source containing this field.",
                        "alternatives": conflicts,
                    }
                )

        chosen_drugs, drug_conflicts = collect_drug_conflicts(sources)
        if chosen_drugs:
            best_route["drugs"] = chosen_drugs
        payer_conflicts.extend(drug_conflicts)

        latest_source = max(
            sources,
            key=lambda source: (source_stamp(source).rank, source_stamp(source).retrieved_rank, source.get("source_id", "")),
        )

        reconciled[payer_key] = {
            "payer": payer_data.get("payer"),
            "strategy": {
                "name": "latest-source-wins",
                "description": "For each field, choose the most recent source_date, then break ties with retrieved_date.",
            },
            "latest_source_seen": {
                "source_id": latest_source.get("source_id"),
                "source_type": latest_source.get("source_type"),
                "source_date": latest_source.get("source_date"),
                "retrieved_date": latest_source.get("retrieved_date"),
            },
            "best_route": best_route,
            "field_sources": field_sources,
        }

        conflicts_report[payer_key] = {
            "payer": payer_data.get("payer"),
            "conflict_count": len(payer_conflicts),
            "conflicts": payer_conflicts,
        }

    with RECONCILED_PATH.open("w") as outfile:
        json.dump(reconciled, outfile, indent=2)
        outfile.write("\n")

    with CONFLICTS_PATH.open("w") as outfile:
        json.dump(conflicts_report, outfile, indent=2)
        outfile.write("\n")

    # Keep a concise, human-readable final answer snapshot in sync.
    best_answer: dict[str, Any] = {}
    for payer_key, payer_payload in reconciled.items():
        best_route = payer_payload.get("best_route", {})
        best_answer[payer_key] = {
            "payer": payer_payload.get("payer"),
            "best_route": clean_route(best_route),
        }

    with BEST_ANSWER_PATH.open("w") as outfile:
        json.dump(best_answer, outfile, indent=2)
        outfile.write("\n")


if __name__ == "__main__":
    main()
