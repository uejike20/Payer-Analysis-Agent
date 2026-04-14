#ifndef PROVIDERMANUAL_H
#define PROVIDERMANUAL_H
#include "MySource.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

class ProviderManual : public MySource {
private:
    vector<string> submission_methods;
    string portal_url;
    string phone_status_only;
    string phone_urgent;
    string phone_hours;
    string pa_form;
    string chart_note_window_days;
    string lab_window_days;
    int turnaround_standard_days = 0;
    int turnaround_urgent_hours = 0;
    vector<SourceDrugInfo> drugs;
    vector<string> coverage_states;
    string note;
    string source_title;

    string jsonEscape(const string& text) const;
    string extractFirstMatch(const string& text, const string& marker) const;
    void parseMetadata(const vector<string>& lines);
    void parseSubmissionMethods(const vector<string>& lines);
    void parseDocumentation(const vector<string>& lines);
    void parseDrugRequirements(const vector<string>& lines);
    void parseTurnaround(const vector<string>& lines);
    void parseDates(const vector<string>& lines);
    void parseCoverageStates(const vector<string>& lines);
    int extractNumberAfterPhrase(const string& text, const string& marker) const;
    string buildDrugJson() const;

public:
    void sourceReader() override;
    string toJsonRecord() const;
};

#endif
