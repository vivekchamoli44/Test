#pragma once
/**
 * ============================================================================
 * Module: Book a Lab Test & Triage Phlebotomy Queue (DSA Module)
 * Implemented with:
 *  1. Augmented Interval Tree (AVL-balanced) for O(log N) slot conflict checks
 *  2. Binary Max-Heap Priority Queue for O(log K) clinical sample collection triage
 * ============================================================================
 * Description:
 *  - Allows patients to book diagnostic laboratory tests (Blood Sugar, HbA1c,
 *    Lipid Profile, KFT, CBC, ECG, Sputum).
 *  - DSA 1 (Interval Tree): Prevents double-booking slots in O(log N) time
 *    by verifying interval overlaps [start_mins, end_mins).
 *  - DSA 2 (Priority Queue): Automatically orders daily phlebotomy collections
 *    by clinical urgency so critical AI-risk patients are sampled first.
 * ============================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <mutex>
#include <sstream>
#include <chrono>
#include "crow_all.h"
#include "models.h"
#include "db_history.h"
#include "auth_notification.h"
#include "nlohmann/json.h"

namespace health {

// ============================================================================
// DSA 1: Augmented AVL Interval Tree for O(log N) Time-Slot Overlap Resolution
// ============================================================================
struct SlotInterval {
    int start;          // Minutes from midnight, e.g. 540 = 09:00 AM
    int end;            // e.g. 570 = 09:30 AM
    int booking_id;     // ID in database
    std::string patient_name;
    std::string test_name;

    SlotInterval(int s = 0, int e = 0, int b_id = 0, const std::string& p_name = "", const std::string& t_name = "")
        : start(s), end(e), booking_id(b_id), patient_name(p_name), test_name(t_name) {}

    // Overlap condition: max(start1, start2) < min(end1, end2)
    bool overlaps_with(const SlotInterval& other) const {
        return std::max(start, other.start) < std::min(end, other.end);
    }

    std::string format_time_label() const {
        auto format_m = [](int mins) {
            int h = mins / 60;
            int m = mins % 60;
            std::string period = (h >= 12) ? "PM" : "AM";
            int dh = (h > 12) ? (h - 12) : ((h == 0) ? 12 : h);
            char b[16];
            snprintf(b, sizeof(b), "%02d:%02d %s", dh, m, period.c_str());
            return std::string(b);
        };
        return format_m(start) + " - " + format_m(end);
    }
};

struct IntervalTreeNode {
    SlotInterval interval;
    int max_end;                // Augmented value: maximum `end` in subtree
    int height;                 // For AVL balancing
    IntervalTreeNode* left;
    IntervalTreeNode* right;

    IntervalTreeNode(const SlotInterval& iv)
        : interval(iv), max_end(iv.end), height(1), left(nullptr), right(nullptr) {}
};

class LabSlotIntervalTree {
private:
    IntervalTreeNode* root_ = nullptr;
    int count_ = 0;

    int get_h(IntervalTreeNode* n) const { return n ? n->height : 0; }
    int get_max(IntervalTreeNode* n) const { return n ? n->max_end : 0; }

    void update(IntervalTreeNode* n) {
        if (!n) return;
        n->height = 1 + std::max(get_h(n->left), get_h(n->right));
        n->max_end = std::max({n->interval.end, get_max(n->left), get_max(n->right)});
    }

    int get_balance(IntervalTreeNode* n) const {
        return n ? (get_h(n->left) - get_h(n->right)) : 0;
    }

    IntervalTreeNode* rot_right(IntervalTreeNode* y) {
        IntervalTreeNode* x = y->left;
        IntervalTreeNode* T2 = x->right;
        x->right = y;
        y->left = T2;
        update(y);
        update(x);
        return x;
    }

    IntervalTreeNode* rot_left(IntervalTreeNode* x) {
        IntervalTreeNode* y = x->right;
        IntervalTreeNode* T2 = y->left;
        y->left = x;
        x->right = T2;
        update(x);
        update(y);
        return y;
    }

    IntervalTreeNode* insert_rec(IntervalTreeNode* node, const SlotInterval& iv) {
        if (!node) {
            count_++;
            return new IntervalTreeNode(iv);
        }

        if (iv.start < node->interval.start) {
            node->left = insert_rec(node->left, iv);
        } else {
            node->right = insert_rec(node->right, iv);
        }

        update(node);

        int balance = get_balance(node);
        // LL
        if (balance > 1 && iv.start < node->left->interval.start)
            return rot_right(node);
        // RR
        if (balance < -1 && iv.start >= node->right->interval.start)
            return rot_left(node);
        // LR
        if (balance > 1 && iv.start >= node->left->interval.start) {
            node->left = rot_left(node->left);
            return rot_right(node);
        }
        // RL
        if (balance < -1 && iv.start < node->right->interval.start) {
            node->right = rot_right(node->right);
            return rot_left(node);
        }

        return node;
    }

    IntervalTreeNode* search_overlap(IntervalTreeNode* node, const SlotInterval& target) const {
        if (!node) return nullptr;

        if (node->interval.overlaps_with(target)) {
            return node;
        }

        // Pruning heuristic: if left child exists and left->max_end > target.start,
        // an overlap could exist in left subtree
        if (node->left && node->left->max_end > target.start) {
            IntervalTreeNode* res = search_overlap(node->left, target);
            if (res) return res;
        }

        return search_overlap(node->right, target);
    }

    void free_all(IntervalTreeNode* node) {
        if (!node) return;
        free_all(node->left);
        free_all(node->right);
        delete node;
    }

public:
    LabSlotIntervalTree() : root_(nullptr), count_(0) {}
    ~LabSlotIntervalTree() { free_all(root_); }

    LabSlotIntervalTree(const LabSlotIntervalTree&) = delete;
    LabSlotIntervalTree& operator=(const LabSlotIntervalTree&) = delete;

    void clear() {
        free_all(root_);
        root_ = nullptr;
        count_ = 0;
    }

    int size() const { return count_; }

    // O(log N) check: returns true if requested interval conflicts with an existing booking
    bool has_conflict(int start_m, int end_m, SlotInterval& out_conflicting) const {
        SlotInterval req(start_m, end_m);
        IntervalTreeNode* hit = search_overlap(root_, req);
        if (hit) {
            out_conflicting = hit->interval;
            return true;
        }
        return false;
    }

    // O(log N) insert
    bool book_slot(int start_m, int end_m, int booking_id, const std::string& p_name, const std::string& t_name, std::string& err_msg) {
        SlotInterval conflict;
        if (has_conflict(start_m, end_m, conflict)) {
            err_msg = "Slot conflict! Already booked from " + conflict.format_time_label() +
                      " (" + conflict.test_name + ") for " + conflict.patient_name;
            return false;
        }
        root_ = insert_rec(root_, SlotInterval(start_m, end_m, booking_id, p_name, t_name));
        return true;
    }
};

// ============================================================================
// DSA 2: Binary Max-Heap Priority Queue for Triage-Aware Lab Sample Collection
// ============================================================================
class LabTriageMaxHeap {
private:
    std::vector<LabBooking> data_;

    static int parent(int i) { return (i - 1) / 2; }
    static int left(int i) { return 2 * i + 1; }
    static int right(int i) { return 2 * i + 2; }

    void sift_up(int i) {
        while (i > 0 && data_[parent(i)].priority_score < data_[i].priority_score) {
            std::swap(data_[parent(i)], data_[i]);
            i = parent(i);
        }
    }

    void sift_down(int i) {
        int n = static_cast<int>(data_.size());
        while (true) {
            int l = left(i), r = right(i), largest = i;
            if (l < n && data_[l].priority_score > data_[largest].priority_score) largest = l;
            if (r < n && data_[r].priority_score > data_[largest].priority_score) largest = r;
            if (largest == i) break;
            std::swap(data_[i], data_[largest]);
            i = largest;
        }
    }

public:
    bool empty() const { return data_.empty(); }
    int size() const { return static_cast<int>(data_.size()); }

    void insert(const LabBooking& b) {
        data_.push_back(b);
        sift_up(static_cast<int>(data_.size()) - 1);
    }

    LabBooking extract_max() {
        if (data_.empty()) return LabBooking();
        LabBooking top = data_.front();
        data_.front() = data_.back();
        data_.pop_back();
        if (!data_.empty()) sift_down(0);
        return top;
    }

    // Drains the heap into a prioritized list (highest clinical priority first)
    std::vector<LabBooking> drain_sorted() {
        std::vector<LabBooking> out;
        out.reserve(data_.size());
        while (!empty()) {
            out.push_back(extract_max());
        }
        return out;
    }
};

// ============================================================================
// Lab Test Booking Manager & Database Coordinator
// ============================================================================
class LabBookingManager {
private:
    std::mutex lab_mutex_;
    std::vector<LabCatalogItem> catalog_;
    std::vector<LabBooking> memory_bookings_;
    int next_booking_id_ = 1;

    // Separate Interval Tree per date to isolate daily slots
    std::unordered_map<std::string, std::unique_ptr<LabSlotIntervalTree>> date_slot_trees_;

    void seed_catalog() {
        catalog_ = {
            {"FBG_HBA1C", "HbA1c & Fasting Blood Sugar", "Diabetes", "Blood", true, "10-12 hours overnight fasting mandatory.", "Same Day", 150},
            {"LIPID_PANEL", "Complete Lipid Profile", "Cardiovascular", "Blood", true, "12 hours fasting required. Avoid fatty meals night before.", "Same Day", 200},
            {"CBC_HEMO", "Complete Blood Count (CBC) & Hemoglobin", "Hematology", "Blood", false, "No fasting required. Routine blood draw.", "Same Day", 100},
            {"KFT_CREAT", "Kidney Function Test (Creatinine & Urea)", "Renal", "Blood", false, "Drink plenty of water before test.", "24 Hours", 180},
            {"LFT_PANEL", "Liver Function Test (Bilirubin, SGOT, SGPT)", "Hepatic", "Blood", true, "8-10 hours fasting recommended.", "24 Hours", 180},
            {"TB_SPUTUM", "Sputum Smear / CBNAAT (Tuberculosis)", "Respiratory", "Sputum", false, "Early morning deep-cough sample before mouthwash.", "24 Hours", 0},
            {"ECG_12LEAD", "12-Lead Resting Electrocardiogram (ECG)", "Cardiovascular", "Diagnostic", false, "Non-invasive cardiac rhythm screening.", "Instant", 120},
            {"THYROID_TSH", "Thyroid Profile (TSH, Total T3, T4)", "Endocrine", "Blood", true, "Early morning fasting sample advised.", "48 Hours", 220}
        };
    }

    void seed_demo_bookings() {
        // Sample confirmed booking for demo patient 2 (Ramesh Kumar)
        LabBooking b1;
        b1.id = next_booking_id_++;
        b1.patient_id = 2;
        b1.patient_name = "Ramesh Kumar";
        b1.phone = "+91 91234 56789";
        b1.village_area = "Rampur Ward 4";
        b1.test_name = "HbA1c & Fasting Blood Sugar";
        b1.test_code = "FBG_HBA1C";
        b1.sample_type = "Blood";
        b1.fasting_required = true;
        b1.booking_date = "2026-11-22";
        b1.slot_start = 540; // 09:00 AM
        b1.slot_end = 570;   // 09:30 AM
        b1.slot_label = "09:00 AM - 09:30 AM";
        b1.risk_score = 75;
        b1.priority_score = 85;
        b1.status = "Confirmed";
        b1.created_at = "Today";

        memory_bookings_.push_back(b1);
        get_or_create_tree("2026-11-22")->book_slot(540, 570, b1.id, b1.patient_name, b1.test_name, b1.status);
    }

public:
    LabBookingManager() {
        seed_catalog();
    }

    void clear_all_bookings() {
        std::lock_guard<std::mutex> lock(lab_mutex_);
        memory_bookings_.clear();
        date_slot_trees_.clear();
        next_booking_id_ = 1;
    }

    LabSlotIntervalTree* get_or_create_tree(const std::string& date) {
        if (date_slot_trees_.find(date) == date_slot_trees_.end()) {
            date_slot_trees_[date] = std::make_unique<LabSlotIntervalTree>();
        }
        return date_slot_trees_[date].get();
    }

    const std::vector<LabCatalogItem>& get_catalog() const {
        return catalog_;
    }

    bool get_catalog_item(const std::string& code, LabCatalogItem& out_item) const {
        for (const auto& c : catalog_) {
            if (c.code == code) {
                out_item = c;
                return true;
            }
        }
        return false;
    }

    // Generates 30-min standard slots and uses Interval Tree O(log N) to determine availability
    nlohmann::json get_available_slots(const std::string& date) {
        std::lock_guard<std::mutex> lock(lab_mutex_);
        LabSlotIntervalTree* tree = get_or_create_tree(date);

        // Standard lab collection operating hours: 08:00 AM to 03:30 PM
        const std::vector<std::pair<int, int>> standard_slots = {
            {480, 510}, // 08:00 - 08:30 AM
            {510, 540}, // 08:30 - 09:00 AM
            {540, 570}, // 09:00 - 09:30 AM
            {570, 600}, // 09:30 - 10:00 AM
            {600, 630}, // 10:00 - 10:30 AM
            {630, 660}, // 10:30 - 11:00 AM
            {660, 690}, // 11:00 - 11:30 AM
            {690, 720}, // 11:30 - 12:00 PM
            {780, 810}, // 01:00 - 01:30 PM
            {810, 840}, // 01:30 - 02:00 PM
            {840, 870}, // 02:00 - 02:30 PM
            {870, 900}  // 02:30 - 03:00 PM
        };

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& slot : standard_slots) {
            SlotInterval conflict;
            bool conflict_detected = tree->has_conflict(slot.first, slot.second, conflict);
            SlotInterval display_iv(slot.first, slot.second);

            arr.push_back({
                {"start_mins", slot.first},
                {"end_mins", slot.second},
                {"label", display_iv.format_time_label()},
                {"is_available", !conflict_detected},
                {"conflict_with", conflict_detected ? conflict.patient_name : ""}
            });
        }
        return arr;
    }

    // Books a lab slot: checks Interval Tree in O(log N) first
    bool book_lab_test(const LabBooking& req, LabBooking& out_confirmed, std::string& err_msg) {
        std::lock_guard<std::mutex> lock(lab_mutex_);
        LabSlotIntervalTree* tree = get_or_create_tree(req.booking_date);

        // 1. Check & Book in Interval Tree in O(log N)
        std::string tree_err;
        int new_id = next_booking_id_++;
        if (!tree->book_slot(req.slot_start, req.slot_end, new_id, req.patient_name, req.test_name, tree_err)) {
            err_msg = tree_err;
            return false;
        }

        // 2. Compute Priority Score based on Clinical Risk Score
        // Priority = (Risk Score * 0.6) + (Fasting bonus * 15) + (Urgency factor)
        int priority = static_cast<int>(req.risk_score * 0.6);
        if (req.fasting_required) priority += 15;
        if (req.risk_score >= 80) priority += 25; // Critical emergency triage boost
        if (priority > 100) priority = 100;
        if (priority < 10) priority = 10;

        out_confirmed = req;
        out_confirmed.id = new_id;
        out_confirmed.priority_score = priority;
        out_confirmed.status = "Confirmed";
        out_confirmed.created_at = "Today";

        SlotInterval iv(out_confirmed.slot_start, out_confirmed.slot_end);
        out_confirmed.slot_label = iv.format_time_label();

        memory_bookings_.push_back(out_confirmed);
        return true;
    }

    // Get bookings for a specific patient
    std::vector<LabBooking> get_patient_bookings(int patient_id) {
        std::lock_guard<std::mutex> lock(lab_mutex_);
        std::vector<LabBooking> res;
        for (const auto& b : memory_bookings_) {
            if (b.patient_id == patient_id) {
                res.push_back(b);
            }
        }
        std::reverse(res.begin(), res.end());
        return res;
    }

    // DSA 2: Priority Queue Triage for ASHA Field Phlebotomy
    std::vector<LabBooking> get_triage_queue_for_worker(const std::string& village_area) {
        std::lock_guard<std::mutex> lock(lab_mutex_);
        LabTriageMaxHeap heap;

        for (const auto& b : memory_bookings_) {
            // Push active bookings in worker's area into the Max-Heap
            if (b.status != "Completed" && (village_area.empty() || in_worker_area_loose(village_area, b.village_area))) {
                heap.insert(b);
            }
        }

        // Drains into strictly prioritized collection order (Highest urgency first)
        return heap.drain_sorted();
    }

    bool update_status(int booking_id, const std::string& new_status) {
        std::lock_guard<std::mutex> lock(lab_mutex_);
        for (auto& b : memory_bookings_) {
            if (b.id == booking_id) {
                b.status = new_status;
                return true;
            }
        }
        return false;
    }

private:
    static bool in_worker_area_loose(const std::string& worker_area, const std::string& item_area) {
        if (worker_area.empty() || item_area.empty()) return true;
        std::string w = worker_area, it = item_area;
        std::transform(w.begin(), w.end(), w.begin(), ::tolower);
        std::transform(it.begin(), it.end(), it.begin(), ::tolower);
        return (w == it || it.find(w) != std::string::npos || w.find(it) != std::string::npos);
    }
};

// Singleton manager instance
inline LabBookingManager& get_lab_booking_manager() {
    static LabBookingManager manager;
    return manager;
}

// ============================================================================
// Crow Route Registration for Lab Test Booking Module
// ============================================================================
inline void register_lab_test_booking_routes(crow::SimpleApp& app, DatabaseManager& db, AuthSessionManager& auth) {

    // 1. GET /api/lab/catalog - Available Diagnostic Tests
    CROW_ROUTE(app, "/api/lab/catalog").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        auto& mgr = get_lab_booking_manager();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& c : mgr.get_catalog()) {
            arr.push_back(c.to_json());
        }

        res.code = 200;
        res.body = nlohmann::json({
            {"success", true},
            {"total_tests", arr.size()},
            {"catalog", arr}
        }).dump();
        return res;
    });

    // 2. GET /api/lab/available-slots?date=YYYY-MM-DD
    // Checks Interval Tree in O(log N) for each slot
    CROW_ROUTE(app, "/api/lab/available-slots").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        std::string date = req.url_params.get("date") ? req.url_params.get("date") : "";
        if (date.empty()) {
            // Default to tomorrow or current date
            date = "2026-11-22";
        }

        auto& mgr = get_lab_booking_manager();
        nlohmann::json slots = mgr.get_available_slots(date);

        res.code = 200;
        res.body = nlohmann::json({
            {"success", true},
            {"date", date},
            {"algorithm", "Augmented Interval Tree (AVL-balanced, O(log N))"},
            {"slots", slots}
        }).dump();
        return res;
    });

    // 3. POST /api/lab/book - Book a Lab Test with Interval Tree Conflict Prevention
    CROW_ROUTE(app, "/api/lab/book").methods(crow::HTTPMethod::Post)
    ([&db](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        try {
            auto body = nlohmann::json::parse(req.body);

            std::string test_code = body.value("test_code", "");
            std::string date = body.value("booking_date", "");
            int start_mins = body.value("slot_start", 540);
            int end_mins = body.value("slot_end", 570);
            int patient_id = body.value("patient_id", 2);
            std::string patient_name = body.value("patient_name", "Patient");
            std::string phone = body.value("phone", "");
            std::string village_area = body.value("village_area", "Rampur Ward 4");

            if (test_code.empty() || date.empty()) {
                res.code = 400;
                res.body = nlohmann::json({{"error", "test_code and booking_date are required."}}).dump();
                return res;
            }

            auto& mgr = get_lab_booking_manager();
            LabCatalogItem cat_item;
            if (!mgr.get_catalog_item(test_code, cat_item)) {
                res.code = 404;
                res.body = nlohmann::json({{"error", "Selected lab test not found in catalog."}}).dump();
                return res;
            }

            // Fetch patient's most recent risk score to compute priority
            int patient_risk = 50;
            auto history = db.show_previous_assessments(patient_id);
            if (!history.empty()) {
                patient_risk = history.front().risk_score;
                if (!history.front().patient_name.empty()) patient_name = history.front().patient_name;
                if (!history.front().phone.empty()) phone = history.front().phone;
                if (!history.front().village_area.empty()) village_area = history.front().village_area;
            }

            LabBooking booking_req;
            booking_req.patient_id = patient_id;
            booking_req.patient_name = patient_name;
            booking_req.phone = phone;
            booking_req.village_area = village_area;
            booking_req.test_name = cat_item.name;
            booking_req.test_code = cat_item.code;
            booking_req.sample_type = cat_item.sample_type;
            booking_req.fasting_required = cat_item.fasting_required;
            booking_req.booking_date = date;
            booking_req.slot_start = start_mins;
            booking_req.slot_end = end_mins;
            booking_req.risk_score = patient_risk;

            LabBooking confirmed;
            std::string err_msg;
            if (!mgr.book_lab_test(booking_req, confirmed, err_msg)) {
                res.code = 409; // Conflict
                res.body = nlohmann::json({
                    {"success", false},
                    {"error", err_msg},
                    {"algorithm", "Interval Tree Overlap Conflict Detection"}
                }).dump();
                return res;
            }

            res.code = 201;
            res.body = nlohmann::json({
                {"success", true},
                {"message", "Lab test appointment booked successfully!"},
                {"algorithm", "Interval Tree (O(log N) slot validation) + Max-Heap Triage"},
                {"booking", confirmed.to_json()}
            }).dump();
            return res;

        } catch (const std::exception& e) {
            res.code = 400;
            res.body = nlohmann::json({{"error", std::string("Invalid request: ") + e.what()}}).dump();
            return res;
        }
    });

    // 4. GET /api/lab/my-bookings?patient_id=X
    CROW_ROUTE(app, "/api/lab/my-bookings").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        int pid = req.url_params.get("patient_id") ? std::atoi(req.url_params.get("patient_id")) : 2;
        auto& mgr = get_lab_booking_manager();
        auto bookings = mgr.get_patient_bookings(pid);

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& b : bookings) {
            arr.push_back(b.to_json());
        }

        res.code = 200;
        res.body = nlohmann::json({
            {"success", true},
            {"patient_id", pid},
            {"total_bookings", arr.size()},
            {"bookings", arr}
        }).dump();
        return res;
    });

    // 5. GET /api/worker/lab/triage-queue - ASHA Worker / Phlebotomist Priority Queue
    // Orders samples strictly by Priority Queue (Max-Heap)
    CROW_ROUTE(app, "/api/worker/lab/triage-queue").methods(crow::HTTPMethod::Get)
    ([&auth](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        User worker;
        if (!require_asha_worker(req, auth, worker, res)) return res;

        auto& mgr = get_lab_booking_manager();
        auto queue = mgr.get_triage_queue_for_worker(worker.village_area);

        nlohmann::json arr = nlohmann::json::array();
        int critical_count = 0;
        int fasting_count = 0;

        for (const auto& b : queue) {
            arr.push_back(b.to_json());
            if (b.priority_score >= 80) critical_count++;
            if (b.fasting_required) fasting_count++;
        }

        res.code = 200;
        res.body = nlohmann::json({
            {"success", true},
            {"module", "ASHA Phlebotomy Triage Queue (DSA: Max-Heap Priority Queue)"},
            {"village_area", worker.village_area},
            {"total_pending_samples", queue.size()},
            {"critical_priority_count", critical_count},
            {"fasting_samples_count", fasting_count},
            {"queue", arr}
        }).dump();
        return res;
    });

    // 6. POST /api/worker/lab/<int>/status - Update Collection Status
    CROW_ROUTE(app, "/api/worker/lab/<int>/status").methods(crow::HTTPMethod::Post)
    ([&auth](const crow::request& req, int booking_id) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        User worker;
        if (!require_asha_worker(req, auth, worker, res)) return res;

        std::string new_status = "Sample Collected";
        try {
            if (!req.body.empty()) {
                auto b = nlohmann::json::parse(req.body);
                new_status = b.value("status", "Sample Collected");
            }
        } catch (...) {}

        auto& mgr = get_lab_booking_manager();
        if (mgr.update_status(booking_id, new_status)) {
            res.code = 200;
            res.body = nlohmann::json({
                {"success", true},
                {"booking_id", booking_id},
                {"new_status", new_status},
                {"message", "Sample collection status updated successfully."}
            }).dump();
        } else {
            res.code = 404;
            res.body = nlohmann::json({{"error", "Lab booking record not found."}}).dump();
        }
        return res;
    });

    // 7. POST /api/lab/clear-all - Clear all lab appointments & Interval Tree slots
    CROW_ROUTE(app, "/api/lab/clear-all").methods(crow::HTTPMethod::Post, crow::HTTPMethod::Get)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        auto& mgr = get_lab_booking_manager();
        mgr.clear_all_bookings();

        res.code = 200;
        res.body = nlohmann::json({
            {"success", true},
            {"message", "All lab appointments and interval tree slots have been cleared successfully."}
        }).dump();
        return res;
    });
}

} // namespace health
