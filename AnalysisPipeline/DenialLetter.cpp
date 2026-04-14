#include "DenialLetter.h"

#include <cctype>
#include <fstream>
#include <regex>
#include <vector>

using namespace std;

namespace {
string trim(const string& text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == string::npos) {
        return "";
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

string toLowerCopy(const string& text) {
    string lowered = text;
    for (size_t i = 0; i < lowered.size(); ++i) {
        lowered[i] = static_cast<char>(tolower(static_cast<unsigned char>(lowered[i])));
    }
    return lowered;
}

string payerPrefix(const string& payer_key) {
    if (payer_key == "aetna") return "AET";
    if (payer_key == "unitedhealthcare") return "UHC";
    if (payer_key == "cigna") return "CIG";
    if (payer_key == "blue_cross_blue_shield") return "BCBS";
    if (payer_key == "humana") return "HUM";
    return "SRC";
}

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

string DenialLetter::jsonEscape(const string& text) const {
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

void DenialLetter::sourceReader() {
    fax_number.clear();
    fax_number_old.clear();
    fax_old_status.clear();
    fax_number_recommended.clear();
    portal_url.clear();
    pa_form.clear();
    chart_note_window_days.clear();
    chart_note_policy_update.clear();
    denial_reason.clear();
    appeal_fax.clear();
    appeal_phone.clear();
    appeal_deadline_days = 0;
    source_name.clear();
    source_date.clear();
    retrieved_date.clear();
    source_type = "denial_letter";
    source_id = payerPrefix(payer_key) + "-SRC-004";

    filename = "payer_sources/" + payer_key + "/denial_letter.txt";
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
    smatch match;

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
        if (lowered.find("portal") != string::npos && portal_url.empty()) {
            regex domainPattern("(www\\.[A-Za-z0-9./-]+|Availity|uhcprovider\\.com)");
            if (regex_search(current, match, domainPattern)) {
                portal_url = match[1].str();
                if (portal_url == "Availity") {
                    portal_url = "www.availity.com";
                }
            }
        }
        if (regex_search(current, match, formPattern) && pa_form.empty()) {
            pa_form = match[1].str();
        }
        if (lowered.find("appeal") != string::npos && regex_search(current, match, phonePattern)) {
            if (appeal_fax.empty() && lowered.find("fax") != string::npos) {
                appeal_fax = match[1].str();
            } else if (appeal_phone.empty() && lowered.find("call") != string::npos) {
                appeal_phone = match[1].str();
            }
        }
        if (lowered.find("within 60 days") != string::npos) {
            appeal_deadline_days = 60;
        }
        if (lowered.find("within 90 days") != string::npos) {
            chart_note_window_days = "90";
        }
        if (lowered.find("policy update") != string::npos || lowered.find("effective january") != string::npos ||
            lowered.find("effective february") != string::npos) {
            chart_note_policy_update = current;
        }
        if (lowered.find("no longer active") != string::npos || lowered.find("routed manually") != string::npos) {
            vector<string> phones;
            sregex_iterator begin(current.begin(), current.end(), phonePattern);
            sregex_iterator end;
            for (sregex_iterator it = begin; it != end; ++it) {
                phones.push_back((*it)[1].str());
            }
            if (!phones.empty()) {
                fax_number_old = phones[0];
                fax_old_status = current;
            }
        }
        if ((lowered.find("fax to our new") != string::npos || lowered.find("please use") != string::npos ||
             lowered.find("fax:") != string::npos) && regex_search(current, match, phonePattern)) {
            if (fax_number_recommended.empty()) {
                fax_number_recommended = match[1].str();
            }
        }
        if (lowered.find("reason for denial") != string::npos || lowered == "reason:" || lowered.find("step therapy not met") != string::npos ||
            lowered.find("outdated documentation") != string::npos || lowered.find("form version") != string::npos) {
            if (!denial_reason.empty()) {
                denial_reason += " ";
            }
            denial_reason += current;
        }
    }

    if (retrieved_date.empty()) {
        retrieved_date = source_date;
    }
}

string DenialLetter::toJsonRecord() const {
    vector<string> fields;
    if (!fax_number_old.empty()) {
        fields.push_back("\"fax_number_old\": \"" + jsonEscape(fax_number_old) + "\"");
    }
    if (!fax_old_status.empty()) {
        fields.push_back("\"fax_old_status\": \"" + jsonEscape(fax_old_status) + "\"");
    }
    if (!fax_number_recommended.empty()) {
        fields.push_back("\"fax_number_recommended\": \"" + jsonEscape(fax_number_recommended) + "\"");
    }
    if (!portal_url.empty()) {
        fields.push_back("\"portal_url\": \"" + jsonEscape(portal_url) + "\"");
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
    if (!denial_reason.empty()) {
        fields.push_back("\"denial_reason\": \"" + jsonEscape(denial_reason) + "\"");
    }
    if (!appeal_fax.empty()) {
        fields.push_back("\"appeal_fax\": \"" + jsonEscape(appeal_fax) + "\"");
    }
    if (!appeal_phone.empty()) {
        fields.push_back("\"appeal_phone\": \"" + jsonEscape(appeal_phone) + "\"");
    }
    if (appeal_deadline_days > 0) {
        fields.push_back("\"appeal_deadline_days\": " + to_string(appeal_deadline_days));
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
