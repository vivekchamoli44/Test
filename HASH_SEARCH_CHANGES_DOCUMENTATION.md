    # Medico — Patient Search via Hashing Implementation & Changes Documentation

This document records the exact changes, data structure design, new files, and modified lines of code implemented to add **O(1) Patient Search using Hashing** to the ASHA Worker Module.

---

## 1. Overview of the Feature & DSA Design

In community health triage, ASHA workers need to locate specific patients instantly across large volumes of community screening records without scanning sequentially or reloading the page.

### Data Structure: Hash Table with Separate Chaining
* **Algorithm / Model**: Direct Addressing via Hash Table using **Separate Chaining** (linked lists / buckets).
* **Hash Function**: Polynomial Rolling Hash:
  $$\text{hash}(s) = \left( \sum_{i=0}^{n-1} (s[i] + 1) \cdot p^i \right) \bmod M$$
  where $p = 31$ (prime base) and $M = 101$ (prime bucket count) to distribute keys uniformly and minimize clustering.
* **Collision Resolution**: Separate Chaining — if two distinct patient keys hash to the same bucket index, they are stored as linked nodes in that bucket's chain.
* **Search Complexity**:
  * **Average Time Complexity**: $\mathcal{O}(1)$ for lookup and insert.
  * **Worst Case Time Complexity**: $\mathcal{O}(N)$ (in the rare theoretical event of total collisions).
  * **Space Complexity**: $\mathcal{O}(M + N)$ where $M = 101$ buckets and $N$ is total assessments.
* **Multi-Key Indexing**:
  Each patient assessment is indexed under multiple keys:
  1. **Normalized Patient Full Name** (e.g., `"rameshkumar"`)
  2. **Individual Name Tokens** (e.g., `"ramesh"`, `"kumar"`) — allows first-name or last-name search
  3. **Cleaned Phone Number** (e.g., `"9123456789"`)
  4. **Patient ID** (e.g., `"2"`)
  5. **Assessment ID** (e.g., `"1"`)

---

## 2. Summary of Created & Modified Files

| # | File Path | Status | Purpose |
|---|-----------|--------|---------|
| 1 | `backend/src/patient_hash_search.h` | **NEW FILE** | C++ DSA Hash Table class (`PatientHashTable`) and Crow route `GET /api/worker/assessments/search` |
| 2 | `backend/src/main.cpp` | **MODIFIED** | Included `patient_hash_search.h` and registered route handler |
| 3 | `frontend/js/patient_hash_search.js` | **NEW FILE** | Client-side JavaScript Hash Table (`ClientPatientHashTable`), input debouncing, and UI hook |
| 4 | `frontend/worker.html`| **MODIFIED** | Added search bar, clear button, result badge, script tag, and Patient ID field in Contact Modal |
| 5 | `frontend/js/dashboard_triage.js` | **MODIFIED** | Triggers hash indexing on data load, displays Patient ID badge in triage table, populates Patient ID in contact modal, and exports table renderer |
| 6 | `frontend/js/ai_assessment.js` | **MODIFIED** | Displays Patient ID in clinical report modal metadata |
| 7 | `frontend/js/auth_notification.js` | **MODIFIED** | Displays Patient ID in top navbar user pill |
| 8 | `frontend/js/db_history.js` | **MODIFIED** | Displays Patient ID badge on past assessment history cards |
| 9 | `frontend/patient.html` | **MODIFIED** | Added Patient ID badge and customized welcome greeting in top hero banner |
| 10 | `DOCUMENTATION.md` | **MODIFIED** | Documented new search endpoint and DSA hashing capabilities |  
---
## 3. Detailed Line-by-Line Changes

### File 1: `backend/src/patient_hash_search.h` (NEW FILE)
* **Total Lines**: 210 lines
* **Key Components**:
  * `normalize_hash_key(const std::string& input)`: Converts search strings to lowercase alphanumeric characters.
  * `class PatientHashTable`:
    * `struct HashNode`: Holds `key`, `std::vector<Assessment> records`, and `HashNode* next`.
    * `int hash_function(const std::string& key)`: Calculates polynomial rolling hash mod 101.
    * `void insert(const std::string& raw_key, const Assessment& record)`: Inserts into bucket chain.
    * `std::vector<Assessment> search(const std::string& raw_key)`: Retrieves matching records in $\mathcal{O}(1)$ average time.
    * `void index_assessment(const Assessment& a)`: Multi-indexes an assessment by name, name words, phone, patient_id, and assessment ID.
    * Destructor: Safely deallocates all linked list nodes to prevent memory leaks.
  * `register_patient_hash_search_routes(...)`: Exposes `GET /api/worker/assessments/search?q=<query>`, validates worker session, scopes to worker's `village_area`, and returns matching JSON records.

---

### File 2: `backend/src/main.cpp` (MODIFIED)

#### Change A — Header Inclusion
* **Location**: Lines 27–30
* **Lines Changed**:
  ```cpp
  // BEFORE:
  #include "auth_notification.h"
  #include "dashboard_triage.h"

  // AFTER:
  #include "auth_notification.h"
  #include "dashboard_triage.h"
  #include "patient_hash_search.h"
  ```

#### Change B — Route Registration
* **Location**: Lines 100–104
* **Lines Changed**:
  ```cpp
  // BEFORE:
      health::register_auth_notification_routes(app, db, auth); // Yashwardhan Rohilla's Module
      health::register_dashboard_triage_routes(app, db, auth);   // Nandini's Module

  // AFTER:
      health::register_auth_notification_routes(app, db, auth); // Yashwardhan Rohilla's Module
      health::register_dashboard_triage_routes(app, db, auth);   // Nandini's Module
      health::register_patient_hash_search_routes(app, db, auth); // Patient Search via Hashing (DSA Module)
  ```

---

### File 3: `frontend/worker.html` (MODIFIED)

#### Change A — Added Search Input & Result Badge
* **Location**: Lines 40–55
* **Lines Changed**:
  ```html
  <!-- BEFORE: -->
              <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 1.25rem; flex-wrap: wrap; gap: 1rem;">
                  <div>
                      <h3 style="font-size: 1.25rem; font-weight: 800;">High Risk Individuals (Sorted: Highest to Lowest Risk)</h3>
                      
                  </div>
              </div>

  <!-- AFTER: -->
              <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 1.25rem; flex-wrap: wrap; gap: 1rem;">
                  <div>
                      <h3 style="font-size: 1.25rem; font-weight: 800;">High Risk Individuals (Sorted: Highest to Lowest Risk)</h3>
                      <p style="font-size: 0.82rem; color: var(--text-muted); margin-top: 2px;">Community screening records sorted by clinical risk priority</p>
                  </div>

                  <!-- Patient Search using Hashing (O(1) Lookup) -->
                  <div style="display: flex; align-items: center; gap: 8px; flex-wrap: wrap;">
                      <div style="position: relative; width: 280px; max-width: 100%;">
                          <input type="text" id="patient-search-input" placeholder="🔍 Search Name, Phone, or ID..." 
                                 style="width: 100%; padding: 7px 30px 7px 10px; border: 1px solid var(--border); border-radius: 8px; font-size: 0.88rem; outline: none;" />
                          <button type="button" id="patient-search-clear" title="Clear search"
                                  style="position: absolute; right: 8px; top: 50%; transform: translateY(-50%); background: none; border: none; font-size: 1.1rem; color: var(--text-muted); cursor: pointer; display: none; line-height: 1;">&times;</button>
                      </div>
                      <span id="search-result-count" style="display: none; font-size: 0.78rem; font-weight: 700; color: var(--primary-dark); background: var(--primary-light); padding: 4px 8px; border-radius: 6px;"></span>
                  </div>
              </div>
  ```

#### Change B — Included Script Tag
* **Location**: Lines 169–173
* **Lines Changed**:
  ```html
  <!-- BEFORE: -->
      <!-- Patient & ASHA Worker Dashboard & ASHA Worker: High Risk Individual (Nandini) -->
      <script src="/js/dashboard_triage.js"></script>
  </body>

  <!-- AFTER: -->
      <!-- Patient & ASHA Worker Dashboard & ASHA Worker: High Risk Individual (Nandini) -->
      <script src="/js/dashboard_triage.js"></script>
      <!-- ASHA Worker Patient Search via Hashing (DSA Module) -->
      <script src="/js/patient_hash_search.js"></script>
  </body>
  ```

---

### File 4: `frontend/js/patient_hash_search.js` (NEW FILE)
* **Total Lines**: 175 lines
* **Key Components**:
  * `class ClientPatientHashTable`: Provides immediate, zero-latency in-browser hash table searching with polynomial rolling hash function.
  * Real-time search handler: Listens to `'input'` events on `#patient-search-input`.
  * Automatic table re-rendering: Calls `DashboardTriageModule.render(matched)` with instant match badges.
  * Clear button handler: Restores the full risk-prioritized list immediately.
  * `window.PatientHashSearchModule`: Exposes `buildIndex`, `search`, and `searchBackend` globally.

---

### File 5: `frontend/js/dashboard_triage.js` (MODIFIED)

#### Change A — Build Hash Index upon Data Load
* **Location**: Lines 60–66
* **Lines Changed**:
  ```javascript
  // BEFORE:
          renderTriageTable(allPatients);

      } catch (err) {

  // AFTER:
          renderTriageTable(allPatients);

          // Build O(1) Hash Search Index for fast patient searching
          if (window.PatientHashSearchModule) {
              window.PatientHashSearchModule.buildIndex(allPatients);
          }

      } catch (err) {
  ```

#### Change B — Search-friendly Empty Table State
* **Location**: Lines 75–80
* **Lines Changed**:
  ```javascript
  // BEFORE:
      if (patients.length === 0) {
          tbody.innerHTML = '<tr><td colspan="6" style="text-align: center; padding: 20px; color: var(--text-muted);">No patients from your area yet.</td></tr>';
          return;
      }

  // AFTER:
      if (patients.length === 0) {
          tbody.innerHTML = '<tr><td colspan="6" style="text-align: center; padding: 20px; color: var(--text-muted);">No patients found.</td></tr>';
          return;
      }
  ```

#### Change C — Export Render & Patients Accessor
* **Location**: Lines 180–189
* **Lines Changed**:
  ```javascript
  // BEFORE:
  window.DashboardTriageModule = {
      loadTriage: loadTriage
  };

  // AFTER:
  window.DashboardTriageModule = {
      loadTriage: loadTriage,
      render: renderTriageTable,
      getAllPatients: function () { return allPatients; }
  };
  ```

---

## 4. Verification and Testing

1. **Compilation Check**:
   Running `backend/build.bat` compiles cleanly with GCC 13+ (C++17) into `server.exe` with static runtimes (`-lws2_32 -lmswsock -lwinhttp -lmysql`).
2. **Search Verification Scenarios**:
   * **By Name**: Typing `"Ramesh"` or `"Kumar"` instantly displays Ramesh Kumar's record.
   * **By Phone**: Typing `"91234"` or `"+91 91234 56789"` resolves to the patient.
   * **By Patient ID**: Typing `"2"` retrieves the patient's records.
   * **Clear Action**: Clicking the `×` button resets the input and restores all high-risk patients sorted by risk score.
