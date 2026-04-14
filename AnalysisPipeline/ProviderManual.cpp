#include "ProviderManual.h"

#include <algorithm>
#include <cctype>
#include <regex>

using namespace std;

namespace {
/** Trim leading/trailing whitespace. */
string trim(const string& text) {
    size_t start = 0;
    while (start < text.size() && isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }

    size_t end = text.size();
    while (end > start && isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }

    return text.substr(start, end - start);
}

/** Return lowercase copy for case-insensitive matching. */
string toLowerCopy(const string& text) {
    string lowered = text;
    for (size_t i = 0; i < lowered.size(); ++i) {
        lowered[i] = static_cast<char>(tolower(static_cast<unsigned char>(lowered[i])));
    }
    return lowered;
}

/** Build uppercase snake-style key from free text. */
string toUpperKey(const string& text) {
    string key;
    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        if (isalnum(static_cast<unsigned char>(ch))) {
            key += static_cast<char>(toupper(static_cast<unsigned char>(ch)));
        } else if (ch == ' ' || ch == '_' || ch == '-') {
            key += '_';
        }
    }
    return key;
}

/** Extract first phone-number-like token from text. */
string extractPhoneNumber(const string& text) {
    size_t openParen = text.find('(');
    if (openParen == string::npos) {
        return "";
    }

    size_t closeParen = text.find(')', openParen);
    if (closeParen == string::npos) {
        return "";
    }

    size_t end = closeParen + 1;
    while (end < text.size()) {
        char ch = text[end];
        if (isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == ' ') {
            ++end;
        } else {
            break;
        }
    }

    return trim(text.substr(openParen, end - openParen));
}

/** Return first integer found in text (0 when absent). */
int findFirstNumber(const string& text) {
    for (size_t i = 0; i < text.size(); ++i) {
        if (isdigit(static_cast<unsigned char>(text[i]))) {
            size_t end = i;
            while (end < text.size() && isdigit(static_cast<unsigned char>(text[end]))) {
                ++end;
            }
            return stoi(text.substr(i, end - i));
        }
    }
    return 0;
}

/** Remove bullets/numbering prefixes from a line. */
string stripBulletPrefix(const string& text) {
    string cleaned = trim(text);
    if (!cleaned.empty() && (cleaned[0] == '-' || cleaned[0] == '*' || isdigit(static_cast<unsigned char>(cleaned[0])))) {
        size_t start = 1;
        while (start < cleaned.size() &&
               (cleaned[start] == '.' || cleaned[start] == ')' || cleaned[start] == ':' || isspace(static_cast<unsigned char>(cleaned[start])))) {
            ++start;
        }
        cleaned = trim(cleaned.substr(start));
    }
    return cleaned;
}

/** Heuristic detector for section boundary headings. */
bool isSectionHeading(const string& text) {
    string trimmed = trim(text);
    string lowered = toLowerCopy(trimmed);

    if (trimmed.empty()) {
        return false;
    }

    if (lowered.find("step therapy requirements") != string::npos ||
        lowered.find("turnaround times") != string::npos ||
        lowered.find("important notes") != string::npos ||
        lowered.find("appeals") != string::npos ||
        lowered.find("section 4") != string::npos ||
        lowered.find("section 5") != string::npos ||
        lowered.find("12.4") != string::npos ||
        lowered.find("12.5") != string::npos ||
        lowered.find("8.4") != string::npos ||
        lowered == "turnaround:" || lowered == "appeals:") {
        return true;
    }

    return false;
}
}

/** Escape text for safe JSON output. */
string ProviderManual::jsonEscape(const string& text) const {
    string escaped;

    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        if (ch == '\\') {
            escaped += "\\\\";
        } else if (ch == '"') {
            escaped += "\\\"";
        } else if (ch == '\n') {
            escaped += "\\n";
        } else if (ch == '\t') {
            escaped += "\\t";
        } else {
            escaped += ch;
        }
    }

    return escaped;
}

/** Extract first line fragment after marker. */
string ProviderManual::extractFirstMatch(const string& text, const string& marker) const {
    size_t start = text.find(marker);
    if (start == string::npos) {
        return "";
    }

    start += marker.size();
    size_t end = text.find('\n', start);
    if (end == string::npos) {
        end = text.size();
    }

    return trim(text.substr(start, end - start));
}

/** Extract first number appearing after marker phrase. */
int ProviderManual::extractNumberAfterPhrase(const string& text, const string& marker) const {
    size_t start = text.find(marker);
    if (start == string::npos) {
        return 0;
    }

    return findFirstNumber(text.substr(start + marker.size()));
}

/** Parse source metadata fields from raw lines. */
void ProviderManual::parseMetadata(const vector<string>& lines) {
    for (size_t i = 0; i < lines.size(); ++i) {
        string line = trim(lines[i]);
        if (line.rfind("Source:", 0) == 0) {
            source_name = trim(line.substr(7));
        }
    }
}

/** Parse submission channels, portal URL, and contact numbers. */
void ProviderManual::parseSubmissionMethods(const vector<string>& lines) {
    bool hasPortal = false;
    bool hasFax = false;
    bool hasPhoneUrgentOnly = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        string line = trim(lines[i]);
        string lowered = toLowerCopy(line);

        if (portal_url.empty()) {
            size_t wwwPos = lowered.find("www.");
            if (wwwPos != string::npos) {
                size_t end = wwwPos;
                while (end < line.size() && !isspace(static_cast<unsigned char>(line[end])) &&
                       line[end] != ')' && line[end] != ',' && line[end] != '.') {
                    ++end;
                }
                if (end == wwwPos && wwwPos < line.size()) {
                    end = line.size();
                }

                size_t actualEnd = wwwPos;
                while (actualEnd < line.size() &&
                       !isspace(static_cast<unsigned char>(line[actualEnd])) &&
                       line[actualEnd] != ')' && line[actualEnd] != ',') {
                    ++actualEnd;
                }
                portal_url = trim(line.substr(wwwPos, actualEnd - wwwPos));
                while (!portal_url.empty() && portal_url.back() == '.') {
                    portal_url.pop_back();
                }
            }
        }

        if (lowered.find("portal") != string::npos || lowered.find("online") != string::npos ||
            lowered.find("availity") != string::npos) {
            hasPortal = true;
        }

        if (lowered.find("fax") != string::npos) {
            hasFax = true;
            if (fax_number.empty()) {
                fax_number = extractPhoneNumber(line);
            }
        }

        if (lowered.find("urgent") != string::npos && lowered.find("phone") != string::npos) {
            hasPhoneUrgentOnly = true;
            if (phone_urgent.empty()) {
                phone_urgent = extractPhoneNumber(line);
            }
        }

        if (lowered.find("status can be directed") != string::npos ||
            lowered.find("status checks") != string::npos ||
            lowered.find("status check") != string::npos) {
            phone_status_only = extractPhoneNumber(line);
        }

        if (lowered.find("m-f") != string::npos || lowered.find("sat") != string::npos) {
            phone_hours = stripBulletPrefix(line);
        }
    }

    if (hasPortal) {
        submission_methods.push_back("portal");
    }
    if (hasFax) {
        submission_methods.push_back("fax");
    }
    if (hasPhoneUrgentOnly) {
        submission_methods.push_back("phone_urgent_only");
    }
}

/** Parse documentation window and form requirements. */
void ProviderManual::parseDocumentation(const vector<string>& lines) {
    for (size_t i = 0; i < lines.size(); ++i) {
        string line = trim(lines[i]);
        string lowered = toLowerCopy(line);

        if (pa_form.empty()) {
            regex formPattern("([A-Z]{2,}(?:-[A-Z0-9]+)+)");
            smatch formMatch;
            if (regex_search(line, formMatch, formPattern)) {
                pa_form = formMatch[1].str();
            }
        }

        if (chart_note_window_days.empty() &&
            (lowered.find("chart notes") != string::npos || lowered.find("visit notes") != string::npos ||
             lowered.find("physician notes") != string::npos || lowered.find("office visit notes") != string::npos)) {
            int days = findFirstNumber(line);
            if (days > 0) {
                chart_note_window_days = to_string(days);
            }
        }

        if (lab_window_days.empty() &&
            (lowered.find("lab") != string::npos || lowered.find("diagnostic") != string::npos) &&
            lowered.find("within") != string::npos) {
            int days = findFirstNumber(line);
            if (days > 0) {
                lab_window_days = to_string(days);
            }
        }
    }
}

/** Parse per-drug policy requirements from provider manual text. */
void ProviderManual::parseDrugRequirements(const vector<string>& lines) {
    bool inDrugSection = false;
    regex drugHeaderPattern("^([A-Z0-9][A-Z0-9\\- ]*(?:\\([A-Za-z0-9\\- ]+\\))?):\\s*(.*)$");

    for (size_t i = 0; i < lines.size(); ++i) {
        string line = trim(lines[i]);
        string lowered = toLowerCopy(line);

        if (lowered.find("drug-specific") != string::npos ||
            lowered.find("drug-specific requirements") != string::npos ||
            lowered.find("drug-specific policies") != string::npos ||
            lowered.find("drug formulary notes") != string::npos ||
            lowered.find("drug-specific notes") != string::npos) {
            inDrugSection = true;
            continue;
        }

        if (inDrugSection && isSectionHeading(line)) {
            inDrugSection = false;
        }

        if (!inDrugSection || line.empty()) {
            continue;
        }

        smatch drugMatch;
        if (!regex_match(line, drugMatch, drugHeaderPattern)) {
            continue;
        }

        string candidateDrug = trim(drugMatch[1].str());
        string initialNotes = trim(drugMatch[2].str());
        if (candidateDrug == "A" || candidateDrug == "B" || candidateDrug == "C") {
            continue;
        }

        SourceDrugInfo info;
        size_t paren = candidateDrug.find('(');
        if (paren != string::npos) {
            candidateDrug = trim(candidateDrug.substr(0, paren));
        }
        info.setDrugName(candidateDrug);

        string combinedNotes = initialNotes;
        size_t j = i + 1;
        while (j < lines.size()) {
            string nextLine = trim(lines[j]);
            string nextLowered = toLowerCopy(nextLine);
            if (nextLine.empty()) {
                break;
            }
            if (regex_match(nextLine, drugHeaderPattern)) {
                break;
            }
            if (isSectionHeading(nextLine)) {
                break;
            }

            string cleaned = stripBulletPrefix(nextLine);
            if (!cleaned.empty()) {
                if (!combinedNotes.empty()) {
                    combinedNotes += " ";
                }
                combinedNotes += cleaned;
            }
            ++j;
        }

        string loweredNotes = toLowerCopy(combinedNotes);
        if (loweredNotes.find("step therapy required") != string::npos ||
            loweredNotes.find("step therapy") != string::npos ||
            loweredNotes.find("failure of") != string::npos ||
            loweredNotes.find("trial and failure") != string::npos) {
            info.setStepTherapyRequired(true);
        }

        if (loweredNotes.find("biosimilar preferred") != string::npos ||
            loweredNotes.find("biosimilar attestation") != string::npos) {
            info.setBiosimilarPreferred(true);
        }

        int authMonths = extractNumberAfterPhrase(loweredNotes, "auth period:");
        if (authMonths == 0) {
            authMonths = extractNumberAfterPhrase(loweredNotes, "auth:");
        }
        if (authMonths == 0) {
            authMonths = extractNumberAfterPhrase(loweredNotes, "initial auth:");
        }
        if (authMonths > 0) {
            info.setAuthPeriodMonths(authMonths);
        }

        info.setNotes(combinedNotes);
        drugs.push_back(info);
        i = j > 0 ? j - 1 : i;
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        string line = trim(lines[i]);
        string lowered = toLowerCopy(line);

        if (lowered.find("for remicade and entyvio") != string::npos) {
            for (size_t j = 0; j < drugs.size(); ++j) {
                string drugLower = toLowerCopy(drugs[j].getDrugName());
                if (drugLower == "remicade" || drugLower == "entyvio") {
                    drugs[j].setStepTherapyRequired(true);
                }
            }
        }
    }
}

/** Parse standard and urgent turnaround windows. */
void ProviderManual::parseTurnaround(const vector<string>& lines) {
    regex standardPattern("(\\d+)\\s+business days?\\s+standard|(standard[^0-9]*)(\\d+)\\s+business days?");
    regex urgentPattern("(\\d+)\\s+hours?\\s+urgent|(urgent[^0-9]*)(\\d+)\\s+hours?");

    for (size_t i = 0; i < lines.size(); ++i) {
        string line = trim(lines[i]);
        string lowered = toLowerCopy(line);
        smatch match;

        if (turnaround_standard_days == 0 && regex_search(lowered, match, standardPattern)) {
            if (match[1].matched) {
                turnaround_standard_days = stoi(match[1].str());
            } else if (match[3].matched) {
                turnaround_standard_days = stoi(match[3].str());
            }
        }

        if (turnaround_urgent_hours == 0 && regex_search(lowered, match, urgentPattern)) {
            if (match[1].matched) {
                turnaround_urgent_hours = stoi(match[1].str());
            } else if (match[3].matched) {
                turnaround_urgent_hours = stoi(match[3].str());
            }
        }
    }
}

/** Parse effective/last-updated source dates. */
void ProviderManual::parseDates(const vector<string>& lines) {
    for (size_t i = 0; i < lines.size(); ++i) {
        string line = trim(lines[i]);
        if (source_date.empty() &&
            (line.rfind("Last updated:", 0) == 0 || line.rfind("Last revised:", 0) == 0 ||
             line.rfind("Effective:", 0) == 0 || line.rfind("EFFECTIVE DATE:", 0) == 0)) {
            size_t colon = line.find(':');
            if (colon != string::npos) {
                source_date = trim(line.substr(colon + 1));
            }
        }
    }
}

/** Parse coverage states list (when present in source). */
void ProviderManual::parseCoverageStates(const vector<string>& lines) {
    for (size_t i = 0; i < lines.size(); ++i) {
        string line = trim(lines[i]);
        string lowered = toLowerCopy(line);
        if (lowered.find("covers anthem bcbs commercial plans in") != string::npos) {
            size_t inPos = lowered.find("in ");
            if (inPos != string::npos) {
                string statesText = line.substr(inPos + 3);
                size_t stopPos = statesText.find(". For other");
                if (stopPos != string::npos) {
                    statesText = statesText.substr(0, stopPos);
                }
                if (!statesText.empty() && statesText.back() == '.') {
                    statesText.pop_back();
                }

                string state;
                stringstream stateStream(statesText);
                while (getline(stateStream, state, ',')) {
                    string cleaned = trim(state);
                    if (cleaned.size() > 4 && cleaned.substr(0, 4) == "and ") {
                        cleaned = trim(cleaned.substr(4));
                    }
                    if (!cleaned.empty()) {
                        coverage_states.push_back(cleaned);
                    }
                }
            }
        }
    }
}

/** Build JSON object fragment for parsed drug requirements. */
string ProviderManual::buildDrugJson() const {
    string json = "        \"drugs\": {\n";

    for (size_t i = 0; i < drugs.size(); ++i) {
        json += "          \"" + jsonEscape(drugs[i].getDrugName()) + "\": {\n";

        bool needsComma = false;
        if (drugs[i].getStepTherapyRequired()) {
            json += "            \"step_therapy_required\": true";
            needsComma = true;
        }
        if (drugs[i].getBiosimilarPreferred()) {
            if (needsComma) {
                json += ",\n";
            } else {
                needsComma = true;
            }
            json += "            \"biosimilar_preferred\": true";
        }
        if (drugs[i].getAuthPeriodMonths() > 0) {
            if (needsComma) {
                json += ",\n";
            } else {
                needsComma = true;
            }
            json += "            \"auth_period_months\": " + to_string(drugs[i].getAuthPeriodMonths());
        }
        if (!drugs[i].getNotes().empty()) {
            if (needsComma) {
                json += ",\n";
            }
            json += "            \"notes\": \"" + jsonEscape(drugs[i].getNotes()) + "\"\n";
        } else {
            json += '\n';
        }

        json += "          }";
        if (i < drugs.size() - 1) {
            json += ",";
        }
        json += "\n";
    }

    json += "        }";
    return json;
}

/** Read and parse provider manual source for current payer. */
void ProviderManual::sourceReader() {
    submission_methods.clear();
    portal_url.clear();
    phone_status_only.clear();
    phone_urgent.clear();
    phone_hours.clear();
    pa_form.clear();
    chart_note_window_days.clear();
    lab_window_days.clear();
    turnaround_standard_days = 0;
    turnaround_urgent_hours = 0;
    drugs.clear();
    coverage_states.clear();
    note.clear();
    source_name.clear();
    source_date.clear();
    retrieved_date.clear();

    if (payer_key.empty()) {
        cerr << "ProviderManual::sourceReader: setPayerKey() was not called.\n";
        return;
    }

    string inFilename = "payer_sources/" + payer_key + "/provider_manual.txt";
    filename = inFilename;
    source_type = "provider_manual";

    ifstream inFile(inFilename);
    if (!inFile.is_open()) {
        cerr << "Error opening file: " << inFilename << endl;
        return;
    }
    cout << "File opened successfully: " << inFilename << endl;

    vector<string> lines;
    string line;
    while (getline(inFile, line)) {
        lines.push_back(line);
    }

    parseMetadata(lines);
    parseSubmissionMethods(lines);
    parseDocumentation(lines);
    parseDrugRequirements(lines);
    parseTurnaround(lines);
    parseDates(lines);
    parseCoverageStates(lines);

    if (retrieved_date.empty()) {
        retrieved_date = "2026-04-13";
    }
    if (source_name.empty()) {
        source_name = payer_key + " provider manual";
    }

    string upperKey = toUpperKey(payer_key);
    if (upperKey == "BLUE_CROSS_BLUE_SHIELD") {
        upperKey = "BCBS";
    } else if (upperKey == "UNITEDHEALTHCARE") {
        upperKey = "UHC";
    } else if (upperKey == "HUMANA") {
        upperKey = "HUM";
    } else if (upperKey == "CIGNA") {
        upperKey = "CIG";
    } else if (upperKey == "AETNA") {
        upperKey = "AET";
    }
    source_id = upperKey + "-SRC-001";
}

/** Serialize parsed provider-manual content as one source JSON record. */
string ProviderManual::toJsonRecord() const {
    string json;

    json += "      {\n";
    json += "        \"source_id\": \"" + jsonEscape(source_id) + "\",\n";
    json += "        \"source_type\": \"" + jsonEscape(source_type) + "\",\n";
    json += "        \"source_name\": \"" + jsonEscape(source_name) + "\",\n";
    json += "        \"source_date\": \"" + jsonEscape(source_date) + "\",\n";
    json += "        \"retrieved_date\": \"" + jsonEscape(retrieved_date) + "\",\n";
    json += "        \"data\": {\n";

    json += "          \"submission_methods\": [";
    for (size_t i = 0; i < submission_methods.size(); ++i) {
        json += "\"" + jsonEscape(submission_methods[i]) + "\"";
        if (i < submission_methods.size() - 1) {
            json += ", ";
        }
    }
    json += "]";

    if (!fax_number.empty()) {
        json += ",\n          \"fax_number\": \"" + jsonEscape(fax_number) + "\"";
    }
    if (!portal_url.empty()) {
        json += ",\n          \"portal_url\": \"" + jsonEscape(portal_url) + "\"";
    }
    if (!phone_status_only.empty()) {
        json += ",\n          \"phone_status_only\": \"" + jsonEscape(phone_status_only) + "\"";
    }
    if (!phone_urgent.empty()) {
        json += ",\n          \"phone_urgent\": \"" + jsonEscape(phone_urgent) + "\"";
    }
    if (!phone_hours.empty()) {
        json += ",\n          \"phone_hours\": \"" + jsonEscape(phone_hours) + "\"";
    }
    if (!pa_form.empty()) {
        json += ",\n          \"pa_form\": \"" + jsonEscape(pa_form) + "\"";
    }
    if (!chart_note_window_days.empty()) {
        json += ",\n          \"chart_note_window_days\": " + chart_note_window_days;
    }
    if (!lab_window_days.empty()) {
        json += ",\n          \"lab_window_days\": " + lab_window_days;
    }
    if (turnaround_standard_days > 0) {
        json += ",\n          \"turnaround_standard_days\": " + to_string(turnaround_standard_days);
    }
    if (turnaround_urgent_hours > 0) {
        json += ",\n          \"turnaround_urgent_hours\": " + to_string(turnaround_urgent_hours);
    }
    if (!coverage_states.empty()) {
        json += ",\n          \"coverage_states\": [";
        for (size_t i = 0; i < coverage_states.size(); ++i) {
            json += "\"" + jsonEscape(coverage_states[i]) + "\"";
            if (i < coverage_states.size() - 1) {
                json += ", ";
            }
        }
        json += "]";
    }
    if (!drugs.empty()) {
        json += ",\n" + buildDrugJson();
    }

    json += "\n        }\n";
    json += "      }";

    return json;
}
