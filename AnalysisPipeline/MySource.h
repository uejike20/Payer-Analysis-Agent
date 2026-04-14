#ifndef MYSOURCE_H
#define MYSOURCE_H

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
using namespace std;
struct SourceDrugInfo {
private:
    string drug_name;
    bool step_therapy_required = false;
    bool biosimilar_preferred = false;
    int auth_period_months = 0;
    string notes;

public:
    /** Set canonical drug name used in output JSON. */
    void setDrugName(const string& name) { drug_name = name; }
    /** Get canonical drug name used in output JSON. */
    string getDrugName() const { return drug_name; }

    /** Mark whether step-therapy is required for this drug. */
    void setStepTherapyRequired(bool step_therapy_required){
        this->step_therapy_required = step_therapy_required;
    }
    /** Mark whether biosimilar preference is called out for this drug. */
    void setBiosimilarPreferred(bool biosimilar_preferred){
        this->biosimilar_preferred = biosimilar_preferred;
    }
    /** Set authorization period length in months (0 if unknown). */
    void setAuthPeriodMonths(int auth_period_months){
        this->auth_period_months = auth_period_months;
    }
    /** Set free-text requirement notes for this drug. */
    void setNotes(string notes){
        this->notes = notes;
    }
    /** Return whether step-therapy is required. */
    bool getStepTherapyRequired() const {
        return step_therapy_required;
    }
    /** Return whether biosimilar preference is called out. */
    bool getBiosimilarPreferred() const {
        return biosimilar_preferred;
    }
    /** Return authorization period length in months (0 if unknown). */
    int getAuthPeriodMonths() const {
        return auth_period_months;
    }
    /** Return free-text requirement notes for this drug. */
    string getNotes() const {
        return notes;
    }
};
class MySource {
protected:
    string payer_key;
    string source_id;
    string source_type;
    string source_name;
    string source_date;
    string retrieved_date;
    string filename;
    string fax_number;
public:
    /** Set payer key used to derive input file paths and source IDs. */
    void setPayerKey(const string& k) { 
        payer_key = k; 
    }
    /** Return payer key used for this source parser instance. */
    string getPayerKey() const { 
        return payer_key; 
    }
    /** Return last source filename read by this parser. */
    string getFilename() const {
        return filename;
    }

    /** Parse source text and populate this parser's normalized fields. */
    virtual void sourceReader() = 0;
};
#endif
