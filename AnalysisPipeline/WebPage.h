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

    string jsonEscape(const string& text) const;
    string buildDrugJson() const;

public:
    void sourceReader() override;
    string toJsonRecord() const;
};

#endif
