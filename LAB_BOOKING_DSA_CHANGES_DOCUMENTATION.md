# Medico — Diagnostic Lab Test Booking Module (Interval Tree + Priority Queue) Documentation

This document records the exact changes, data structure designs, new files, and modified lines of code implemented to build the **Diagnostic Lab Test Booking & Phlebotomy Triage Module** in Medico.

---

## 1. Overview & Data Structures Used

This module implements two advanced computer science data structures to solve two distinct real-world healthcare challenges:

### Data Structure 1: Augmented AVL Interval Tree (Time-Slot Scheduling & Conflict Prevention)
* **Problem:** In diagnostic lab collection, a phlebotomist or testing booth can only service one patient at a time. Two patients must never book overlapping time slots (e.g. `09:00 - 09:30 AM` vs `09:15 - 09:45 AM`).
* **Implementation:** `LabSlotIntervalTree`
  * Each node stores a booked time interval $[start, end)$ in minutes from midnight (e.g., $540 \to 570$).
  * **Augmentation:** Each node stores `max_end`, the maximum upper endpoint in its entire subtree.
  * **AVL Balancing:** Maintained via left and right tree rotations (LL, RR, LR, RL) so that tree height $h \le 1.44 \log_2 N$.
  * **Overlap Test:** $\max(start_1, start_2) < \min(end_1, end_2)$.
  * **Complexity:**
    * Conflict Check: $\mathcal{O}(\log N)$
    * Slot Insertion: $\mathcal{O}(\log N)$
    * Space Complexity: $\mathcal{O}(N)$

### Data Structure 2: Binary Max-Heap Priority Queue (Triage-Aware Phlebotomy Queue)
* **Problem:** Community lab sample collectors have finite daily kits. A critical patient identified by Medico's AI (e.g. suspected acute cardiac ischemia, severe diabetes) should not wait behind routine annual checkups.
* **Implementation:** `LabTriageMaxHeap`
  * Elements: Confirmed `LabBooking` records.
  * **Priority Formula:**
    $$\text{Priority Score} = (\text{AI Risk Score} \times 0.6) + (\text{Fasting Bonus} \times 15) + (\text{Emergency Critical Bonus} \times 25)$$
  * Operations:
    * `insert(booking)` with sift-up: $\mathcal{O}(\log K)$
    * `extract_max()` with sift-down: $\mathcal{O}(\log K)$
    * `drain_sorted()`: Returns samples ordered strictly from highest clinical priority to lowest in $\mathcal{O}(K \log K)$ time.

---

## 2. Summary of Created & Modified Files

| # | File Path | Status | Purpose |
|---|-----------|--------|---------|
| 1 | `backend/src/lab_test_booking.h` | **NEW FILE** | Contains Augmented AVL Interval Tree, Binary Max-Heap Priority Queue, Lab catalog, booking manager, and Crow REST routes |
| 2 | `backend/src/models.h` | **MODIFIED** | Added `LabCatalogItem` and `LabBooking` data structs with JSON serialization |
| 3 | `backend/src/main.cpp` | **MODIFIED** | Included `lab_test_booking.h` and registered routes |
| 4 | `frontend/js/lab_booking.js` | **NEW FILE** | Frontend client for catalog, slot availability rendering, patient booking form, and ASHA triage queue |
| 5 | `frontend/patient.html` | **MODIFIED** | Added Book Diagnostic Lab Test section, slot grid, and My Lab Bookings table |
| 6 | `frontend/worker.html` | **MODIFIED** | Added Phlebotomy & Lab Sample Collection Queue section with KPI indicators |
| 7 | `frontend/css/style.css` | **MODIFIED** | Added styles for `.slots-grid`, `.slot-btn`, `.slot-booked`, and `.slot-btn.selected` |
| 8 | `DOCUMENTATION.md` | **MODIFIED** | Updated API endpoint table with `/api/lab/*` routes |

---

## 3. Detailed Line-by-Line Changes

### File 1: `backend/src/lab_test_booking.h` (NEW FILE)
* **Total Lines**: 485 lines
* **Key Components**:
  * `struct SlotInterval`: Encapsulates $[start, end)$ minutes from midnight and overlap condition logic.
  * `struct IntervalTreeNode`: Node storing `interval`, `max_end`, `height`, and child pointers.
  * `class LabSlotIntervalTree`: AVL-balanced Interval Tree providing `has_conflict()` and `book_slot()` in $\mathcal{O}(\log N)$.
  * `class LabTriageMaxHeap`: Array-based binary Max-Heap providing `insert()`, `extract_max()`, and `drain_sorted()`.
  * `class LabBookingManager`: In-memory and date-partitioned tree manager.
  * REST Endpoints:
    * `GET /api/lab/catalog`: Returns diagnostic test catalog with fasting guidelines.
    * `GET /api/lab/available-slots?date=YYYY-MM-DD`: Returns available 30-min slots calculated via the Interval Tree.
    * `POST /api/lab/book`: Enforces $\mathcal{O}(\log N)$ conflict checking, computes priority, and records booking.
    * `GET /api/lab/my-bookings?patient_id=X`: Returns patient's scheduled lab tests.
    * `GET /api/worker/lab/triage-queue`: Returns sample collection schedule prioritized by Max-Heap.
    * `POST /api/worker/lab/<id>/status`: Updates collection status (`Sample Collected`, `Completed`).

---

### File 2: `backend/src/models.h` (MODIFIED)

#### Location: Lines 147–215
* **Lines Changed**:
  ```cpp
  // Added LabCatalogItem and LabBooking structs:
  struct LabCatalogItem {
      std::string code;
      std::string name;
      std::string category;
      std::string sample_type;
      bool fasting_required = false;
      std::string preparation_note;
      std::string turnaround_time;
      int price_inr = 0;
      nlohmann::json to_json() const;
  };

  struct LabBooking {
      int id = 0;
      int patient_id = 0;
      std::string patient_name;
      std::string phone;
      std::string village_area;
      std::string test_name;
      std::string test_code;
      std::string sample_type;
      bool fasting_required = false;
      std::string booking_date;
      int slot_start = 540;
      int slot_end = 570;
      std::string slot_label;
      int risk_score = 0;
      int priority_score = 50;
      std::string status = "Confirmed";
      std::string created_at;
      nlohmann::json to_json() const;
  };
  ```

---

### File 3: `backend/src/main.cpp` (MODIFIED)

#### Change A — Header Inclusion
* **Location**: Lines 27–31
* **Lines Changed**:
  ```cpp
  // BEFORE:
  #include "auth_notification.h"
  #include "dashboard_triage.h"
  #include "patient_hash_search.h"

  // AFTER:
  #include "auth_notification.h"
  #include "dashboard_triage.h"
  #include "patient_hash_search.h"
  #include "lab_test_booking.h"
  ```

#### Change B — Route Registration
* **Location**: Lines 100–106
* **Lines Changed**:
  ```cpp
  // BEFORE:
      health::register_dashboard_triage_routes(app, db, auth);   // Nandini's Module
      health::register_patient_hash_search_routes(app, db, auth); // Patient Search via Hashing (DSA Module)

  // AFTER:
      health::register_dashboard_triage_routes(app, db, auth);   // Nandini's Module
      health::register_patient_hash_search_routes(app, db, auth); // Patient Search via Hashing (DSA Module)
      health::register_lab_test_booking_routes(app, db, auth);    // Lab Test Booking (Interval Tree + Max-Heap DSA)
  ```

---

### File 4: `frontend/js/lab_booking.js` (NEW FILE)
* **Total Lines**: 245 lines
* **Key Components**:
  * `initPatientLabBooking()`: Loads catalog, handles date picker changes, dynamically calls `/api/lab/available-slots`, renders selectable slots, and handles appointment confirmations.
  * `loadPatientBookings(patientId)`: Populates patient's booked test history.
  * `loadWorkerPhlebotomyQueue()`: Populates ASHA worker phlebotomy queue ordered by the Max-Heap priority score with one-click collection buttons.

---

### File 5: `frontend/patient.html` (MODIFIED)

#### Change A — Book Diagnostic Lab Test Section
* **Location**: Lines 99–178
* **Lines Added**:
  * Test selector dropdown `#lab-test-select`
  * Date picker `#lab-date-select`
  * Fasting tag `#lab-fasting-tag`
  * Preparation instructions box `#lab-prep-note`
  * 30-min collection slot button grid `#lab-slots-container`
  * Book appointment button `#btn-confirm-lab-book`
  * Scheduled tests table `#patient-lab-bookings-tbody`

#### Change B — Script Inclusion
* **Location**: Lines 520–524
* **Lines Added**:
  ```html
  <script src="/js/lab_booking.js"></script>
  ```

---

### File 6: `frontend/worker.html` (MODIFIED)

#### Change A — Phlebotomy Triage Queue Section
* **Location**: Lines 75–118
* **Lines Added**:
  * Phlebotomy collection queue table `#worker-lab-queue-tbody`
  * Summary KPI pills: Pending samples (`#lab-kpi-total`), Critical urgency (`#lab-kpi-critical`), Fasting needed (`#lab-kpi-fasting`).

#### Change B — Script Inclusion
* **Location**: Lines 218–222
* **Lines Added**:
  ```html
  <script src="/js/lab_booking.js"></script>
  ```

---

### File 7: `frontend/css/style.css` (MODIFIED)

#### Location: Lines 505–548
* **Lines Added**:
  ```css
  /* Lab Booking Slot Styles (Interval Tree) */
  .slots-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(130px, 1fr));
      gap: 8px;
      margin: 12px 0;
  }

  .slot-btn {
      background: #ffffff;
      border: 1.5px solid var(--border);
      border-radius: 8px;
      padding: 8px 10px;
      text-align: center;
      cursor: pointer;
      transition: all 0.2s ease;
      font-size: 13px;
      color: var(--text-main);
  }

  .slot-btn:hover:not(:disabled) {
      border-color: var(--primary);
      background: var(--primary-light);
  }

  .slot-btn.selected {
      background: var(--primary);
      border-color: var(--primary-dark);
      color: #ffffff;
      box-shadow: 0 2px 4px rgba(2, 132, 199, 0.3);
  }

  .slot-btn.slot-booked {
      background: #f1f5f9;
      border-color: #cbd5e1;
      color: #94a3b8;
      cursor: not-allowed;
      text-decoration: line-through;
  }
  ```

---

## 4. Verification and Testing

1. **Compilation Check**:
   Rebuilt backend using `backend/build.bat`. Static MinGW-w64 C++ compilation completed with return code `0`.
2. **Conflict Prevention Test**:
   * Patient 1 books `09:00 AM - 09:30 AM` for date `2026-11-22`.
   * When Patient 2 selects `2026-11-22`, the `09:00 AM - 09:30 AM` slot is greyed out (`<del>Booked</del>`).
   * Any manual POST request to `/api/lab/book` attempting to overlap $[540, 570)$ is rejected with HTTP `409 Conflict` in $\mathcal{O}(\log N)$ time.
3. **Phlebotomy Triage Ordering Test**:
   * Critical risk patient (priority score: 85) automatically sits at the top of the ASHA worker's sample queue ahead of a routine check (priority score: 45) due to the Max-Heap.
