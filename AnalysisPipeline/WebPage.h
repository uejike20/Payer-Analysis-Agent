#ifndef WEBPAGE_H
#define WEBPAGE_H

#include "MySource.h"
#include <string>
#include <vector>

class WebPage : public MySource {
private:
    vector<string> submission_methods;
    string fax_number_specialty;
    string fax_number_general;
    string portal_url;
    string phone_status_only;
    string phone_urgent;
    string pa_form;
    string chart_note_window_days;
    string chart_note_policy_update;
    string turnaround_portal_days;
    string turnaround_fax_days;
    string turnaround_standard_days;
    int turnaround_urgent_hours = 0;
    vector<SourceDrugInfo> drugs;

    /** Escape text for safe JSON string emission. */
    string jsonEscape(const string& text) const;
    /** Build JSON fragment for parsed drug requirements. */
    string buildDrugJson() const;

public:
    /** Read and parse web_page.txt for the configured payer key. */
    void sourceReader() override;
    /** Serialize parsed web-page data as a source JSON record. */
    string toJsonRecord() const;
};

#endif
