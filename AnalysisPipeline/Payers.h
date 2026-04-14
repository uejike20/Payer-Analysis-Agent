#ifndef PAYERS_H
#define PAYERS_H

#include "DenialLetter.h"
#include "PhoneTranscript.h"
#include "ProviderManual.h"
#include "WebPage.h"
#include <string>

class Payers {
private:
    string payer_key;
    string payer_display_name;
    ProviderManual provider_manual;
    PhoneTranscript phone_transcript;
    WebPage web_page;
    DenialLetter denial_letter;

public:
    Payers(string payer_key, string payer_display_name);

    void setPayerKey(string payer_key) { 
        this->payer_key = payer_key; 
    }
    void setPayerDisplayName(string payer_display_name) {
        this->payer_display_name = payer_display_name;
    }
    string getPayerKey() { 
        return payer_key; 
    }
    string getPayerDisplayName() { 
        return payer_display_name; 
    }

    // Reads payer_sources/<payer_key>/provider_manual.txt into provider_manual. 
    void readProviderManual() { 
        provider_manual.sourceReader(); 
    }
    void readPhoneTranscript() {
        phone_transcript.sourceReader();
    }
    void readWebPage() {
        web_page.sourceReader();
    }
    void readDenialLetter() {
        denial_letter.sourceReader();
    }
    void readAllSources() {
        readProviderManual();
        readPhoneTranscript();
        readWebPage();
        readDenialLetter();
    }

    string toJsonEntry() const;
};

#endif
