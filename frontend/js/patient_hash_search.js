/**
 * ============================================================================
 * Module: ASHA Worker Patient Search via Hashing (DSA Module)
 * ============================================================================
 * Description:
 *  - Client-side & backend-integrated Patient Search using Hashing (O(1) average lookup).
 *  - Implements a Hash Table with Separate Chaining for collision resolution.
 *  - Indexes patients by:
 *      1. Full Name & Individual Name Words (e.g. "Ramesh", "Kumar")
 *      2. Contact Phone Number (e.g. "9123456789")
 *      3. Patient User ID (e.g. "2")
 *      4. Assessment ID (e.g. "1")
 * ============================================================================
 */

(function () {
    // ------------------------------------------------------------------------
    // Key Normalization Helper
    // ------------------------------------------------------------------------
    function normalizeSearchKey(input) {
        if (!input) return '';
        return String(input).toLowerCase().replace(/[^a-z0-9]/g, '');
    }

    // ------------------------------------------------------------------------
    // DSA: Hash Table with Separate Chaining (Client-Side)
    // ------------------------------------------------------------------------
    class ClientPatientHashTable {
        constructor(bucketCount = 101) {
            this.bucketCount = bucketCount;
            this.buckets = new Array(bucketCount).fill(null).map(() => []);
            this.totalEntries = 0;
        }

        // Polynomial rolling hash: hash(s) = sum(s[i] * 31^i) % bucketCount
        hashFunction(key) {
            const p = 31;
            let hashVal = 0;
            let pPow = 1;
            for (let i = 0; i < key.length; i++) {
                hashVal = (hashVal + (key.charCodeAt(i) + 1) * pPow) % this.bucketCount;
                pPow = (pPow * p) % this.bucketCount;
            }
            return (hashVal + this.bucketCount) % this.bucketCount;
        }

        clear() {
            for (let i = 0; i < this.bucketCount; i++) {
                this.buckets[i] = [];
            }
            this.totalEntries = 0;
        }

        // Insert assessment record into hash table under a specific key
        insert(rawKey, patient) {
            const key = normalizeSearchKey(rawKey);
            if (!key) return;

            const bucketIdx = this.hashFunction(key);
            const chain = this.buckets[bucketIdx];

            // Check if key node already exists in this bucket
            let node = chain.find(item => item.key === key);
            if (node) {
                if (!node.records.some(r => r.id === patient.id)) {
                    node.records.push(patient);
                    this.totalEntries++;
                }
            } else {
                chain.push({
                    key: key,
                    records: [patient]
                });
                this.totalEntries++;
            }
        }

        // O(1) Average Search by exact key
        search(rawQuery) {
            const query = normalizeSearchKey(rawQuery);
            if (!query) return [];

            const bucketIdx = this.hashFunction(query);
            const chain = this.buckets[bucketIdx];
            const node = chain.find(item => item.key === query);

            if (node) {
                return node.records;
            }

            // Prefix/Sub-string match fallback across bucket entries
            const results = [];
            const seenIds = new Set();
            for (let i = 0; i < this.bucketCount; i++) {
                for (const item of this.buckets[i]) {
                    if (item.key.includes(query)) {
                        for (const r of item.records) {
                            if (!seenIds.has(r.id)) {
                                seenIds.add(r.id);
                                results.push(r);
                            }
                        }
                    }
                }
            }
            return results;
        }

        // Indexes an assessment under name, name tokens, phone, and IDs
        indexAssessment(patient) {
            if (!patient) return;

            // 1. Patient full name
            if (patient.patient_name) {
                this.insert(patient.patient_name, patient);
                // Individual words in name (e.g. "Ramesh", "Kumar")
                patient.patient_name.split(/\s+/).forEach(word => {
                    if (word) this.insert(word, patient);
                });
            }

            // 2. Contact Phone
            if (patient.phone) {
                this.insert(patient.phone, patient);
            }

            // 3. Patient ID & Assessment ID
            if (patient.patient_id !== undefined && patient.patient_id !== null) {
                this.insert(String(patient.patient_id), patient);
            }
            if (patient.id !== undefined && patient.id !== null) {
                this.insert(String(patient.id), patient);
            }
        }
    }

    // Single hash table instance for the dashboard
    const patientHashTable = new ClientPatientHashTable(101);

    // Build the hash index from the patient array
    function buildIndex(patients) {
        patientHashTable.clear();
        (patients || []).forEach(p => patientHashTable.indexAssessment(p));
    }

    // Search function used by the UI
    function searchPatients(query) {
        if (!query || !query.trim()) return null;
        return patientHashTable.search(query);
    }

    // Optional: Call the backend C++ Hash Search endpoint
    async function searchBackend(query) {
        if (!query || !query.trim()) return null;
        try {
            const auth = window.AuthNotificationModule;
            const res = await fetch('/api/worker/assessments/search?q=' + encodeURIComponent(query.trim()), {
                headers: auth ? auth.authHeaders() : {}
            });
            if (res.ok) {
                const data = await res.json();
                return data.results || [];
            }
        } catch (e) {
            console.warn('Backend hash search failed, falling back to local hash index:', e);
        }
        return patientHashTable.search(query);
    }

    // Initialize UI listeners on page load
    document.addEventListener('DOMContentLoaded', function () {
        const searchInput = document.getElementById('patient-search-input');
        const clearBtn = document.getElementById('patient-search-clear');
        const countBadge = document.getElementById('search-result-count');

        if (!searchInput) return;

        searchInput.addEventListener('input', function () {
            const query = searchInput.value.trim();

            if (clearBtn) {
                clearBtn.style.display = query ? 'block' : 'none';
            }

            if (!query) {
                if (countBadge) countBadge.style.display = 'none';
                if (window.DashboardTriageModule && window.DashboardTriageModule.getAllPatients) {
                    window.DashboardTriageModule.render(window.DashboardTriageModule.getAllPatients());
                }
                return;
            }

            // Execute O(1) Hash Table Lookup
            const matched = searchPatients(query);

            if (countBadge) {
                countBadge.style.display = 'inline-block';
                countBadge.textContent = matched.length === 1 
                    ? '1 patient found (Hash Table O(1))' 
                    : matched.length + ' patients found (Hash Table O(1))';
            }

            if (window.DashboardTriageModule && window.DashboardTriageModule.render) {
                window.DashboardTriageModule.render(matched);
            }
        });

        if (clearBtn) {
            clearBtn.addEventListener('click', function () {
                searchInput.value = '';
                clearBtn.style.display = 'none';
                if (countBadge) countBadge.style.display = 'none';
                if (window.DashboardTriageModule && window.DashboardTriageModule.getAllPatients) {
                    window.DashboardTriageModule.render(window.DashboardTriageModule.getAllPatients());
                }
                searchInput.focus();
            });
        }
    });

    // Expose module globally
    window.PatientHashSearchModule = {
        buildIndex: buildIndex,
        search: searchPatients,
        searchBackend: searchBackend,
        ClientPatientHashTable: ClientPatientHashTable
    };
})();
