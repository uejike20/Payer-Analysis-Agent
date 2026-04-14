#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from copy import deepcopy
from datetime import datetime
from pathlib import Path
from typing import Any

from format_best_answer import clean_route


MONTH_FORMATS = [
    "%Y-%m-%d",
    "%B %d, %Y",
    "%b %d, %Y",
    "%B %Y",
    "%b %Y",
    "%B %d %Y",
    "%b %d %Y",
]

FIELD_GROUPS = {
    "submission": [
        "submission_methods",
        "portal_url",
        "portal_note",
        "fax_number",
        "fax_number_specialty",
        "fax_number_general",
        "fax_number_old",
        "fax_old_status",
        "fax_note",
        "phone_status_only",
        "phone_urgent",
        "phone_hours",
    ],
    "required_documents": [
        "pa_form",
        "pa_form_old",
        "pa_form_note",
        "chart_note_window_days",
        "chart_note_policy_update",
        "lab_window_days",
    ],
    "turnaround": [
        "turnaround_standard_days",
        "turnaround_portal_days",
        "turnaround_fax_days",
        "turnaround_urgent_hours",
        "pend_period_days",
    ],
    "appeals_and_restrictions": [
        "appeal_fax",
        "appeal_phone",
        "appeal_deadline_days",
        "denial_reason",
    ],
}


def parse_date(raw: str | None) -> datetime:
    """Parse flexible source dates; unknown formats sort as oldest."""
    if not raw:
        return datetime.min

    value = raw.strip().rstrip(".")
    for fmt in MONTH_FORMATS:
        try:
            return datetime.strptime(value, fmt)
        except ValueError:
            continue

    if len(value) == 4 and value.isdigit():
        return datetime(int(value), 1, 1)

    return datetime.min


def freshness_key(source: dict[str, Any]) -> tuple[datetime, datetime]:
    """Return recency tuple used for sorting sources newest-first."""
    return (
        parse_date(source.get("source_date")),
        parse_date(source.get("retrieved_date")),
    )


def normalize_value(value: Any) -> Any:
    """Normalize nested values into hashable forms for de-duplication."""
    if isinstance(value, list):
        return tuple(value)
    if isinstance(value, dict):
        return tuple(sorted((k, normalize_value(v)) for k, v in value.items()))
    return value


def confidence_for_source(source_type: str, chosen_is_latest: bool) -> str:
    """Assign a lightweight confidence label based on source type and freshness."""
    if source_type in {"phone_transcript", "denial_letter"} and chosen_is_latest:
        return "high"
    if chosen_is_latest:
        return "medium-high"
    if source_type == "provider_manual":
        return "low"
    return "medium"


def collect_candidates(sources: list[dict[str, Any]], field: str) -> list[dict[str, Any]]:
    """Collect all non-empty occurrences of a field with source metadata."""
    candidates: list[dict[str, Any]] = []
    for source in sources:
        data = source.get("data", {})
        if field in data and data[field] not in ("", None, []):
            candidates.append(
                {
                    "value": deepcopy(data[field]),
                    "source_id": source.get("source_id"),
                    "source_type": source.get("source_type"),
                    "source_name": source.get("source_name"),
                    "source_date": source.get("source_date"),
                    "retrieved_date": source.get("retrieved_date"),
                    "freshness": freshness_key(source),
                }
            )
    candidates.sort(key=lambda item: item["freshness"], reverse=True)
    return candidates


def choose_latest(candidates: list[dict[str, Any]]) -> dict[str, Any] | None:
    """Pick the freshest candidate for a field."""
    if not candidates:
        return None
    return candidates[0]


def build_conflict_entry(field: str, chosen: dict[str, Any], candidates: list[dict[str, Any]]) -> dict[str, Any] | None:
    """Create a conflict report entry when multiple distinct values exist."""
    distinct = []
    seen = set()
    for candidate in candidates:
        normalized = normalize_value(candidate["value"])
        if normalized in seen:
            continue
        seen.add(normalized)
        distinct.append(
            {
                "value": candidate["value"],
                "source_id": candidate["source_id"],
                "source_type": candidate["source_type"],
                "source_date": candidate["source_date"],
            }
        )

    if len(distinct) <= 1:
        return None

    return {
        "field": field,
        "chosen_value": chosen["value"],
        "chosen_source_id": chosen["source_id"],
        "chosen_source_type": chosen["source_type"],
        "resolution_rule": "latest_source_date_then_retrieved_date",
        "candidates": distinct,
    }


def reconcile_drugs(sources: list[dict[str, Any]]) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    """Reconcile drug requirements and capture per-drug conflicts."""
    drug_sources: dict[str, list[dict[str, Any]]] = defaultdict(list)

    for source in sources:
        for drug_name, drug_data in source.get("data", {}).get("drugs", {}).items():
            drug_sources[drug_name].append(
                {
                    "drug_name": drug_name,
                    "drug_data": deepcopy(drug_data),
                    "source_id": source.get("source_id"),
                    "source_type": source.get("source_type"),
                    "source_date": source.get("source_date"),
                    "retrieved_date": source.get("retrieved_date"),
                    "freshness": freshness_key(source),
                }
            )

    reconciled_drugs: dict[str, Any] = {}
    conflicts: list[dict[str, Any]] = []

    for drug_name, candidates in drug_sources.items():
        candidates.sort(key=lambda item: item["freshness"], reverse=True)
        chosen = candidates[0]
        reconciled_drugs[drug_name] = {
            **chosen["drug_data"],
            "_selected_from": {
                "source_id": chosen["source_id"],
                "source_type": chosen["source_type"],
                "source_date": chosen["source_date"],
                "confidence": confidence_for_source(chosen["source_type"], True),
            },
        }

        distinct = []
        seen = set()
        for candidate in candidates:
            normalized = normalize_value(candidate["drug_data"])
            if normalized in seen:
                continue
            seen.add(normalized)
            distinct.append(
                {
                    "source_id": candidate["source_id"],
                    "source_type": candidate["source_type"],
                    "source_date": candidate["source_date"],
                    "value": candidate["drug_data"],
                }
            )

        if len(distinct) > 1:
            conflicts.append(
                {
                    "field": f"drugs.{drug_name}",
                    "chosen_value": chosen["drug_data"],
                    "chosen_source_id": chosen["source_id"],
                    "chosen_source_type": chosen["source_type"],
                    "resolution_rule": "latest_source_date_then_retrieved_date",
                    "candidates": distinct,
                }
            )

    return reconciled_drugs, conflicts


def reconcile_payer(payer_key: str, payer_block: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any]]:
    """Build reconciled route + conflict report for one payer."""
    sources = sorted(payer_block.get("sources", []), key=freshness_key, reverse=True)
    latest_source = sources[0] if sources else {}

    route = {
        "payer": payer_block.get("payer"),
        "payer_key": payer_key,
        "selection_policy": "latest_source_date_then_retrieved_date",
        "latest_source": {
            "source_id": latest_source.get("source_id"),
            "source_type": latest_source.get("source_type"),
            "source_name": latest_source.get("source_name"),
            "source_date": latest_source.get("source_date"),
            "retrieved_date": latest_source.get("retrieved_date"),
        },
    }

    conflict_report = {
        "payer": payer_block.get("payer"),
        "payer_key": payer_key,
        "selection_policy": "latest_source_date_then_retrieved_date",
        "field_conflicts": [],
    }

    for group_name, fields in FIELD_GROUPS.items():
        route[group_name] = {}
        for field in fields:
            candidates = collect_candidates(sources, field)
            chosen = choose_latest(candidates)
            if not chosen:
                continue

            route[group_name][field] = {
                "value": chosen["value"],
                "selected_from": {
                    "source_id": chosen["source_id"],
                    "source_type": chosen["source_type"],
                    "source_name": chosen["source_name"],
                    "source_date": chosen["source_date"],
                    "retrieved_date": chosen["retrieved_date"],
                    "confidence": confidence_for_source(chosen["source_type"], chosen["source_id"] == latest_source.get("source_id")),
                },
                "reasoning": "Selected from the freshest source that contains this field.",
            }

            conflict = build_conflict_entry(field, chosen, candidates)
            if conflict:
                conflict_report["field_conflicts"].append(conflict)

    reconciled_drugs, drug_conflicts = reconcile_drugs(sources)
    route["drug_requirements"] = reconciled_drugs
    conflict_report["field_conflicts"].extend(drug_conflicts)

    route["conflict_count"] = len(conflict_report["field_conflicts"])
    return route, conflict_report


def extract_best_route(payer_payload: dict[str, Any]) -> dict[str, Any]:
    """Normalize different reconciler schemas to a flat best-route shape."""
    # Support both reconcile output schemas:
    # 1) {"best_route": {...}} from reconcile_routes.py
    # 2) grouped fields from reconcile_latest.py
    direct = payer_payload.get("best_route")
    if isinstance(direct, dict) and direct:
        return direct

    combined: dict[str, Any] = {}
    for group_key in (
        "submission",
        "required_documents",
        "turnaround",
        "appeals_and_restrictions",
    ):
        group = payer_payload.get(group_key, {})
        if not isinstance(group, dict):
            continue
        for field_name, field_payload in group.items():
            if isinstance(field_payload, dict) and "value" in field_payload:
                combined[field_name] = field_payload["value"]

    drug_requirements = payer_payload.get("drug_requirements")
    if isinstance(drug_requirements, dict):
        combined["drugs"] = drug_requirements

    return combined


def main() -> None:
    """CLI entrypoint for latest-source reconciliation workflow."""
    parser = argparse.ArgumentParser(description="Reconcile payer route data using the freshest available source.")
    parser.add_argument(
        "--input",
        default="outputs/extracted_route_data.json",
        help="Path to extracted structured payer data.",
    )
    parser.add_argument(
        "--routes-output",
        default="outputs/reconciled_routes.json",
        help="Path for reconciled best-route output.",
    )
    parser.add_argument(
        "--conflicts-output",
        default="outputs/conflicts_report.json",
        help="Path for conflict report output.",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    raw_bytes = input_path.read_bytes()
    try:
        raw_text = raw_bytes.decode("utf-8")
    except UnicodeDecodeError:
        try:
            raw_text = raw_bytes.decode("cp1252")
        except UnicodeDecodeError:
            raw_text = raw_bytes.decode("latin-1")
    data = json.loads(raw_text)

    reconciled_routes: dict[str, Any] = {}
    conflict_reports: dict[str, Any] = {}

    for payer_key, payer_block in data.items():
        route, conflicts = reconcile_payer(payer_key, payer_block)
        reconciled_routes[payer_key] = route
        conflict_reports[payer_key] = conflicts

    routes_output = Path(args.routes_output)
    conflicts_output = Path(args.conflicts_output)
    routes_output.parent.mkdir(parents=True, exist_ok=True)
    conflicts_output.parent.mkdir(parents=True, exist_ok=True)
    routes_output.write_text(json.dumps(reconciled_routes, indent=2))
    conflicts_output.write_text(json.dumps(conflict_reports, indent=2))

    best_answer_output = routes_output.with_name("best_answer.json")
    best_answer: dict[str, Any] = {}
    for payer_key, payer_payload in reconciled_routes.items():
        best_route = extract_best_route(payer_payload)
        best_answer[payer_key] = {
            "payer": payer_payload.get("payer"),
            "best_route": clean_route(best_route),
        }
    best_answer_output.write_text(json.dumps(best_answer, indent=2))


if __name__ == "__main__":
    main()
