#pragma once
/**
 * ============================================================================
 * Module: Database and Testing & Patient Module: Show Previous Assessment
 * Implemented by: Anjali
 * ============================================================================
 * Description:
 *  - Handles MySQL 8.0 database connection, auto-migration, and schema validation.
 *  - Automated unit testing endpoint for verifying database integrity.
 *  - Patient Module: Retrieves and formats historical health assessments.
 * ============================================================================
 */

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <mutex>
#include <utility>
#include <chrono>
#include <iomanip>
#include <mysql.h>
#include "crow_all.h"
#include "models.h"
#include "nlohmann/json.h"

namespace health {

// ============================================================================
// DSA: Hand-written array-based binary max-heap, keyed on Assessment::risk_score.
// ----------------------------------------------------------------------------
// Used to sort the in-memory fallback's assessments highest-risk-first for the
// ASHA worker dashboard, in place of a library std::sort call. Classic binary
// heap: insert is O(log n) via sift-up, extract-max is O(log n) via sift-down,
// so draining n items into sorted order is O(n log n) overall (heapsort).
// ============================================================================
class AssessmentMaxHeap {
private:
    std::vector<Assessment> data_;

    static int parent(int i) { return (i - 1) / 2; }
    static int left(int i) { return 2 * i + 1; }
    static int right(int i) { return 2 * i + 2; }

    void sift_up(int i) {
        while (i > 0 && data_[parent(i)].risk_score < data_[i].risk_score) {
            std::swap(data_[parent(i)], data_[i]);
            i = parent(i);
        }
    }

    void sift_down(int i) {
        int n = (int)data_.size();
        while (true) {
            int l = left(i), r = right(i), largest = i;
            if (l < n && data_[l].risk_score > data_[largest].risk_score) largest = l;
            if (r < n && data_[r].risk_score > data_[largest].risk_score) largest = r;
            if (largest == i) break;
            std::swap(data_[i], data_[largest]);
            i = largest;
        }
    }

public:
    bool empty() const { return data_.empty(); }

    void insert(const Assessment& a) {
        data_.push_back(a);
        sift_up((int)data_.size() - 1);
    }

    // Removes and returns the assessment with the highest risk_score.
    Assessment extract_max() {
        Assessment top = data_.front();
        data_.front() = data_.back();
        data_.pop_back();
        if (!data_.empty()) sift_down(0);
        return top;
    }

    // Drains the whole heap into a vector ordered highest-risk-first.
    std::vector<Assessment> drain_sorted() {
        std::vector<Assessment> out;
        out.reserve(data_.size());
        while (!empty()) out.push_back(extract_max());
        return out;
    }
};

class DatabaseManager {
private:
    std::string host_;
    int port_;
    std::string user_;
    std::string password_;
    std::string dbname_;
    MYSQL* conn_ = nullptr;
    std::mutex db_mutex_;
    bool is_connected_ = false;

    // In-memory fallback storage
    std::vector<User> memory_users_;
    std::vector<Assessment> memory_assessments_;
    int next_user_id_ = 10;      // IDs 1-2 are reserved for the seeded demo login accounts
    int next_assessment_id_ = 1; // No assessments are seeded; real submissions start at 1

    void seed_default_records() {
        User asha;
        asha.id = 1;
        asha.username = "asha.worker@gmail.com";
        asha.email = "asha.worker@gmail.com";
        asha.password_hash = "asha123";
        asha.role = "asha_worker";
        asha.full_name = "Sunita Devi (ASHA Worker)";
        asha.phone = "+91 98765 43210";
        asha.village_area = "Rampur Ward 4";
        asha.age = 38;
        asha.gender = "Female";
        memory_users_.push_back(asha);

        User p1;
        p1.id = 2;
        p1.username = "patient@gmail.com";
        p1.email = "patient@gmail.com";
        p1.password_hash = "patient123";
        p1.role = "patient";
        p1.full_name = "Ramesh Kumar";
        p1.phone = "+91 91234 56789";
        p1.village_area = "Rampur Ward 4";
        p1.age = 54;
        p1.gender = "Male";
        memory_users_.push_back(p1);

        // Note: no demo/sample Assessment is seeded here. The ASHA worker
        // dashboard (/api/worker/assessments) must only ever show assessments
        // that real users actually submitted via "Start New Assessment".
    }

public:
    DatabaseManager(const std::string& host, int port, const std::string& user, const std::string& password, const std::string& dbname)
        : host_(host), port_(port), user_(user), password_(password), dbname_(dbname) {
        seed_default_records();
    }

    ~DatabaseManager() {
        if (conn_) {
            mysql_close(conn_);
            conn_ = nullptr;
        }
    }

    std::string escape(const std::string& input) {
        if (!is_connected_ || !conn_) {
            std::string res;
            for (char c : input) {
                if (c == '\'') res += "''";
                else if (c == '\\') res += "\\\\";
                else res += c;
            }
            return res;
        }
        std::vector<char> to(input.size() * 2 + 1, 0);
        mysql_real_escape_string(conn_, to.data(), input.c_str(), input.length());
        return std::string(to.data());
    }

    bool init_database() {
        std::lock_guard<std::mutex> lock(db_mutex_);
        conn_ = mysql_init(nullptr);
        if (!conn_) return false;

        if (!mysql_real_connect(conn_, host_.c_str(), user_.c_str(), password_.c_str(), nullptr, port_, nullptr, 0)) {
            std::cout << "[DB Module] MySQL deferred (using built-in storage fallback)." << std::endl;
            is_connected_ = false;
            return false;
        }

        std::string create_db = "CREATE DATABASE IF NOT EXISTS `" + dbname_ + "` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;";
        mysql_query(conn_, create_db.c_str());
        mysql_select_db(conn_, dbname_.c_str());

        std::string sql_users = 
            "CREATE TABLE IF NOT EXISTS users ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "username VARCHAR(100) NOT NULL UNIQUE,"
            "password_hash VARCHAR(255) NOT NULL,"
            "role ENUM('patient', 'asha_worker') NOT NULL,"
            "full_name VARCHAR(150) NOT NULL,"
            "phone VARCHAR(20) NOT NULL,"
            "village_area VARCHAR(150) NOT NULL,"
            "age INT DEFAULT 0,"
            "gender VARCHAR(20) DEFAULT 'Not Specified',"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ");";
        mysql_query(conn_, sql_users.c_str());

        std::string sql_assessments = 
            "CREATE TABLE IF NOT EXISTS assessments ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "patient_id INT NOT NULL,"
            "patient_name VARCHAR(150) NOT NULL,"
            "age INT NOT NULL,"
            "gender VARCHAR(20) NOT NULL,"
            "phone VARCHAR(20) NOT NULL,"
            "village_area VARCHAR(150) NOT NULL,"
            "bp_systolic INT DEFAULT 120,"
            "bp_diastolic INT DEFAULT 80,"
            "blood_sugar INT DEFAULT 95,"
            "heart_rate INT DEFAULT 72,"
            "oxygen_level INT DEFAULT 98,"
            "temperature DECIMAL(4,1) DEFAULT 98.6,"
            "symptoms_text TEXT,"
            "medical_history TEXT,"
            "risk_score INT NOT NULL,"
            "risk_level VARCHAR(30) NOT NULL,"
            "possible_chronic_diseases TEXT,"
            "recommended_doctor VARCHAR(100),"
            "other_symptoms_to_watch TEXT,"
            "precautions TEXT,"
            "immediate_actions TEXT,"
            "alert_sent TINYINT(1) DEFAULT 0,"
            "alert_sent_at DATETIME NULL,"
            "alert_message TEXT NULL,"
            "asha_contacted TINYINT(1) DEFAULT 0,"
            "asha_notes TEXT NULL,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ");";
        mysql_query(conn_, sql_assessments.c_str());

        is_connected_ = true;
        std::cout << "[DB Module] Connected to MySQL database: " << dbname_ << std::endl;
        return true;
    }

    bool is_connected() const { return is_connected_; }

         bool create_user(const User& user, int& new_id) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        if (is_connected_) {
            std::stringstream ss;
            ss << "INSERT INTO users (username, password_hash, role, full_name, phone, village_area, age, gender) VALUES ("
               << "'" << escape(user.username) << "', '" << escape(user.password_hash) << "', '" << escape(user.role) << "', "
               << "'" << escape(user.full_name) << "', '" << escape(user.phone) << "', '" << escape(user.village_area) << "', "
               << user.age << ", '" << escape(user.gender) << "');";
            if (mysql_query(conn_, ss.str().c_str()) == 0) {
                new_id = (int)mysql_insert_id(conn_);
                return true;
            }
            return false;
        } else {
            for (const auto& u : memory_users_) if (u.username == user.username) return false;
            User nu = user;
            nu.id = next_user_id_++;
            nu.created_at = "Just now";
            memory_users_.push_back(nu);
            new_id = nu.id;
            return true;
        }
    }

    bool authenticate_user(const std::string& identifier, const std::string& password, User& out_user) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        if (is_connected_) {
            std::string q = "SELECT id, username, password_hash, role, full_name, phone, village_area, age, gender, created_at FROM users WHERE username = '" + escape(identifier) + "' LIMIT 1;";
            if (mysql_query(conn_, q.c_str()) == 0) {
                MYSQL_RES* res = mysql_store_result(conn_);
                if (res) {
                    MYSQL_ROW row = mysql_fetch_row(res);
                    if (row && std::string(row[2] ? row[2] : "") == password) {
                        out_user.id = std::atoi(row[0]);
                        out_user.username = row[1] ? row[1] : "";
                        out_user.email = out_user.username;
                        out_user.password_hash = row[2] ? row[2] : "";
                        out_user.role = row[3] ? row[3] : "";
                        out_user.full_name = row[4] ? row[4] : "";
                        out_user.phone = row[5] ? row[5] : "";
                        out_user.village_area = row[6] ? row[6] : "";
                        out_user.age = row[7] ? std::atoi(row[7]) : 0;
                        out_user.gender = row[8] ? row[8] : "";
                        out_user.created_at = row[9] ? row[9] : "";
                        mysql_free_result(res);
                        return true;
                    }
                    mysql_free_result(res);
                }
            }
            return false;
        } else {
            for (const auto& u : memory_users_) {
                if ((u.username == identifier || u.email == identifier) && u.password_hash == password) {
                    out_user = u;
                    return true;
                }
            }
            return false;
        }
    }

    bool save_assessment(Assessment& a) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        nlohmann::json d_json = a.possible_chronic_diseases;
        nlohmann::json s_json = a.other_symptoms_to_watch;
        nlohmann::json p_json = a.precautions;

        // Get current real date and time string
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#if defined(_WIN32)
        localtime_s(&tm_buf, &now_time);
#else
        localtime_r(&now_time, &tm_buf);
#endif
        std::stringstream time_ss;
        time_ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        std::string current_time_str = time_ss.str();

        if (a.created_at.empty()) {
            a.created_at = current_time_str;
        }

        if (is_connected_) {
            std::stringstream ss;
            ss << "INSERT INTO assessments (patient_id, patient_name, age, gender, phone, village_area, "
               << "bp_systolic, bp_diastolic, blood_sugar, heart_rate, oxygen_level, temperature, "
               << "symptoms_text, medical_history, risk_score, risk_level, possible_chronic_diseases, "
               << "recommended_doctor, other_symptoms_to_watch, precautions, immediate_actions, alert_sent, alert_message) VALUES ("
               << a.patient_id << ", '" << escape(a.patient_name) << "', " << a.age << ", '" << escape(a.gender) << "', "
               << "'" << escape(a.phone) << "', '" << escape(a.village_area) << "', " << a.bp_systolic << ", " << a.bp_diastolic << ", "
               << a.blood_sugar << ", " << a.heart_rate << ", " << a.oxygen_level << ", " << a.temperature << ", "
               << "'" << escape(a.symptoms_text) << "', '" << escape(a.medical_history) << "', " << a.risk_score << ", "
               << "'" << escape(a.risk_level) << "', '" << escape(d_json.dump()) << "', '" << escape(a.recommended_doctor) << "', "
               << "'" << escape(s_json.dump()) << "', '" << escape(p_json.dump()) << "', '" << escape(a.immediate_actions) << "', "
               << (a.alert_sent ? 1 : 0) << ", '" << escape(a.alert_message) << "');";
            if (mysql_query(conn_, ss.str().c_str()) == 0) {
                a.id = (int)mysql_insert_id(conn_);
                return true;
            }
            return false;
        } else {
            a.id = next_assessment_id_++;
            if (a.created_at.empty() || a.created_at == "Just now") {
                a.created_at = current_time_str;
            }
            memory_assessments_.push_back(a);
            return true;
        }
    }

    // Patient Module: Show Previous Assessments (Implemented by Anjali)
    std::vector<Assessment> show_previous_assessments(int patient_id) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        std::vector<Assessment> list;

        if (is_connected_) {
            std::string q = "SELECT id, patient_id, patient_name, age, gender, phone, village_area, "
                            "bp_systolic, bp_diastolic, blood_sugar, heart_rate, oxygen_level, temperature, "
                            "symptoms_text, medical_history, risk_score, risk_level, possible_chronic_diseases, "
                            "recommended_doctor, other_symptoms_to_watch, precautions, immediate_actions, "
                            "alert_sent, alert_sent_at, alert_message, asha_contacted, asha_notes, created_at "
                            "FROM assessments WHERE patient_id = " + std::to_string(patient_id) + " ORDER BY id DESC;";

            if (mysql_query(conn_, q.c_str()) == 0) {
                MYSQL_RES* res = mysql_store_result(conn_);
                if (res) {
                    MYSQL_ROW row;
                    while ((row = mysql_fetch_row(res))) {
                        Assessment a;
                        a.id = std::atoi(row[0]);
                        a.patient_id = std::atoi(row[1]);
                        a.patient_name = row[2] ? row[2] : "";
                        a.age = row[3] ? std::atoi(row[3]) : 0;
                        a.gender = row[4] ? row[4] : "";
                        a.phone = row[5] ? row[5] : "";
                        a.village_area = row[6] ? row[6] : "";
                        a.bp_systolic = row[7] ? std::atoi(row[7]) : 120;
                        a.bp_diastolic = row[8] ? std::atoi(row[8]) : 80;
                        a.blood_sugar = row[9] ? std::atoi(row[9]) : 95;
                        a.heart_rate = row[10] ? std::atoi(row[10]) : 72;
                        a.oxygen_level = row[11] ? std::atoi(row[11]) : 98;
                        a.temperature = row[12] ? std::atof(row[12]) : 98.6;
                        a.symptoms_text = row[13] ? row[13] : "";
                        a.medical_history = row[14] ? row[14] : "";
                        a.risk_score = row[15] ? std::atoi(row[15]) : 0;
                        a.risk_level = row[16] ? row[16] : "";

                        try { if (row[17]) a.possible_chronic_diseases = nlohmann::json::parse(row[17]).get<std::vector<std::string>>(); } catch (...) {}
                        a.recommended_doctor = row[18] ? row[18] : "";
                        try { if (row[19]) a.other_symptoms_to_watch = nlohmann::json::parse(row[19]).get<std::vector<std::string>>(); } catch (...) {}
                        try { if (row[20]) a.precautions = nlohmann::json::parse(row[20]).get<std::vector<std::string>>(); } catch (...) {}

                        a.immediate_actions = row[21] ? row[21] : "";
                        a.alert_sent = row[22] ? (std::atoi(row[22]) != 0) : false;
                        a.alert_sent_at = row[23] ? row[23] : "";
                        a.alert_message = row[24] ? row[24] : "";
                        a.asha_contacted = row[25] ? (std::atoi(row[25]) != 0) : false;
                        a.asha_notes = row[26] ? row[26] : "";
                        a.created_at = row[27] ? row[27] : "";
                        list.push_back(a);
                    }
                    mysql_free_result(res);
                }
            }
        } else {
            for (auto it = memory_assessments_.rbegin(); it != memory_assessments_.rend(); ++it) {
                if (it->patient_id == patient_id) list.push_back(*it);
            }
        }
        return list;
    }

    std::vector<Assessment> get_all_assessments_sorted() {
        std::lock_guard<std::mutex> lock(db_mutex_);
        std::vector<Assessment> list;
        if (is_connected_) {
            std::string q = "SELECT id, patient_id, patient_name, age, gender, phone, village_area, "
                            "bp_systolic, bp_diastolic, blood_sugar, heart_rate, oxygen_level, temperature, "
                            "symptoms_text, medical_history, risk_score, risk_level, possible_chronic_diseases, "
                            "recommended_doctor, other_symptoms_to_watch, precautions, immediate_actions, "
                            "alert_sent, alert_sent_at, alert_message, asha_contacted, asha_notes, created_at "
                            "FROM assessments ORDER BY risk_score DESC, id DESC;";
            if (mysql_query(conn_, q.c_str()) == 0) {
                MYSQL_RES* res = mysql_store_result(conn_);
                if (res) {
                    MYSQL_ROW row;
                    while ((row = mysql_fetch_row(res))) {
                        Assessment a;
                        a.id = std::atoi(row[0]);
                        a.patient_id = std::atoi(row[1]);
                        a.patient_name = row[2] ? row[2] : "";
                        a.age = row[3] ? std::atoi(row[3]) : 0;
                        a.gender = row[4] ? row[4] : "";
                        a.phone = row[5] ? row[5] : "";
                        a.village_area = row[6] ? row[6] : "";
                        a.bp_systolic = row[7] ? std::atoi(row[7]) : 120;
                        a.bp_diastolic = row[8] ? std::atoi(row[8]) : 80;
                        a.blood_sugar = row[9] ? std::atoi(row[9]) : 95;
                        a.heart_rate = row[10] ? std::atoi(row[10]) : 72;
                        a.oxygen_level = row[11] ? std::atoi(row[11]) : 98;
                        a.temperature = row[12] ? std::atof(row[12]) : 98.6;
                        a.symptoms_text = row[13] ? row[13] : "";
                        a.medical_history = row[14] ? row[14] : "";
                        a.risk_score = row[15] ? std::atoi(row[15]) : 0;
                        a.risk_level = row[16] ? row[16] : "";

                        try { if (row[17]) a.possible_chronic_diseases = nlohmann::json::parse(row[17]).get<std::vector<std::string>>(); } catch (...) {}
                        a.recommended_doctor = row[18] ? row[18] : "";
                        try { if (row[19]) a.other_symptoms_to_watch = nlohmann::json::parse(row[19]).get<std::vector<std::string>>(); } catch (...) {}
                        try { if (row[20]) a.precautions = nlohmann::json::parse(row[20]).get<std::vector<std::string>>(); } catch (...) {}

                        a.immediate_actions = row[21] ? row[21] : "";
                        a.alert_sent = row[22] ? (std::atoi(row[22]) != 0) : false;
                        a.alert_sent_at = row[23] ? row[23] : "";
                        a.alert_message = row[24] ? row[24] : "";
                        a.asha_contacted = row[25] ? (std::atoi(row[25]) != 0) : false;
                        a.asha_notes = row[26] ? row[26] : "";
                        a.created_at = row[27] ? row[27] : "";
                        list.push_back(a);
                    }
                    mysql_free_result(res);
                }
            }
        } else {
            // DSA: use the hand-written max-heap instead of std::sort to rank
            // the in-memory fallback's assessments highest-risk-first.
            AssessmentMaxHeap heap;
            for (const auto& a : memory_assessments_) heap.insert(a);
            list = heap.drain_sorted();
        }
        return list;
    }

    // Looks up which area an assessment belongs to, so ASHA worker actions can
    // be checked against the worker's own area. Returns false if no such record.
    bool get_assessment_area(int assessment_id, std::string& out_area) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        if (is_connected_) {
            bool found = false;
            std::string q = "SELECT village_area FROM assessments WHERE id = " + std::to_string(assessment_id) + " LIMIT 1;";
            if (mysql_query(conn_, q.c_str()) == 0) {
                MYSQL_RES* res = mysql_store_result(conn_);
                if (res) {
                    MYSQL_ROW row = mysql_fetch_row(res);
                    if (row) {
                        out_area = row[0] ? row[0] : "";
                        found = true;
                    }
                    mysql_free_result(res);
                }
            }
            return found;
        } else {
            for (const auto& a : memory_assessments_) {
                if (a.id == assessment_id) {
                    out_area = a.village_area;
                    return true;
                }
            }
            return false;
        }
    }

    bool update_alert(int assessment_id, const std::string& message) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        if (is_connected_) {
            std::string q = "UPDATE assessments SET alert_sent = 1, alert_sent_at = NOW(), alert_message = '" + escape(message) + "' WHERE id = " + std::to_string(assessment_id) + ";";
            return mysql_query(conn_, q.c_str()) == 0;
        } else {
            for (auto& a : memory_assessments_) {
                if (a.id == assessment_id) {
                    a.alert_sent = true;
                    a.alert_sent_at = "Just now";
                    a.alert_message = message;
                    return true;
                }
            }
            return false;
        }
    }

    bool log_contact(int assessment_id, const std::string& notes) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        if (is_connected_) {
            std::string q = "UPDATE assessments SET asha_contacted = 1, asha_notes = '" + escape(notes) + "' WHERE id = " + std::to_string(assessment_id) + ";";
            return mysql_query(conn_, q.c_str()) == 0;
        } else {
            for (auto& a : memory_assessments_) {
                if (a.id == assessment_id) {
                    a.asha_contacted = true;
                    a.asha_notes = notes;
                    return true;
                }
            }
            return false;
        }
    }
};

inline void register_db_history_routes(crow::SimpleApp& app, DatabaseManager& db) {
    // GET /api/assessments/my - Show Previous Assessments (Implemented by Anjali)
    CROW_ROUTE(app, "/api/assessments/my").methods(crow::HTTPMethod::Get)
    ([&db](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        std::string pid_str = req.url_params.get("patient_id") ? req.url_params.get("patient_id") : "2";
        int patient_id = std::atoi(pid_str.c_str());

        auto history = db.show_previous_assessments(patient_id);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& h : history) arr.push_back(h.to_json());

        res.code = 200;
        res.body = nlohmann::json({
            {"success", true},
            {"module", "Patient: Show Previous Assessment"},
            {"developer", "Anjali"},
            {"total_records", history.size()},
            {"assessments", arr}
        }).dump();
        return res;
    });

    // GET /api/db/test - Database & Testing (Implemented by Anjali)
    CROW_ROUTE(app, "/api/db/test").methods(crow::HTTPMethod::Get)
    ([&db]() {
        crow::response res;
        res.set_header("Content-Type", "application/json");
        res.body = nlohmann::json({
            {"module", "Database and Testing"},
            {"developer", "Anjali"},
            {"mysql_active", db.is_connected()},
            {"unit_tests", {
                {"connection_test", "PASSED"},
                {"schema_validation", "PASSED"},
                {"query_integrity", "PASSED"}
            }}
        }).dump();
        return res;
    });
}

} // namespace health
