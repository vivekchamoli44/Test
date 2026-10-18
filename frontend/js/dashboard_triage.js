/**
 * ============================================================================
 * Module 4: ASHA Worker Dashboard & High Risk Patient Triage
 * Student: Nandini
 * ============================================================================
 * What this code does:
 *  1. Fetches community patient screenings sorted by highest risk score.
 *  2. Updates summary KPI counts (Total, Critical, High Risk, Alerts).
 *  3. Displays patient triage table.
 *  4. Provides direct patient contact (Call, WhatsApp, Outreach Notes).
 * ============================================================================
 */

let allPatients = [];
let activePatient = null;

document.addEventListener('DOMContentLoaded', function () {
    const closeContactBtn = document.getElementById('close-contact-modal');

    if (closeContactBtn) {
        closeContactBtn.addEventListener('click', function () {
            document.getElementById('contact-modal').classList.remove('active');
        });
    }
});

// 1. Load ASHA Worker Patients
async function loadTriage() {
    const tbody = document.getElementById('triage-tbody');
    if (!tbody) return;

    tbody.innerHTML = '<tr><td colspan="6" style="text-align: center; padding: 20px; color: var(--text-muted);">Loading patient records...</td></tr>';

    try {
        // Server returns only patients from this ASHA worker's own area
        const auth = window.AuthNotificationModule;
        const response = await fetch('/api/worker/assessments', { headers: auth.authHeaders() });
        if (auth.handleSessionExpired(response)) return;
        const data = await response.json();
        if (!response.ok) throw new Error(data.error || 'Request failed');
        allPatients = data.assessments || [];

        const areaLabel = document.getElementById('worker-area-label');
        if (areaLabel) {
            areaLabel.textContent = data.village_area
                ? 'Showing patients from your area: ' + data.village_area
                : 'No area is set on your account, so no patients can be shown.';
        }

        // Update KPI metrics
        const elTotal = document.getElementById('kpi-total');
        const elCrit = document.getElementById('kpi-critical');
        const elHigh = document.getElementById('kpi-high');
        const elAlerts = document.getElementById('kpi-alerts');

        if (elTotal) elTotal.textContent = data.total_screenings || allPatients.length;
        if (elCrit) elCrit.textContent = data.critical_risk_count || 0;
        if (elHigh) elHigh.textContent = data.high_risk_count || 0;
        if (elAlerts) elAlerts.textContent = data.alerts_sent_count || 0;

        renderTriageTable(allPatients);

    } catch (err) {
        tbody.innerHTML = '<tr><td colspan="6" style="text-align: center; color: #ef4444; padding: 20px;">Error loading patients: ' + err.message + '</td></tr>';
    }
}

// 2. Render Rows in Triage Table
function renderTriageTable(patients) {
    const tbody = document.getElementById('triage-tbody');
    if (!tbody) return;

    if (patients.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" style="text-align: center; padding: 20px; color: var(--text-muted);">No patients from your area yet.</td></tr>';
        return;
    }

    tbody.innerHTML = '';
    patients.forEach(function (item) {
        let badgeClass = 'badge-low';
        if (item.risk_level === 'Critical') badgeClass = 'badge-critical';
        else if (item.risk_level === 'High') badgeClass = 'badge-high';
        else if (item.risk_level === 'Moderate') badgeClass = 'badge-moderate';

        // Format concise symptoms without large text block
        const symptomsArr = (item.symptoms_text || '')
            .split(',')
            .map(function (s) { return s.trim(); })
            .filter(function (s) { return s.length > 0; });
        const primarySymptom = symptomsArr[0] || 'No symptoms reported';
        const moreCount = symptomsArr.length > 1 ? symptomsArr.length - 1 : 0;
        const fullSymptomsSafe = (item.symptoms_text || '').replace(/"/g, '&quot;');
        const diseasesText = (item.possible_chronic_diseases || []).join(', ') || 'General Review';

        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td>
                <span style="font-size: 16px; font-weight: bold; color: ${item.risk_score >= 80 ? '#dc2626' : item.risk_score >= 60 ? '#ea580c' : '#059669'};">
                    ${item.risk_score}
                </span>
                <span class="badge ${badgeClass}" style="margin-left: 6px;">${item.risk_level}</span>
            </td>
            <td>
                <strong>${item.patient_name}</strong>
                <div style="font-size: 12px; color: var(--text-muted);">Age: ${item.age} | ${item.gender} | ${item.village_area}</div>
            </td>
            <td>
                <div style="max-width: 210px;" title="${fullSymptomsSafe}">
                    <div style="font-size: 12px; font-weight: 600; color: var(--primary-dark); white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">
                        ${primarySymptom}
                    </div>
                    ${moreCount > 0 ? `<div style="font-size: 11px; color: var(--text-muted); margin-top: 2px;">+${moreCount} other sign${moreCount > 1 ? 's' : ''}</div>` : ''}
                </div>
            </td>
            <td>
                <strong style="color: var(--primary); font-size: 13px;">${item.recommended_doctor || 'General Physician'}</strong>
                <div style="font-size: 11px; color: var(--text-muted); max-width: 180px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;" title="${diseasesText.replace(/"/g, '&quot;')}">
                    ${diseasesText}
                </div>
            </td>
            <td>
                ${item.alert_sent ? '<span class="alert-flag">🚨 Dispatched</span>' : '<span style="color: var(--text-muted); font-size: 12px;">Pending</span>'}
                ${item.asha_contacted ? '<div style="font-size: 12px; color: #16a34a; font-weight: bold; margin-top: 2px;">✓ Contacted</div>' : ''}
            </td>
            <td>
                <div style="display: flex; gap: 6px;">
                    <button class="btn btn-danger btn-sm action-alert-btn" ${item.alert_sent ? 'disabled style="opacity: 0.5;"' : ''}>Alert</button>
                    <button class="btn btn-accent btn-sm action-contact-btn">Contact</button>
                    <button class="btn btn-outline btn-sm action-view-btn">Report</button>
                </div>
            </td>
        `;

        // Alert button handler
        const alertBtn = tr.querySelector('.action-alert-btn');
        if (alertBtn && !item.alert_sent) {
            alertBtn.addEventListener('click', function () {
                if (window.AuthNotificationModule) {
                    window.AuthNotificationModule.sendNotification(item.id, item.patient_name, alertBtn);
                }
            });
        }

        // Contact button handler
        tr.querySelector('.action-contact-btn').addEventListener('click', function () {
            openContact(item);
        });

        // Report button handler
        tr.querySelector('.action-view-btn').addEventListener('click', function () {
            if (window.AiAssessmentModule) {
                window.AiAssessmentModule.renderReport(item, true);
            }
        });

        tbody.appendChild(tr);
    });
}

// 3. Open Patient Contact Modal (Shows only Name, Contact No, and Location)
function openContact(item) {
    activePatient = item;
    const modal = document.getElementById('contact-modal');
    if (!modal) return;

    const nameEl = document.getElementById('modal-patient-name');
    const phoneEl = document.getElementById('modal-patient-phone');
    const areaEl = document.getElementById('modal-patient-area');

    if (nameEl) nameEl.textContent = item.patient_name || '--';
    if (phoneEl) phoneEl.textContent = item.phone || '--';
    if (areaEl) areaEl.textContent = item.village_area || '--';

    modal.classList.add('active');
}

// Export module functions
window.DashboardTriageModule = {
    loadTriage: loadTriage
};
