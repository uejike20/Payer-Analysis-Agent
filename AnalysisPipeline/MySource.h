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
    void setDrugName(const string& name) { drug_name = name; }
    string getDrugName() const { return drug_name; }

    void setStepTherapyRequired(bool step_therapy_required){
        this->step_therapy_required = step_therapy_required;
    }
    void setBiosimilarPreferred(bool biosimilar_preferred){
        this->biosimilar_preferred = biosimilar_preferred;
    }
    void setAuthPeriodMonths(int auth_period_months){
        this->auth_period_months = auth_period_months;
    }
    void setNotes(string notes){
        this->notes = notes;
    }
    bool getStepTherapyRequired() const {
        return step_therapy_required;
    }
    bool getBiosimilarPreferred() const {
        return biosimilar_preferred;
    }
    int getAuthPeriodMonths() const {
        return auth_period_months;
    }
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
    void setPayerKey(const string& k) { 
        payer_key = k; 
    }
    string getPayerKey() const { 
        return payer_key; 
    }
    string getFilename() const {
        return filename;
    }

    virtual void sourceReader() = 0;
};
#endif
