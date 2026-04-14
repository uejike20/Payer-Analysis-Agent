#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


def normalize_drug_name(name: str) -> str:
    cleaned = name.strip().lstrip("-").strip()
    return " ".join(part.capitalize() for part in cleaned.split())


def normalize_drugs(drugs: dict[str, Any]) -> dict[str, Any]:
    normalized: dict[str, Any] = {}
    for raw_name, details in drugs.items():
        key = normalize_drug_name(raw_name)
        # Keep the first complete entry encountered for a normalized drug key.
        if key not in normalized:
            normalized[key] = details
    return dict(sorted(normalized.items(), key=lambda item: item[0]))


def looks_like_phone_number(value: Any) -> bool:
    if not isinstance(value, str):
        return False
    digits = re.sub(r"\D", "", value)
    # Some malformed values appear as local-number fragments like "267-3300".
    return len(digits) >= 7


def looks_like_case_id(value: Any) -> bool:
    if not isinstance(value, str):
        return False
    # Denial letters often include case IDs like PA-1847, which are not form names.
    return re.fullmatch(r"PA-\d{3,}", value.strip(), flags=re.IGNORECASE) is not None


def looks_like_form_code(value: Any) -> bool:
    if not isinstance(value, str):
        return False
    token = value.strip().upper()
    return ("PA" in token or "AUTH" in token) and "-" in token and not looks_like_case_id(token)


def extract_form_code_from_text(text: Any) -> str | None:
    if not isinstance(text, str) or not text.strip():
        return None
    # Capture form-like tokens such as UHC-PA-200, HUM-AUTH-2026, CG-PA-002.
    matches = re.findall(r"\b[A-Z]{2,}(?:-[A-Z0-9]{2,})+\b", text.upper())
    for token in matches:
        if looks_like_form_code(token):
            return token
    return None


def is_placeholder_denial_reason(value: Any) -> bool:
    if not isinstance(value, str):
        return False
    normalized = value.strip().upper()
    placeholders = {
        "REASON:",
        "REASON FOR DENIAL:",
        "DENIAL REASON:",
    }
    return normalized in placeholders


def clean_route(route: dict[str, Any]) -> dict[str, Any]:
    cleaned = dict(route)
    drugs = cleaned.get("drugs")
    if isinstance(drugs, dict):
        cleaned["drugs"] = normalize_drugs(drugs)

    # Guard against known parser mistakes where turnaround days gets a phone number.
    turnaround_fax = cleaned.get("turnaround_fax_days")
    if looks_like_phone_number(turnaround_fax):
        cleaned["turnaround_fax_days"] = None
        cleaned["turnaround_fax_days_note"] = "Removed invalid phone-number-like value."

    submission_method = {
        "methods": cleaned.get("submission_methods"),
        "portal_url": cleaned.get("portal_url"),
        "portal_note": cleaned.get("portal_note"),
    }

    contact_info = {
        "fax_number": cleaned.get("fax_number"),
        "fax_number_general": cleaned.get("fax_number_general"),
        "fax_number_specialty": cleaned.get("fax_number_specialty"),
        "fax_number_recommended": cleaned.get("fax_number_recommended"),
        "phone_status_only": cleaned.get("phone_status_only"),
        "phone_urgent": cleaned.get("phone_urgent"),
        "phone_hours": cleaned.get("phone_hours"),
        "appeal_fax": cleaned.get("appeal_fax"),
        "appeal_phone": cleaned.get("appeal_phone"),
    }

    pa_form_raw = cleaned.get("pa_form")
    pa_form_old = cleaned.get("pa_form_old")
    extracted_from_note = extract_form_code_from_text(cleaned.get("pa_form_note"))
    extracted_from_policy = extract_form_code_from_text(cleaned.get("chart_note_policy_update"))
    extracted_from_portal = extract_form_code_from_text(cleaned.get("portal_note"))

    pa_form = None
    for candidate in (
        pa_form_raw,
        pa_form_old,
        extracted_from_note,
        extracted_from_policy,
        extracted_from_portal,
    ):
        if looks_like_form_code(candidate):
            pa_form = candidate
            break

    if pa_form is None and isinstance(pa_form_raw, str) and pa_form_raw.strip():
        # Keep the source-provided value when no better form code exists.
        pa_form = pa_form_raw

    required_documents = {
        "pa_form": pa_form,
        "pa_form_old": cleaned.get("pa_form_old"),
        "pa_form_note": cleaned.get("pa_form_note"),
        "chart_note_window_days": cleaned.get("chart_note_window_days"),
        "lab_window_days": cleaned.get("lab_window_days"),
    }

    restrictions = {
        "denial_reason": (
            None
            if is_placeholder_denial_reason(cleaned.get("denial_reason"))
            else cleaned.get("denial_reason")
        ),
        "chart_note_policy_update": cleaned.get("chart_note_policy_update"),
        "fax_old_status": cleaned.get("fax_old_status"),
        "appeal_deadline_days": cleaned.get("appeal_deadline_days"),
        "turnaround_standard_days": cleaned.get("turnaround_standard_days"),
        "turnaround_portal_days": cleaned.get("turnaround_portal_days"),
        "turnaround_fax_days": cleaned.get("turnaround_fax_days"),
        "turnaround_urgent_hours": cleaned.get("turnaround_urgent_hours"),
    }
    if "turnaround_fax_days_note" in cleaned:
        restrictions["turnaround_fax_days_note"] = cleaned["turnaround_fax_days_note"]

    def compact(section: dict[str, Any]) -> dict[str, Any]:
        return {k: v for k, v in section.items() if v not in (None, "", [], {})}

    def compact_drug_details(drug_details: dict[str, Any]) -> dict[str, Any]:
        # Remove internal provenance metadata for presentation output.
        dropped_keys = {"_selected_from", "selected_from", "confidence"}
        return {
            key: value
            for key, value in drug_details.items()
            if key not in dropped_keys and value not in (None, "", [], {})
        }

    drug_specific_requirements: dict[str, Any] = {}
    for drug_name, details in cleaned.get("drugs", {}).items():
        if isinstance(details, dict):
            compacted = compact_drug_details(details)
            if compacted:
                drug_specific_requirements[drug_name] = compacted
        elif details not in (None, "", [], {}):
            drug_specific_requirements[drug_name] = details

    return {
        "submission_method": compact(submission_method),
        "contact_info": compact(contact_info),
        "required_documents": compact(required_documents),
        "drug_specific_requirements": drug_specific_requirements,
        "restrictions": compact(restrictions),
    }


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create a clean, readable best-answer JSON from reconciled route output."
    )
    parser.add_argument(
        "--input",
        default="outputs/reconciled_routes.json",
        help="Path to reconciled routes JSON.",
    )
    parser.add_argument(
        "--output",
        default="outputs/best_answer.json",
        help="Path for the cleaned best answer output.",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)
    data = read_json(input_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    result: dict[str, Any] = {}
    for payer_key, payer_payload in data.items():
        best_route = payer_payload.get("best_route", {})
        result[payer_key] = {
            "payer": payer_payload.get("payer"),
            "best_route": clean_route(best_route),
        }

    output_path.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    main()
