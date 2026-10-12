#pragma once
/**
 * ============================================================================
 * Module: Ai integration with gemni and patient module : start new assessment
 * Implemented by: Vivek Chamoli
 * ============================================================================
 * Description:
 *  - Clinical Diagnostic AI Engine for Early Chronic Disease Detection.
 *  - Integrates Google Gemini Generative AI (gemini-1.5-flash / gemini-pro)
 *    via secure HTTPS API calls using native Windows WinHTTP.
 *  - Loads the Gemini API key securely from .env or environment variables.
 *  - Delivers a structured clinical response:
 *     1. risk_score (0 - 100)
 *     2. risk_level ("Critical", "High", "Moderate", "Low")
 *     3. possible_chronic_diseases
 *     4. recommended_doctor_specialty
 *     5. other_symptoms_to_watch
 *     6. precautions
 *     7. immediate_actions
 *  - Google Gemini is the sole diagnostic engine: there is no offline or
 *    rule-based fallback. If the API key is missing or the request fails,
 *    the assessment endpoint reports the failure instead of guessing.
 * ============================================================================
 */

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <winhttp.h>
#include "crow_all.h"
#include "models.h"
#include "db_history.h"
#include "nlohmann/json.h"

#pragma comment(lib, "winhttp.lib")

namespace health {

// Helper to read key=value from .env files
inline std::string load_env_var(const std::string& key_name) {
    // 1. Check system environment
    const char* sys_val = std::getenv(key_name.c_str());
    if (sys_val && std::string(sys_val).length() > 0) {
        return std::string(sys_val);
    }

    // 2. Search candidate paths for .env
    std::vector<std::string> env_candidates = {
        ".env",
        "../.env",
        "backend/.env",
        "../../.env"
    };

    for (const auto& path : env_candidates) {
        std::ifstream f(path);
        if (!f.is_open()) continue;

        std::string line;
        while (std::getline(f, line)) {
            // Trim whitespace
            auto start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos || line[start] == '#') continue;

            auto eq_pos = line.find('=', start);
            if (eq_pos == std::string::npos) continue;

            std::string k = line.substr(start, eq_pos - start);
            // Trim key
            auto k_end = k.find_last_not_of(" \t");
            if (k_end != std::string::npos) k = k.substr(0, k_end + 1);

            if (k == key_name) {
                std::string v = line.substr(eq_pos + 1);
                auto v_start = v.find_first_not_of(" \t");
                auto v_end = v.find_last_not_of(" \t\r\n");
                if (v_start != std::string::npos && v_end != std::string::npos) {
                    v = v.substr(v_start, v_end - v_start + 1);
                    // Strip optional quotes
                    if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\''))) {
                        v = v.substr(1, v.size() - 2);
                    }
                    return v;
                }
            }
        }
    }
    return "";
}



// Gemini Clinical AI Engine (Implemented by Vivek Chamoli)
class GeminiAIEngine {
private:
    std::string api_key_;

    static std::wstring to_wide(const std::string& str) {
        if (str.empty()) return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
        return wstr;
    }

    // Native Windows HTTPS POST for Google Gemini REST API
    std::string http_post(const std::wstring& host, const std::wstring& path, const std::string& json_body) {
        std::string response_data;
        HINTERNET hSession = WinHttpOpen(L"AI-Health-Center-Vivek/2.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "";

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return "";
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
                                                NULL, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        // Set HTTPS timeouts (10 seconds)
        WinHttpSetTimeouts(hRequest, 5000, 5000, 10000, 10000);

        std::wstring headers = L"Content-Type: application/json\r\n";
        BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(),
                                           (LPVOID)json_body.c_str(), (DWORD)json_body.length(),
                                           (DWORD)json_body.length(), 0);

        if (bResults) {
            bResults = WinHttpReceiveResponse(hRequest, NULL);
        }

        if (bResults) {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;
            do {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                if (dwSize == 0) break;

                std::vector<char> buffer(dwSize + 1, 0);
                if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
                    response_data.append(buffer.data(), dwDownloaded);
                }
            } while (dwSize > 0);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response_data;
    }

    // Thrown for a Gemini failure that is worth a brief same-model retry
    // (the model is momentarily overloaded, e.g. HTTP 503 / UNAVAILABLE, or
    // the network call itself produced no response).
    struct GeminiOverloadedError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // Thrown when this model's free-tier quota is exhausted (HTTP 429 /
    // RESOURCE_EXHAUSTED). Retrying the same model is pointless — Google
    // reports these resets in hours, not seconds — so the caller should
    // move straight to the next free-tier model instead.
    struct GeminiQuotaExhaustedError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // Single attempt at calling Gemini and parsing its structured JSON reply.
    // Throws GeminiOverloadedError / GeminiQuotaExhaustedError for conditions
    // worth falling back on, std::runtime_error for everything else (bad key,
    // malformed response) which should fail the whole assessment immediately.
    AIResult request_gemini_assessment(const Assessment& req, const std::string& model) {
        std::string prompt =
            "You are an expert AI clinical health diagnostician specialized in early chronic disease detection and community triage for ASHA health workers in India.\n"
            "Evaluate this patient's assessment data carefully:\n"
            "- Age: " + std::to_string(req.age) + "\n"
            "- Gender: " + req.gender + "\n"
            "- Patient Reported Symptoms & Duration: " + req.symptoms_text + "\n"
            "- Medical History, Family Risk & Lifestyle: " + req.medical_history + "\n\n"
            "Return a STRICT JSON response ONLY (no markdown formatting, no ```json codeblocks, plain text JSON only) with these exact keys:\n"
            "{\n"
            "  \"risk_score\": <integer 0-100>,\n"
            "  \"risk_level\": <\"Low\" | \"Moderate\" | \"High\" | \"Critical\">,\n"
            "  \"possible_chronic_diseases\": [<list of probable conditions>],\n"
            "  \"recommended_doctor_specialty\": <string e.g. \"Cardiologist\", \"Endocrinologist\", \"General Physician\">,\n"
            "  \"other_symptoms_to_watch\": [<list of other warning symptoms to monitor>],\n"
            "  \"precautions\": [<list of actionable lifestyle and dietary precautions>],\n"
            "  \"immediate_actions\": <concise clinical next action>\n"
            "}";

        nlohmann::json request_payload = {
            {"contents", {{{"parts", {{{"text", prompt}}}}}}},
            {"generationConfig", {{"temperature", 0.2}, {"response_mime_type", "application/json"}}}
        };

        std::wstring host = L"generativelanguage.googleapis.com";
        std::wstring path = L"/v1beta/models/" + to_wide(model) + L":generateContent?key=" + to_wide(api_key_);

        std::string raw_resp = http_post(host, path, request_payload.dump());
        if (raw_resp.empty()) {
            throw GeminiOverloadedError("Gemini API (" + model + ") did not respond. Check network connectivity.");
        }

        auto parsed = nlohmann::json::parse(raw_resp);

        if (parsed.contains("error")) {
            std::string status = parsed["error"].value("status", "");
            int code = parsed["error"].value("code", 0);
            std::string msg = "Gemini API (" + model + ") error: " + parsed["error"].dump();
            if (status == "RESOURCE_EXHAUSTED" || code == 429) {
                throw GeminiQuotaExhaustedError(msg);
            }
            if (status == "UNAVAILABLE" || code == 503) {
                throw GeminiOverloadedError(msg);
            }
            throw std::runtime_error(msg);
        }

        if (!parsed.contains("candidates") || parsed["candidates"].empty()) {
            throw std::runtime_error("Gemini API (" + model + ") returned no candidates.");
        }

        auto& cand = parsed["candidates"][0];
        if (!cand.contains("content") || !cand["content"].contains("parts") || cand["content"]["parts"].empty()) {
            throw std::runtime_error("Gemini API (" + model + ") response missing content.");
        }

        std::string text_content = cand["content"]["parts"][0]["text"].get<std::string>();

        // Clean any markdown if Gemini added it despite prompt
        if (text_content.find("```json") != std::string::npos) {
            auto p1 = text_content.find("```json");
            text_content = text_content.substr(p1 + 7);
        } else if (text_content.find("```") != std::string::npos) {
            auto p1 = text_content.find("```");
            text_content = text_content.substr(p1 + 3);
        }
        if (text_content.rfind("```") != std::string::npos) {
            text_content = text_content.substr(0, text_content.rfind("```"));
        }

        auto ai_json = nlohmann::json::parse(text_content);

        AIResult res;
        res.risk_score = ai_json.value("risk_score", 45);
        res.risk_level = ai_json.value("risk_level", "Moderate");
        if (ai_json.contains("possible_chronic_diseases") && ai_json["possible_chronic_diseases"].is_array()) {
            for (auto& item : ai_json["possible_chronic_diseases"]) {
                res.possible_chronic_diseases.push_back(item.get<std::string>());
            }
        }
        res.recommended_doctor = ai_json.value("recommended_doctor_specialty", "General Physician");
        if (ai_json.contains("other_symptoms_to_watch") && ai_json["other_symptoms_to_watch"].is_array()) {
            for (auto& item : ai_json["other_symptoms_to_watch"]) {
                res.other_symptoms_to_watch.push_back(item.get<std::string>());
            }
        }
        if (ai_json.contains("precautions") && ai_json["precautions"].is_array()) {
            for (auto& item : ai_json["precautions"]) {
                res.precautions.push_back(item.get<std::string>());
            }
        }
        res.immediate_actions = ai_json.value("immediate_actions", "Consult physician for care assessment.");
        std::cout << "[Vivek - AI Module] Successfully parsed live Gemini AI diagnosis (model: " << model << ")! Risk: " << res.risk_score << std::endl;
        return res;
    }

public:
    GeminiAIEngine(const std::string& explicit_key = "") {
        if (!explicit_key.empty()) {
            api_key_ = explicit_key;
        } else {
            api_key_ = load_env_var("GEMINI_API_KEY");
        }
    }

    bool has_api_key() const { return !api_key_.empty() && api_key_.length() > 10; }

    // Primary AI Assessment Method (Google Gemini only — no offline/rule-based fallback).
    // Tries a sequence of free-tier Gemini models (lightest/highest-quota first),
    // retrying a model briefly only when it is momentarily overloaded, and moving
    // straight to the next model when its quota is exhausted or it fails outright.
    AIResult assess_patient(const Assessment& req) {
        if (!has_api_key()) {
            throw std::runtime_error("Gemini API key is not configured. Set GEMINI_API_KEY in .env or config.json.");
        }

        // Free-tier models only, ordered by typical free daily quota (highest first).
        // Pro/preview/experimental models are intentionally excluded — their free-tier
        // quotas are far smaller and exhaust almost immediately under real usage.
        static const std::vector<std::string> models = {
            "gemini-flash-lite-latest",
            "gemini-2.5-flash-lite",
            "gemini-2.5-flash"
        };
        const int max_attempts_per_model = 2;
        std::string last_error = "Unknown error.";

        for (const auto& model : models) {
            for (int attempt = 1; attempt <= max_attempts_per_model; ++attempt) {
                try {
                    std::cout << "[Vivek - AI Module] Sending assessment request to Google Gemini API (model: "
                              << model << ", attempt " << attempt << "/" << max_attempts_per_model << ")..." << std::endl;
                    return request_gemini_assessment(req, model);
                } catch (const GeminiQuotaExhaustedError& e) {
                    // This model's free-tier quota is used up for the day — retrying
                    // it will not help. Move straight to the next model.
                    last_error = e.what();
                    std::cout << "[Vivek - AI Module] " << last_error << std::endl;
                    break;
                } catch (const GeminiOverloadedError& e) {
                    // Momentary overload — worth a short same-model retry.
                    last_error = e.what();
                    std::cout << "[Vivek - AI Module] " << last_error << std::endl;
                    if (attempt < max_attempts_per_model) {
                        Sleep(800 * attempt);
                    }
                } catch (const std::exception& e) {
                    // Any other failure for this specific model (unexpected response
                    // shape, etc.) — don't let it abort the whole fallback chain.
                    last_error = e.what();
                    std::cout << "[Vivek - AI Module] " << last_error << std::endl;
                    break;
                }
            }
            std::cout << "[Vivek - AI Module] Model '" << model << "' unavailable, trying next free-tier Gemini model..." << std::endl;
        }

        throw std::runtime_error("All free-tier Gemini models are currently unavailable. Last error: " + last_error);
    }
};

// Route Registration for Vivek Chamoli's AI Module
inline void register_ai_assessment_routes(crow::SimpleApp& app, DatabaseManager& db, GeminiAIEngine& ai) {

    // POST /api/assessments - Start New Assessment (Implemented by Vivek Chamoli)
    CROW_ROUTE(app, "/api/assessments").methods(crow::HTTPMethod::Post)
    ([&db, &ai](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        // 1. Parse and validate the incoming request payload
        Assessment a;
        try {
            auto body = nlohmann::json::parse(req.body);
            a.patient_id = body.value("patient_id", 2);
            a.patient_name = body.value("patient_name", "Patient");
            a.age = body.value("age", 45);
            a.gender = body.value("gender", "Not Specified");
            a.phone = body.value("phone", "+91 98765 00000");
            a.village_area = body.value("village_area", "Community Area");

            // Clinical Vitals & Measurements removed from assessment input
            a.bp_systolic = body.value("bp_systolic", 0);
            a.bp_diastolic = body.value("bp_diastolic", 0);
            a.blood_sugar = body.value("blood_sugar", 0);
            a.heart_rate = body.value("heart_rate", 0);
            a.oxygen_level = body.value("oxygen_level", 0);
            a.temperature = body.value("temperature", 0.0);

            // Clinical Questionnaire Inputs: symptoms & lifestyle habits
            a.symptoms_text = body.value("symptoms_text", "");
            a.medical_history = body.value("medical_history", "");
        } catch (const std::exception& e) {
            res.code = 400;
            res.body = nlohmann::json({{"error", std::string("Invalid request payload: ") + e.what()}}).dump();
            return res;
        }

        // 2. Run the AI Assessment — Google Gemini only, no offline fallback.
        //    Any failure (missing key, network error, bad response) is reported
        //    to the caller instead of silently substituting a different result.
        AIResult result;
        try {
            result = ai.assess_patient(a);
        } catch (const std::exception& e) {
            res.code = 503;
            res.body = nlohmann::json({
                {"success", false},
                {"module", "AI Integration with Gemini & Patient Assessment"},
                {"error", std::string("Gemini AI assessment unavailable: ") + e.what()}
            }).dump();
            return res;
        }

        a.risk_score = result.risk_score;
        a.risk_level = result.risk_level;
        a.possible_chronic_diseases = result.possible_chronic_diseases;
        a.recommended_doctor = result.recommended_doctor;
        a.other_symptoms_to_watch = result.other_symptoms_to_watch;
        a.precautions = result.precautions;
        a.immediate_actions = result.immediate_actions;

        // alert_sent stays false regardless of risk level — the ASHA worker
        // must explicitly press "Send Alert" on the dashboard to dispatch one
        // (see auth_notification.h: POST /api/worker/assessments/<id>/alert).

        db.save_assessment(a);

        res.code = 201;
        res.body = nlohmann::json({
            {"success", true},
            {"module", "AI Integration with Gemini & Patient Assessment"},
            {"developer", "Vivek Chamoli"},
            {"assessment", a.to_json()}
        }).dump();
        return res;
    });
}

} // namespace health
