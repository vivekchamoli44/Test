#pragma once
#include <string>
#include <vector>
#include <memory>
#include "nlohmann/json.h"
namespace health {\
struct User {
    int id = 0;
    std::string username; // Also serves as/mirrors email
    std::string email;    // Gmail address
    std::string password_hash;
    std::string role; // "patient" or "asha_worker"
    std::string full_name;
    std::string phone;
    std::string village_area;
    int age = 0;
    std::string gender;
    std::string created_at;

    nlohmann::json to_json() const {
        return {
            {"id", id},
            {"username", username},
            {"email", email.empty() ? username : email},
            {"role", role},
            {"full_name", full_name},
            {"phone", phone},
            {"village_area", village_area},
            {"age", age},
            {"gender", gender},
            {"created_at", created_at}
        };
    }
};

struct Assessment {
    int id = 0;
    int patient_id = 0;
    std::string patient_name;
    int age = 0;
    std::string gender;
    std::string phone;
    std::string village_area;

    // Clinical Vitals
    int bp_systolic = 120;
    int bp_diastolic = 80;
    int blood_sugar = 95;
    int heart_rate = 72;
    int oxygen_level = 98;
    double temperature = 98.6;
    std::string symptoms_text;
    std::string medical_history;

    // AI Results
    int risk_score = 0;
    std::string risk_level; // "Low", "Moderate", "High", "Critical"
    std::vector<std::string> possible_chronic_diseases;
    std::string recommended_doctor;
    std::vector<std::string> other_symptoms_to_watch;
    std::vector<std::string> precautions;
    std::string immediate_actions;

    // ASHA Health Worker Actions
    bool alert_sent = false;
    std::string alert_sent_at;
    std::string alert_message;
    bool asha_contacted = false;
    std::string asha_notes;
    std::string created_at;

    nlohmann::json to_json() const {
        return {
            {"id", id},
            {"patient_id", patient_id},
            {"patient_name", patient_name},
            {"age", age},
            {"gender", gender},
            {"phone", phone},
            {"village_area", village_area},
            {"bp_systolic", bp_systolic},
            {"bp_diastolic", bp_diastolic},
            {"blood_sugar", blood_sugar},
            {"heart_rate", heart_rate},
            {"oxygen_level", oxygen_level},
            {"temperature", temperature},
            {"symptoms_text", symptoms_text},
            {"medical_history", medical_history},
            {"risk_score", risk_score},
            {"risk_level", risk_level},
            {"possible_chronic_diseases", possible_chronic_diseases},
            {"recommended_doctor", recommended_doctor},
            {"other_symptoms_to_watch", other_symptoms_to_watch},
            {"precautions", precautions},
            {"immediate_actions", immediate_actions},
            {"alert_sent", alert_sent},
            {"alert_sent_at", alert_sent_at},
            {"alert_message", alert_message},
            {"asha_contacted", asha_contacted},
            {"asha_notes", asha_notes},
            {"created_at", created_at}
        };
    }
};

struct AIResult {
    int risk_score = 15;
    std::string risk_level = "Low";
    std::vector<std::string> possible_chronic_diseases;
    std::string recommended_doctor = "General Physician";
    std::vector<std::string> other_symptoms_to_watch;
    std::vector<std::string> precautions;
    std::string immediate_actions;
};

// ============================================================================
// OOP: Person / Patient / AshaWorker hierarchy (inheritance + polymorphism)
// ----------------------------------------------------------------------------
// `User` above stays the plain storage/API record (what gets saved to MySQL
// and sent over JSON) so none of that is touched. This small hierarchy is
// built on top of a User purely to demonstrate role-specific behavior via
// a virtual method and dynamic dispatch.
// ============================================================================

class Person {
protected:
    std::string full_name_;
    std::string village_area_;

public:
    Person(const std::string& full_name, const std::string& village_area)
        : full_name_(full_name), village_area_(village_area) {}

    virtual ~Person() = default;

    const std::string& full_name() const { return full_name_; }

    // Overridden by each role below — same call, different behavior (polymorphism).
    virtual std::string role_description() const {
        return "Community member";
    }
};

class Patient : public Person {
public:
    Patient(const std::string& full_name, const std::string& village_area)
        : Person(full_name, village_area) {}

    std::string role_description() const override {
        return "Patient - tracks personal health assessments and ASHA alerts";
    }
};

class AshaWorker : public Person {
public:
    AshaWorker(const std::string& full_name, const std::string& village_area)
        : Person(full_name, village_area) {}

    std::string role_description() const override {
        return "ASHA Worker - triages community members by risk and dispatches alerts";
    }
};

// Builds the right Person subclass from a stored User record.
inline std::unique_ptr<Person> make_person(const User& u) {
    if (u.role == "asha_worker") {
        return std::make_unique<AshaWorker>(u.full_name, u.village_area);
    }
    return std::make_unique<Patient>(u.full_name, u.village_area);
}

} // namespace health
