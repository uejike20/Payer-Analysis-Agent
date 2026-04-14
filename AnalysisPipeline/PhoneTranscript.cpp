#include "PhoneTranscript.h"

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
    if (payer_key == "aetna") {
        return "AET";
    }
    if (payer_key == "unitedhealthcare") {
        return "UHC";
    }
    if (payer_key == "cigna") {
        return "CIG";
    }
    if (payer_key == "blue_cross_blue_shield") {
        return "BCBS";
    }
    if (payer_key == "humana") {
        return "HUM";
    }
    return "SRC";
}

/** Extract first phone number token from a line. */
string firstPhone(const string& text) {
    regex phonePattern("(\\(\\d{3}\\) \\d{3}-\\d{4})");
    smatch match;
    if (regex_search(text, match, phonePattern)) {
        return match[1].str();
    }
    return "";
}

/** Extract all phone number tokens from a line. */
vector<string> allPhones(const string& text) {
    vector<string> phones;
    regex phonePattern("(\\(\\d{3}\\) \\d{3}-\\d{4})");
    sregex_iterator begin(text.begin(), text.end(), phonePattern);
    sregex_iterator end;
    for (sregex_iterator it = begin; it != end; ++it) {
        phones.push_back((*it)[1].str());
    }
    return phones;
}

/** Extract first form-code-like token from a line. */
string firstForm(const string& text) {
    regex formPattern("([A-Z]{2,}(?:-[A-Z0-9]+)+)");
    smatch match;
    if (regex_search(text, match, formPattern)) {
        return match[1].str();
    }
    return "";
}

/** Return first integer in text (0 if absent). */
int firstNumber(const string& text) {
    regex numPattern("(\\d+)");
    smatch match;
    if (regex_search(text, match, numPattern)) {
        return stoi(match[1].str());
    }
    return 0;
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
string PhoneTranscript::jsonEscape(const string& text) const {
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
string PhoneTranscript::buildDrugJson() const {
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

/** Read and parse phone transcript source for current payer. */
void PhoneTranscript::sourceReader() {
    submission_methods.clear();
    portal_url.clear();
    portal_note.clear();
    fax_number.clear();
    fax_number_old.clear();
    fax_old_status.clear();
    fax_number_general.clear();
    fax_note.clear();
    phone_urgent.clear();
    pa_form.clear();
    pa_form_old.clear();
    pa_form_note.clear();
    chart_note_window_days.clear();
    turnaround_standard_days = 0;
    turnaround_fax_days.clear();
    turnaround_urgent_hours = 0;
    drugs.clear();
    source_name.clear();
    source_date.clear();
    retrieved_date.clear();
    source_type = "phone_transcript";
    source_id = payerPrefix(payer_key) + "-SRC-002";

    filename = "payer_sources/" + payer_key + "/phone_transcript.txt";
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

    for (size_t i = 0; i < lines.size(); ++i) {
        string current = trim(lines[i]);
        string lowered = toLowerCopy(current);

        if (current.rfind("Source:", 0) == 0) {
            source_name = trim(current.substr(7));
        }
        if (current.rfind("Date:", 0) == 0) {
            source_date = trim(current.substr(5));
            retrieved_date = source_date;
        }
        if (current.rfind("Called:", 0) == 0 && phone_urgent.empty()) {
            phone_urgent = trim(current.substr(7));
        }
        if (lowered.find("portal") != string::npos || lowered.find("availity") != string::npos ||
            lowered.find("uhcprovider.com") != string::npos) {
            if (find(submission_methods.begin(), submission_methods.end(), "portal") == submission_methods.end()) {
                submission_methods.push_back("portal");
            }
            if (portal_url.empty()) {
                regex domainPattern("(www\\.[A-Za-z0-9./-]+|[a-z]+provider\\.com|Availity)");
                smatch domainMatch;
                if (regex_search(current, domainMatch, domainPattern)) {
                    portal_url = domainMatch[1].str();
                    if (portal_url == "Availity") {
                        portal_url = "www.availity.com";
                    }
                }
            }
        }
        if (lowered.find("fax") != string::npos) {
            if (find(submission_methods.begin(), submission_methods.end(), "fax") == submission_methods.end()) {
                submission_methods.push_back("fax");
            }
        }
        if (lowered.find("urgent") != string::npos && lowered.find("24 hours") != string::npos) {
            turnaround_urgent_hours = 24;
        } else if (lowered.find("urgent") != string::npos && lowered.find("48 hours") != string::npos) {
            turnaround_urgent_hours = 48;
        }
        if (lowered.find("standard is") != string::npos && lowered.find("business days") != string::npos) {
            turnaround_standard_days = firstNumber(current);
        }
        if (lowered.find("fax submissions are taking") != string::npos ||
            lowered.find("real turnaround is more like") != string::npos) {
            regex rangePattern("(\\d+-\\d+)");
            smatch rangeMatch;
            if (regex_search(current, rangeMatch, rangePattern)) {
                turnaround_fax_days = rangeMatch[1].str();
            }
        }
        if (lowered.find("chart notes within") != string::npos || lowered.find("chart notes are") != string::npos) {
            int days = firstNumber(current);
            if (days > 0) {
                chart_note_window_days = to_string(days);
            }
        }
        if (lowered.find("old form") != string::npos || lowered.find("phased out") != string::npos) {
            string foundForm = firstForm(current);
            if (!foundForm.empty()) {
                pa_form_old = foundForm;
                pa_form_note = current;
            }
        }
        if (lowered.find("new form") != string::npos || lowered.find("use form") != string::npos ||
            lowered.find("released a new form") != string::npos) {
            string foundForm = firstForm(current);
            if (!foundForm.empty()) {
                pa_form = foundForm;
            }
        }
        if (lowered.find("fax number is") != string::npos || lowered.find("new fax line") != string::npos ||
            lowered.find("fax number is (") != string::npos || lowered.find("same as it's always been") != string::npos) {
            string foundPhone = firstPhone(current);
            if (!foundPhone.empty()) {
                fax_number = foundPhone;
            }
        }

        vector<string> phones = allPhones(current);
        if (phones.size() >= 2 && lowered.find("still works") != string::npos) {
            fax_number = phones[0];
            fax_number_general = phones[1];
            fax_note = current;
        }
        if (phones.size() >= 2 && lowered.find("decommissioned") != string::npos) {
            fax_number_old = phones[0];
            fax_number = phones[1];
            fax_old_status = current;
            portal_note = "Preferred - faster than fax";
        }

        if (lowered.find("preferred method") != string::npos || lowered.find("preferred method now") != string::npos ||
            lowered.find("i'd fax it if i were you") != string::npos || lowered.find("portal now") != string::npos) {
            portal_note = current;
        }

        if (lowered.find("remicade") != string::npos || lowered.find("rituxan") != string::npos) {
            if (lowered.find("need") != string::npos || lowered.find("deny") != string::npos || lowered.find("reason") != string::npos) {
                SourceDrugInfo drug;
                if (lowered.find("rituxan") != string::npos) {
                    drug.setDrugName("Rituxan");
                } else {
                    drug.setDrugName("Remicade");
                }
                if (lowered.find("step therapy") != string::npos || lowered.find("biosimilar") != string::npos) {
                    drug.setStepTherapyRequired(true);
                }
                if (lowered.find("biosimilar") != string::npos) {
                    drug.setBiosimilarPreferred(true);
                }
                drug.setNotes(current);
                drugs.push_back(drug);
            }
        }
    }

    if (source_date.empty()) {
        source_date = "2026-04-13";
        retrieved_date = source_date;
    }
}

/** Serialize parsed phone-transcript content as one source JSON record. */
string PhoneTranscript::toJsonRecord() const {
    vector<string> fields;

    if (!submission_methods.empty()) {
        vector<string> methods;
        for (size_t i = 0; i < submission_methods.size(); ++i) {
            methods.push_back("\"" + jsonEscape(submission_methods[i]) + "\"");
        }
        fields.push_back("\"submission_methods\": [" + joinFields(methods, ", ") + "]");
    }
    if (!fax_number.empty()) {
        fields.push_back("\"fax_number\": \"" + jsonEscape(fax_number) + "\"");
    }
    if (!fax_number_old.empty()) {
        fields.push_back("\"fax_number_old\": \"" + jsonEscape(fax_number_old) + "\"");
    }
    if (!fax_old_status.empty()) {
        fields.push_back("\"fax_old_status\": \"" + jsonEscape(fax_old_status) + "\"");
    }
    if (!fax_number_general.empty()) {
        fields.push_back("\"fax_number_general\": \"" + jsonEscape(fax_number_general) + "\"");
    }
    if (!fax_note.empty()) {
        fields.push_back("\"fax_note\": \"" + jsonEscape(fax_note) + "\"");
    }
    if (!portal_url.empty()) {
        fields.push_back("\"portal_url\": \"" + jsonEscape(portal_url) + "\"");
    }
    if (!portal_note.empty()) {
        fields.push_back("\"portal_note\": \"" + jsonEscape(portal_note) + "\"");
    }
    if (!pa_form.empty()) {
        fields.push_back("\"pa_form\": \"" + jsonEscape(pa_form) + "\"");
    }
    if (!pa_form_old.empty()) {
        fields.push_back("\"pa_form_old\": \"" + jsonEscape(pa_form_old) + "\"");
    }
    if (!pa_form_note.empty()) {
        fields.push_back("\"pa_form_note\": \"" + jsonEscape(pa_form_note) + "\"");
    }
    if (!chart_note_window_days.empty()) {
        fields.push_back("\"chart_note_window_days\": " + chart_note_window_days);
    }
    if (turnaround_standard_days > 0) {
        fields.push_back("\"turnaround_standard_days\": " + to_string(turnaround_standard_days));
    }
    if (!turnaround_fax_days.empty()) {
        fields.push_back("\"turnaround_fax_days\": \"" + jsonEscape(turnaround_fax_days) + "\"");
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
