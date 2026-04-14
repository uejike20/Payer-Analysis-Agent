#ifndef DENIALLETTER_H
#define DENIALLETTER_H

#include "MySource.h"
#include <string>

class DenialLetter : public MySource {
private:
    string fax_number_old;
    string fax_old_status;
    string fax_number_recommended;
    string portal_url;
    string pa_form;
    string chart_note_window_days;
    string chart_note_policy_update;
    string denial_reason;
    string appeal_fax;
    string appeal_phone;
    int appeal_deadline_days = 0;

    string jsonEscape(const string& text) const;

public:
    void sourceReader() override;
    string toJsonRecord() const;
};

#endif
