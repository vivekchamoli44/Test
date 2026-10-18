#define CROW_MAIN
/**
 * ============================================================================
 * AI-POWERED EARLY HEALTH DETECTION SYSTEM (C++)
 * Built with Crow C++, MySQL, HTML, CSS, JavaScript, and Gemini AI.
 * ============================================================================
 * Team Modules:
 *  1. Login & Sign Up & ASHA Worker: Send Notification
 *     -> Implemented by: Yashwardhan Rohilla  [auth_notification.h]
 *  2. AI Integration with Gemini & Patient: Start New Assessment
 *     -> Implemented by: Vivek Chamoli        [ai_assessment.h]
 *  3. Database and Testing & Patient: Show Previous Assessment
 *     -> Implemented by: Anjali               [db_history.h]
 *  4. Patient & ASHA Worker Dashboard & ASHA Worker: High Risk Individual
 *     -> Implemented by: Nandini              [dashboard_triage.h]
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include "crow_all.h"
#include "db_history.h"
#include "ai_assessment.h"
#include "auth_notification.h"
#include "dashboard_triage.h"

namespace fs = std::filesystem;

// Static file reader for frontend assets
std::string read_frontend_file(const std::string& relative_path) {
    std::vector<std::string> search_paths = {
        "../frontend/" + relative_path,
        "./frontend/" + relative_path,
        "../../frontend/" + relative_path,
        "frontend/" + relative_path
    };

    for (const auto& p : search_paths) {
        if (fs::exists(p)) {
            std::ifstream file(p, std::ios::binary);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                return buffer.str();
            }
        }
    }
    return "";
}

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "                      MEDICO (C++)                      " << std::endl;
    std::cout << "   Community Disease Prevention & ASHA Worker Triage    " << std::endl;
    std::cout << "========================================================" << std::endl;

    // Load Configuration from .env / system environment variables
    auto env_or = [](const std::string& key, const std::string& fallback) {
        std::string v = health::load_env_var(key);
        return v.empty() ? fallback : v;
    };
    auto env_or_int = [&](const std::string& key, int fallback) {
        std::string v = health::load_env_var(key);
        if (v.empty()) return fallback;
        try { return std::stoi(v); } catch (...) { return fallback; }
    };

    std::string db_host = env_or("MYSQL_HOST", "127.0.0.1");
    int db_port = env_or_int("MYSQL_PORT", 3306);
    std::string db_user = env_or("MYSQL_USER", "root");
    std::string db_pass = env_or("MYSQL_PASSWORD", "");
    std::string db_name = env_or("MYSQL_DATABASE", "ai_health_center");
    std::string gemini_key = health::load_env_var("GEMINI_API_KEY");
    int port = env_or_int("SERVER_PORT", 8080);

    // 1. Module: Database & Testing (Implemented by Anjali)
    health::DatabaseManager db(db_host, db_port, db_user, db_pass, db_name);
    db.init_database();

    // 2. Module: AI Integration with Gemini (Implemented by Vivek Chamoli)
    health::GeminiAIEngine ai(gemini_key);
    if (ai.has_api_key()) {
        std::cout << "[Vivek - AI Module] Google Gemini cloud AI online." << std::endl;
    } else {
        std::cout << "[Vivek - AI Module] WARNING: No GEMINI_API_KEY configured. AI assessments will fail until one is set." << std::endl;
    }

    // 3. Module: Login & Sign Up Session Manager (Implemented by Yashwardhan Rohilla)
    health::AuthSessionManager auth;

    // Initialize Crow Web Server
    crow::SimpleApp app;

    // Register 4 Team Modules
    health::register_db_history_routes(app, db);           // Anjali's Module
    health::register_ai_assessment_routes(app, db, ai);    // Vivek Chamoli's Module
    health::register_auth_notification_routes(app, db, auth); // Yashwardhan Rohilla's Module
    health::register_dashboard_triage_routes(app, db, auth);   // Nandini's Module

    // Frontend Static Assets Serving
    auto serve_page = [](const std::string& page_file) {
        crow::response res;
        std::string content = read_frontend_file(page_file);
        res.set_header("Content-Type", "text/html; charset=utf-8");
        res.body = content.empty() ? ("<h1>Error: frontend/" + page_file + " not found</h1>") : content;
        return res;
    };

    CROW_ROUTE(app, "/")([serve_page]() { return serve_page("index.html"); });
    CROW_ROUTE(app, "/index.html")([serve_page]() { return serve_page("index.html"); });
    CROW_ROUTE(app, "/patient.html")([serve_page]() { return serve_page("patient.html"); });
    CROW_ROUTE(app, "/worker.html")([serve_page]() { return serve_page("worker.html"); });

    CROW_ROUTE(app, "/css/<string>")([](const std::string& file_name) {
        crow::response res;
        std::string content = read_frontend_file("css/" + file_name);
        res.set_header("Content-Type", "text/css; charset=utf-8");
        res.body = content;
        return res;
    });

    CROW_ROUTE(app, "/js/<string>")([](const std::string& file_name) {
        crow::response res;
        std::string content = read_frontend_file("js/" + file_name);
        res.set_header("Content-Type", "application/javascript; charset=utf-8");
        res.body = content;
        return res;
    });

    // System Health Status
    CROW_ROUTE(app, "/api/health")([&db]() {
        crow::response res;
        res.set_header("Content-Type", "application/json");
        res.body = nlohmann::json({
            {"status", "online"},
            {"system", "Medico - Early Health Detection Center"},
            {"mysql_database", db.is_connected() ? "Connected" : "In-Memory Fallback Active"},
            {"team_modules", {
                {"Login, Sign Up & ASHA Send Notification", "Yashwardhan Rohilla"},
                {"AI Gemini Integration & Start New Assessment", "Vivek Chamoli"},
                {"Database, Testing & Show Previous Assessment", "Anjali"},
                {"Dashboards & ASHA High Risk Individual", "Nandini"}
            }}
        }).dump();
        return res;
    });

    std::cout << "[Server] Online at http://localhost:" << port << std::endl;
    std::cout << "[Server] Open http://localhost:" << port << " in your browser to access the portal." << std::endl;

    app.port(port).multithreaded().run();
    return 0;
}
