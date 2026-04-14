#include "Payers.h"

#include <utility>

using namespace std;

Payers::Payers(string payer_key, string payer_display_name)
    : payer_key(std::move(payer_key)), payer_display_name(std::move(payer_display_name)) {
    provider_manual.setPayerKey(this->payer_key);
    phone_transcript.setPayerKey(this->payer_key);
    web_page.setPayerKey(this->payer_key);
    denial_letter.setPayerKey(this->payer_key);
}

string Payers::toJsonEntry() const {
    string json;

    json += "  \"" + payer_key + "\": {\n";
    json += "    \"payer\": \"" + payer_display_name + "\",\n";
    json += "    \"sources\": [\n";
    json += provider_manual.toJsonRecord() + ",\n";
    json += phone_transcript.toJsonRecord() + ",\n";
    json += web_page.toJsonRecord() + ",\n";
    json += denial_letter.toJsonRecord() + "\n";
    json += "    ]\n";
    json += "  }";

    return json;
}
