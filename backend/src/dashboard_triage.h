#pragma once
/**
 * ============================================================================
 * Module: Patient and asha worker dashboard and Asha worker module : High risk individual
 * Implemented by: Nandini
 * ============================================================================
 * Description:
 *  - Aggregates metrics and statistics for Patient and ASHA Worker dashboards.
 *  - ASHA Worker Module: High Risk Individual prioritization.
 *    Fetches all patient assessments sorted strictly by HIGHEST risk score
 *    to LOWEST risk score for community health triage.
 *  - Contact Patient action: Facilitates immediate communication (Call/WhatsApp)
 *    and logs home visit outreach notes (/api/worker/assessments/<id>/contact).
 * ============================================================================
 */

#include <string>
#include <vector>
#include <iostream>
#include "crow_all.h"
#include "models.h"
#include "db_history.h"
#include "auth_notification.h"
#include "nlohmann/json.h"

namespace health {

inline void register_dashboard_triage_routes(crow::SimpleApp& app, DatabaseManager& db, AuthSessionManager& auth) {

    // GET /api/worker/assessments - High Risk Individual Prioritization (Implemented by Nandini)
    // Only returns patients from the logged-in ASHA worker's own village/area.
    CROW_ROUTE(app, "/api/worker/assessments").methods(crow::HTTPMethod::Get)
    ([&db, &auth](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        User worker;
        if (!require_asha_worker(req, auth, worker, res)) return res;

        // Assessments returned strictly sorted from HIGHEST risk score to LOWEST
        auto all_assessments = db.get_all_assessments_sorted();

        std::vector<Assessment> assessments;
        for (const auto& a : all_assessments) {
            if (in_worker_area(worker, a.village_area)) assessments.push_back(a);
        }

        nlohmann::json arr = nlohmann::json::array();
        int critical_count = 0;
        int high_count = 0;
        int alerts_sent_count = 0;

        for (const auto& a : assessments) {
            arr.push_back(a.to_json());
            if (a.risk_level == "Critical") critical_count++;
            // High-risk KPI is score-based (risk_score > 80) rather than keyed off
            // Gemini's free-text risk_level label, since that label has no fixed
            // scoring band and won't reliably line up with a numeric threshold.
            if (a.risk_score > 80) high_count++;
            if (a.alert_sent) alerts_sent_count++;
        }

        res.code = 200;
        res.body = nlohmann::json({
            {"success", true},
            {"module", "ASHA Worker Module: High Risk Individual & Dashboard"},
            {"developer", "Nandini"},
            {"village_area", worker.village_area},
            {"total_screenings", assessments.size()},
            {"critical_risk_count", critical_count},
            {"high_risk_count", high_count},
            {"alerts_sent_count", alerts_sent_count},
            {"assessments", arr}
        }).dump();
        return res;
    });

    // POST /api/worker/assessments/<int>/contact - Contact Patient & Outreach Logging (Implemented by Nandini)
    CROW_ROUTE(app, "/api/worker/assessments/<int>/contact").methods(crow::HTTPMethod::Post)
    ([&db, &auth](const crow::request& req, int assessment_id){
        crow::response res;
        res.set_header("Content-Type", "application/json");

        User worker;
        if (!require_asha_worker(req, auth, worker, res)) return res;

        std::string patient_area;
        if (!db.get_assessment_area(assessment_id, patient_area)) {
            res.code = 404;
            res.body = nlohmann::json({{"error", "Assessment record not found."}}).dump();
            return res;
        }
        if (!in_worker_area(worker, patient_area)) {
            res.code = 403;
            res.body = nlohmann::json({{"error", "This patient is outside your assigned area."}}).dump();
            return res;
        }

        std::string notes = "";
        try {
            if (!req.body.empty()) {
                auto body = nlohmann::json::parse(req.body);
                notes = body.value("notes", "");
            }
        } catch (...) {}

        if (notes.empty()) {
            notes = "ASHA health worker contacted patient for clinical follow up.";
        }

        if (db.log_contact(assessment_id, notes)) {
            res.code = 200;
            res.body = nlohmann::json({
                {"success", true},
                {"module", "ASHA Worker: Contact Patient & Outreach"},
                {"developer", "Nandini"},
                {"message", "Patient contact recorded successfully."},
                {"assessment_id", assessment_id}
            }).dump();
        } else {
            res.code = 404;
            res.body = nlohmann::json({{"error", "Assessment record not found."}}).dump();
        }
        return res;
    });
}

} // namespace health
