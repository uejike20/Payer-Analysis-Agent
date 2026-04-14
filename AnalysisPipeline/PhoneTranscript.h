#ifndef PHONETRANSCRIPT_H
#define PHONETRANSCRIPT_H

#include "MySource.h"
#include <string>
#include <vector>

class PhoneTranscript : public MySource {
private:
    vector<string> submission_methods;
    string portal_url;
    string portal_note;
    string fax_number_old;
    string fax_old_status;
    string fax_number_general;
    string fax_note;
    string phone_urgent;
    string pa_form;
    string pa_form_old;
    string pa_form_note;
    string chart_note_window_days;
    int turnaround_standard_days = 0;
    string turnaround_fax_days;
    int turnaround_urgent_hours = 0;
    vector<SourceDrugInfo> drugs;

    /** Escape text for safe JSON string emission. */
    string jsonEscape(const string& text) const;
    /** Build JSON fragment for parsed drug requirements. */
    string buildDrugJson() const;

public:
    /** Read and parse phone_transcript.txt for the configured payer key. */
    void sourceReader() override;
    /** Serialize parsed phone-transcript data as a source JSON record. */
    string toJsonRecord() const;
};

#endif
