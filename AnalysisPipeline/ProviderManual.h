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

    /** Escape text for safe JSON string emission. */
    string jsonEscape(const string& text) const;
    /** Return first line match after marker from a text block. */
    string extractFirstMatch(const string& text, const string& marker) const;
    /** Parse metadata fields like source name from raw lines. */
    void parseMetadata(const vector<string>& lines);
    /** Parse submission channels, portal URLs, and contact numbers. */
    void parseSubmissionMethods(const vector<string>& lines);
    /** Parse form/documentation requirements from source text. */
    void parseDocumentation(const vector<string>& lines);
    /** Parse drug-specific requirements and notes. */
    void parseDrugRequirements(const vector<string>& lines);
    /** Parse turnaround times for standard/urgent cases. */
    void parseTurnaround(const vector<string>& lines);
    /** Parse source effective/last-updated dates. */
    void parseDates(const vector<string>& lines);
    /** Parse payer coverage-state list when present. */
    void parseCoverageStates(const vector<string>& lines);
    /** Extract first integer that appears after marker phrase. */
    int extractNumberAfterPhrase(const string& text, const string& marker) const;
    /** Build JSON fragment for parsed drug requirements. */
    string buildDrugJson() const;

public:
    /** Read and parse provider_manual.txt for the configured payer key. */
    void sourceReader() override;
    /** Serialize parsed provider-manual data as a source JSON record. */
    string toJsonRecord() const;
};

#endif
