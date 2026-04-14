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
    /** Construct payer container and bind source readers to payer key. */
    Payers(string payer_key, string payer_display_name);

    /** Update payer key (folder identifier). */
    void setPayerKey(string payer_key) { 
        this->payer_key = payer_key; 
    }
    /** Update payer display name used in output JSON. */
    void setPayerDisplayName(string payer_display_name) {
        this->payer_display_name = payer_display_name;
    }
    /** Return payer key (folder identifier). */
    string getPayerKey() { 
        return payer_key; 
    }
    /** Return human-readable payer name. */
    string getPayerDisplayName() { 
        return payer_display_name; 
    }

    /** Parse provider manual source for this payer. */
    void readProviderManual() { 
        provider_manual.sourceReader(); 
    }
    /** Parse phone transcript source for this payer. */
    void readPhoneTranscript() {
        phone_transcript.sourceReader();
    }
    /** Parse website source for this payer. */
    void readWebPage() {
        web_page.sourceReader();
    }
    /** Parse denial letter source for this payer. */
    void readDenialLetter() {
        denial_letter.sourceReader();
    }
    /** Parse all known source types for this payer. */
    void readAllSources() {
        readProviderManual();
        readPhoneTranscript();
        readWebPage();
        readDenialLetter();
    }

    /** Build payer-level JSON entry with all parsed source records. */
    string toJsonEntry() const;
};

#endif
