/**
 * ============================================================================
 * Module 3: Database & Previous Assessment History
 * Student: Anjali
 * ============================================================================
 * What this code does:
 *  1. Checks database connection status.
 *  2. Fetches past health assessments for the patient from the server.
 *  3. Displays history cards with vitals and risk scores.
 *  4. Allows viewing past AI reports on click.
 * ============================================================================
 */

let alertsPollingTimer = null;

async function loadHistory(patientId, isSilent) {
    const container = document.getElementById('history-timeline-box');
    if (!container) return;

    if (!isSilent) {
        container.innerHTML = '<p style="text-align: center; color: var(--text-muted); padding: 15px;">Loading past assessments...</p>';
    }

    try {
        const id = patientId || 2;
        const response = await fetch('/api/assessments/my?patient_id=' + id);
        const data = await response.json();
        const list = data.assessments || [];

        // Auto-poll in background every 8 seconds for real-time ASHA alerts
        // (patient.html is a dedicated page now, so no visibility check is needed)
        if (!alertsPollingTimer) {
            alertsPollingTimer = setInterval(function () {
                loadHistory(id, true);
            }, 8000);
        }

        if (list.length === 0) {
            container.innerHTML = '<div style="text-align: center; padding: 20px; color: var(--text-muted); background: #f8fafc; border-radius: 8px; border: 1px dashed var(--border);"><p>No previous assessments found. Click <strong>Start New Assessment</strong> above.</p></div>';
            renderAlertsSidebar([]);
            return;
        }

        // Render ASHA worker alerts in the patient sidebar
        renderAlertsSidebar(list);

        container.innerHTML = '';
        list.forEach(function (item) {
            let badgeClass = 'badge-low';
            if (item.risk_level === 'Critical') badgeClass = 'badge-critical';
            else if (item.risk_level === 'High') badgeClass = 'badge-high';
            else if (item.risk_level === 'Moderate') badgeClass = 'badge-moderate';

            const card = document.createElement('div');
            card.className = 'card';
            card.style.display = 'flex';
            card.style.justifyContent = 'space-between';
            card.style.alignItems = 'center';
            card.style.marginBottom = '12px';

            card.innerHTML = `
                <div>
                    <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 4px;">
                        <strong>Assessment #${item.id}</strong>
                        <span class="badge ${badgeClass}">${item.risk_level} (${item.risk_score}/100)</span>
                        ${item.alert_sent ? '<span class="alert-flag">🚨 Alert Dispatched</span>' : ''}
                    </div>
                    <div style="font-size: 13px; color: var(--text-muted); margin-bottom: 6px;">
                        Date: ${item.created_at || 'Recent'} | Specialist: <strong style="color: var(--primary);">${item.recommended_doctor}</strong>
                    </div>
                    <div style="font-size: 13px; color: var(--text-muted);">
                        Reported Symptoms: <strong>${item.symptoms_text || 'None reported'}</strong>
                    </div>
                </div>
                <div>
                    <button class="btn btn-outline btn-sm view-past-btn">View Report</button>
                </div>
            `;

            card.querySelector('.view-past-btn').addEventListener('click', function () {
                if (window.AiAssessmentModule) {
                    window.AiAssessmentModule.renderReport(item, true);
                }
            });

            container.appendChild(card);
        });

    } catch (err) {
        container.innerHTML = '<p style="color: #ef4444; padding: 10px;">Failed to load history: ' + err.message + '</p>';
    }
}

// 2. Render ASHA Worker Alerts in Patient Dashboard Sidebar
function renderAlertsSidebar(list) {
    const alertsContainer = document.getElementById('patient-alerts-list');
    const badgeEl = document.getElementById('patient-alert-badge');
    if (!alertsContainer) return;

    const alerts = (list || []).filter(function (item) {
        return item.alert_sent;
    });

    if (badgeEl) {
        if (alerts.length > 0) {
            badgeEl.className = 'badge badge-critical';
            badgeEl.textContent = alerts.length + (alerts.length === 1 ? ' Alert' : ' Alerts');
        } else {
            badgeEl.className = 'badge badge-low';
            badgeEl.textContent = '0 Alerts';
        }
    }

    if (alerts.length === 0) {
        alertsContainer.innerHTML = `
            <div style="text-align: center; padding: 18px 12px; background: #f0fdf4; border: 1px dashed #86efac; border-radius: 8px; color: #166534; font-size: 12px;">
                <div style="font-size: 24px; margin-bottom: 6px;">✅</div>
                <strong>No Active Alerts</strong>
                <p style="margin: 4px 0 0; color: #15803d; font-size: 11px;">Your ASHA community health worker has not dispatched emergency alerts.</p>
            </div>
        `;
        return;
    }

    alertsContainer.innerHTML = '';
    alerts.forEach(function (item) {
        const alertCard = document.createElement('div');
        alertCard.className = 'alert-item-card';

        const alertTime = item.alert_sent_at || item.created_at || 'Recently';
        const alertMsg = item.alert_message || 'HIGH RISK ALERT: ASHA Healthcare Worker has flagged critical risk for this patient.';

        alertCard.innerHTML = `
            <div class="alert-item-header">
                <span>🚨 ASHA DISPATCHED</span>
                <span>${alertTime}</span>
            </div>
            <div class="alert-item-msg">
                ${alertMsg}
            </div>
            <div style="font-size: 11.5px; color: #991b1b; margin-bottom: 6px;">
                Specialist: <strong>${item.recommended_doctor || 'General Physician'}</strong>
            </div>
            <div class="alert-item-footer">
                <span>Assessment #${item.id} (${item.risk_score}/100 Risk)</span>
                <button type="button" class="btn btn-danger btn-sm view-alert-report-btn" style="padding: 3px 8px; font-size: 11px;">View Report</button>
            </div>
        `;

        alertCard.querySelector('.view-alert-report-btn').addEventListener('click', function () {
            if (window.AiAssessmentModule) {
                window.AiAssessmentModule.renderReport(item, true);
            }
        });

        alertsContainer.appendChild(alertCard);
    });
}

// Export module functions
window.DbHistoryModule = {
    loadHistory: loadHistory,
    renderAlertsSidebar: renderAlertsSidebar
};
