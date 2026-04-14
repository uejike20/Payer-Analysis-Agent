#include "WebPage.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

using namespace std;

namespace {
/** Trim leading/trailing whitespace. */
string trim(const string& text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == string::npos) {
        return "";
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

/** Return lowercase copy for case-insensitive matching. */
string toLowerCopy(const string& text) {
    string lowered = text;
    for (size_t i = 0; i < lowered.size(); ++i) {
        lowered[i] = static_cast<char>(tolower(static_cast<unsigned char>(lowered[i])));
    }
    return lowered;
}

/** Map payer key to stable source ID prefix. */
string payerPrefix(const string& payer_key) {
    if (payer_key == "aetna") return "AET";
    if (payer_key == "unitedhealthcare") return "UHC";
    if (payer_key == "cigna") return "CIG";
    if (payer_key == "blue_cross_blue_shield") return "BCBS";
    if (payer_key == "humana") return "HUM";
    return "SRC";
}

/** Join fields with separator when building JSON fragments. */
string joinFields(const vector<string>& fields, const string& separator) {
    string joined;
    for (size_t i = 0; i < fields.size(); ++i) {
        joined += fields[i];
        if (i + 1 < fields.size()) {
            joined += separator;
        }
    }
    return joined;
}
}

/** Escape text for safe JSON output. */
string WebPage::jsonEscape(const string& text) const {
    string escaped;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\') {
            escaped += "\\\\";
        } else if (text[i] == '"') {
            escaped += "\\\"";
        } else if (text[i] == '\n') {
            escaped += "\\n";
        } else {
            escaped += text[i];
        }
    }
    return escaped;
}

/** Build JSON object fragment for parsed drug requirements. */
string WebPage::buildDrugJson() const {
    vector<string> entries;
    for (size_t i = 0; i < drugs.size(); ++i) {
        vector<string> fields;
        if (drugs[i].getStepTherapyRequired()) {
            fields.push_back("\"step_therapy_required\": true");
        }
        if (drugs[i].getBiosimilarPreferred()) {
            fields.push_back("\"biosimilar_required\": true");
        }
        if (drugs[i].getAuthPeriodMonths() > 0) {
            fields.push_back("\"auth_period_months\": " + to_string(drugs[i].getAuthPeriodMonths()));
        }
        if (!drugs[i].getNotes().empty()) {
            fields.push_back("\"notes\": \"" + jsonEscape(drugs[i].getNotes()) + "\"");
        }
        entries.push_back("          \"" + jsonEscape(drugs[i].getDrugName()) + "\": { " + joinFields(fields, ", ") + " }");
    }
    return "        \"drugs\": {\n" + joinFields(entries, ",\n") + "\n        }";
}

/** Read and parse web-page source for current payer. */
void WebPage::sourceReader() {
    submission_methods.clear();
    fax_number.clear();
    fax_number_specialty.clear();
    fax_number_general.clear();
    portal_url.clear();
    phone_status_only.clear();
    phone_urgent.clear();
    pa_form.clear();
    chart_note_window_days.clear();
    chart_note_policy_update.clear();
    turnaround_portal_days.clear();
    turnaround_fax_days.clear();
    turnaround_standard_days.clear();
    turnaround_urgent_hours = 0;
    drugs.clear();
    source_name.clear();
    source_date.clear();
    retrieved_date.clear();
    source_type = "web_page";
    source_id = payerPrefix(payer_key) + "-SRC-003";

    filename = "payer_sources/" + payer_key + "/web_page.txt";
    ifstream inFile(filename);
    if (!inFile.is_open()) {
        cerr << "Error opening file: " << filename << endl;
        return;
    }

    vector<string> lines;
    string line;
    while (getline(inFile, line)) {
        lines.push_back(line);
    }

    regex phonePattern("(\\(\\d{3}\\) \\d{3}-\\d{4})");
    regex formPattern("([A-Z]{2,}(?:-[A-Z0-9]+)+)");
    regex authPattern("(\\d+) months");

    for (size_t i = 0; i < lines.size(); ++i) {
        string current = trim(lines[i]);
        string lowered = toLowerCopy(current);
        smatch match;

        if (current.rfind("Source:", 0) == 0) {
            source_name = trim(current.substr(7));
        }
        if (current.rfind("URL:", 0) == 0) {
            if (source_name.empty()) {
                source_name = trim(current.substr(4));
            }
        }
        if (lowered.find("scraped date:") != string::npos) {
            size_t pos = current.find("Scraped date:");
            if (pos != string::npos) {
                retrieved_date = trim(current.substr(pos + 13));
                if (!retrieved_date.empty() && retrieved_date.back() == '.') {
                    retrieved_date.pop_back();
                }
            }
        }
        if (lowered.rfind("last updated on page:", 0) == 0) {
            source_date = trim(current.substr(21));
        }
        if (lowered.rfind("page last updated:", 0) == 0) {
            source_date = trim(current.substr(18));
        }
        if (lowered.rfind("page footer:", 0) == 0) {
            source_date = trim(current.substr(12));
            source_date.erase(remove(source_date.begin(), source_date.end(), '"'), source_date.end());
            size_t marker = source_date.find(':');
            if (marker != string::npos) {
                source_date = trim(source_date.substr(marker + 1));
            }
        }

        if (lowered.find("preferred") != string::npos && (lowered.find("portal") != string::npos || lowered.find("availity") != string::npos)) {
            submission_methods.push_back("portal");
        }
        if (lowered.find("fax") != string::npos) {
            if (find(submission_methods.begin(), submission_methods.end(), "fax") == submission_methods.end()) {
                submission_methods.push_back("fax");
            }
            if (regex_search(current, match, phonePattern)) {
                if (lowered.find("specialty") != string::npos) {
                    fax_number_specialty = match[1].str();
                } else if (lowered.find("general") != string::npos) {
                    fax_number_general = match[1].str();
                } else {
                    fax_number = match[1].str();
                }
            }
        }
        if ((lowered.find("portal") != string::npos || lowered.find("availity") != string::npos ||
             lowered.find("uhcprovider.com") != string::npos) && portal_url.empty()) {
            regex domainPattern("(www\\.[A-Za-z0-9./-]+)");
            if (regex_search(current, match, domainPattern)) {
                portal_url = match[1].str();
            }
        }
        if (lowered.find("status inquiries only") != string::npos && regex_search(current, match, phonePattern)) {
            phone_status_only = match[1].str();
        }
        if (lowered.find("urgent only") != string::npos && regex_search(current, match, phonePattern)) {
            phone_urgent = match[1].str();
            if (find(submission_methods.begin(), submission_methods.end(), "phone_urgent_only") == submission_methods.end()) {
                submission_methods.push_back("phone_urgent_only");
            }
        }
        if (regex_search(current, match, formPattern) && pa_form.empty()) {
            pa_form = match[1].str();
        }
        if (lowered.find("chart notes") != string::npos && lowered.find("within") != string::npos) {
            regex dayPattern("(\\d+) days");
            if (regex_search(current, match, dayPattern)) {
                chart_note_window_days = match[1].str();
            }
        }
        if (lowered.find("chart note window extended") != string::npos) {
            chart_note_policy_update = current;
        }
        if (lowered.find("portal submissions now receive auto-acknowledgment") != string::npos) {
            turnaround_portal_days = "1";
        }
        if (lowered.find("3-5 business days") != string::npos && turnaround_portal_days.empty()) {
            turnaround_portal_days = "3-5";
            turnaround_standard_days = "3-5";
        }
        if (lowered.find("5-7 business days") != string::npos) {
            turnaround_fax_days = "5-7";
        }
        if (lowered.find("urgent") != string::npos && lowered.find("24") != string::npos) {
            turnaround_urgent_hours = 24;
        } else if (lowered.find("urgent") != string::npos && lowered.find("48") != string::npos) {
            turnaround_urgent_hours = 48;
        }

        if (current.find('|') != string::npos && current.find("Drug") == string::npos && current.find("---") == string::npos) {
            stringstream row(current);
            string part;
            vector<string> pieces;
            while (getline(row, part, '|')) {
                string cleaned = trim(part);
                if (!cleaned.empty()) {
                    pieces.push_back(cleaned);
                }
            }
            if (pieces.size() >= 4) {
                SourceDrugInfo drug;
                drug.setDrugName(pieces[0]);
                if (toLowerCopy(pieces[1]).find("yes") != string::npos) {
                    drug.setStepTherapyRequired(true);
                }
                if (toLowerCopy(pieces[1]).find("biosimilar") != string::npos) {
                    drug.setBiosimilarPreferred(true);
                }
                if (regex_search(pieces[2], match, authPattern)) {
                    drug.setAuthPeriodMonths(stoi(match[1].str()));
                }
                drug.setNotes(pieces[3]);
                drugs.push_back(drug);
            }
        } else if (lowered.find(':') != string::npos && (lowered.find("remicade") != string::npos || lowered.find("keytruda") != string::npos ||
                   lowered.find("rituxan") != string::npos || lowered.find("entyvio") != string::npos || lowered.find("ocrevus") != string::npos ||
                   lowered.find("herceptin") != string::npos || lowered.find("tysabri") != string::npos)) {
            size_t colon = current.find(':');
            SourceDrugInfo drug;
            drug.setDrugName(trim(current.substr(0, colon)));
            string notes = trim(current.substr(colon + 1));
            if (toLowerCopy(notes).find("step therapy") != string::npos) {
                drug.setStepTherapyRequired(true);
            }
            if (toLowerCopy(notes).find("biosimilar") != string::npos) {
                drug.setBiosimilarPreferred(true);
            }
            if (regex_search(notes, match, authPattern)) {
                drug.setAuthPeriodMonths(stoi(match[1].str()));
            }
            drug.setNotes(notes);
            drugs.push_back(drug);
        }
    }

    if (retrieved_date.empty()) {
        retrieved_date = "2026-04-13";
    }
}

/** Serialize parsed web-page content as one source JSON record. */
string WebPage::toJsonRecord() const {
    vector<string> fields;
    vector<string> methods;
    for (size_t i = 0; i < submission_methods.size(); ++i) {
        methods.push_back("\"" + jsonEscape(submission_methods[i]) + "\"");
    }
    if (!methods.empty()) {
        fields.push_back("\"submission_methods\": [" + joinFields(methods, ", ") + "]");
    }
    if (!fax_number.empty()) {
        fields.push_back("\"fax_number\": \"" + jsonEscape(fax_number) + "\"");
    }
    if (!fax_number_specialty.empty()) {
        fields.push_back("\"fax_number_specialty\": \"" + jsonEscape(fax_number_specialty) + "\"");
    }
    if (!fax_number_general.empty()) {
        fields.push_back("\"fax_number_general\": \"" + jsonEscape(fax_number_general) + "\"");
    }
    if (!portal_url.empty()) {
        fields.push_back("\"portal_url\": \"" + jsonEscape(portal_url) + "\"");
    }
    if (!phone_status_only.empty()) {
        fields.push_back("\"phone_status_only\": \"" + jsonEscape(phone_status_only) + "\"");
    }
    if (!phone_urgent.empty()) {
        fields.push_back("\"phone_urgent\": \"" + jsonEscape(phone_urgent) + "\"");
    }
    if (!pa_form.empty()) {
        fields.push_back("\"pa_form\": \"" + jsonEscape(pa_form) + "\"");
    }
    if (!chart_note_window_days.empty()) {
        fields.push_back("\"chart_note_window_days\": " + chart_note_window_days);
    }
    if (!chart_note_policy_update.empty()) {
        fields.push_back("\"chart_note_policy_update\": \"" + jsonEscape(chart_note_policy_update) + "\"");
    }
    if (!turnaround_portal_days.empty()) {
        fields.push_back("\"turnaround_portal_days\": \"" + jsonEscape(turnaround_portal_days) + "\"");
    }
    if (!turnaround_fax_days.empty()) {
        fields.push_back("\"turnaround_fax_days\": \"" + jsonEscape(turnaround_fax_days) + "\"");
    }
    if (!turnaround_standard_days.empty()) {
        fields.push_back("\"turnaround_standard_days\": \"" + jsonEscape(turnaround_standard_days) + "\"");
    }
    if (turnaround_urgent_hours > 0) {
        fields.push_back("\"turnaround_urgent_hours\": " + to_string(turnaround_urgent_hours));
    }
    if (!drugs.empty()) {
        fields.push_back(buildDrugJson());
    }

    string json;
    json += "      {\n";
    json += "        \"source_id\": \"" + jsonEscape(source_id) + "\",\n";
    json += "        \"source_type\": \"" + jsonEscape(source_type) + "\",\n";
    json += "        \"source_name\": \"" + jsonEscape(source_name) + "\",\n";
    json += "        \"source_date\": \"" + jsonEscape(source_date) + "\",\n";
    json += "        \"retrieved_date\": \"" + jsonEscape(retrieved_date) + "\",\n";
    json += "        \"data\": {\n";
    for (size_t i = 0; i < fields.size(); ++i) {
        json += "          " + fields[i];
        if (i + 1 < fields.size()) {
            json += ",";
        }
        json += "\n";
    }
    json += "        }\n";
    json += "      }";
    return json;
}
