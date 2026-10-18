    /**
 * ============================================================================
 * Module 2: AI Health Assessment & Gemini Integration
 * Student: Vivek Chamoli
 * ============================================================================
 * What this code does:
 *  1. Opens and closes the assessment modal.
 *  2. Collects patient vitals and selected symptoms.
 *  3. Sends data to the AI assessment API (/api/assessments).
 *  4. Displays the risk score, recommended doctor, and health advice.
 * ============================================================================
 */

document.addEventListener('DOMContentLoaded', function () {
    const openBtn = document.getElementById('open-assess-modal');
    const modal = document.getElementById('assess-modal');
    const form = document.getElementById('assessment-form');

    // Open assessment modal
    if (openBtn && modal) {
        openBtn.addEventListener('click', function () {
            if (form) form.reset();
            modal.classList.add('active');
        });
    }

    // Submit assessment form
    if (form) {
        form.addEventListener('submit', handleAssessmentSubmit);
    }
});

// 1. Submit Health Assessment
async function handleAssessmentSubmit(e) {
    e.preventDefault();
    const submitBtn = document.getElementById('submit-assess-btn');
    submitBtn.disabled = true;
    submitBtn.textContent = 'Running AI Diagnosis...';

    // Get current logged in patient info
    let user = null;
    try {
        user = JSON.parse(localStorage.getItem('auth_user'));
    } catch (err) {}

    // Collect selected symptom checkboxes
    const symptoms = [];
    document.querySelectorAll('input[name="symptom_item"]:checked').forEach(function (cb) {
        symptoms.push(cb.value);
    });

    const duration = document.getElementById('input-symptom-duration')?.value;
    if (duration) {
        symptoms.push('Duration: ' + duration);
    }

    const severity = document.querySelector('input[name="severity_level"]:checked')?.value;
    if (severity) {
        symptoms.push('Severity: ' + severity);
    }

    const customNotes = document.getElementById('input-custom-sym')?.value.trim();
    if (customNotes) {
        symptoms.push(customNotes);
    }

    // Collect family and lifestyle history
    const historyList = [];
    document.querySelectorAll('input[name="family_hist"]:checked').forEach(function (cb) {
        historyList.push(cb.value);
    });
    document.querySelectorAll('input[name="lifestyle_hist"]:checked').forEach(function (cb) {
        historyList.push(cb.value);
    });
    const priorMed = document.getElementById('input-history')?.value.trim();
    if (priorMed) {
        historyList.push(priorMed);
    }

    // Build assessment payload (without clinical vitals & measurements)
    const payload = {
        patient_id: user ? user.id : 2,
        patient_name: user ? user.full_name : 'Patient',
        age: user ? user.age : 45,
        gender: user ? user.gender : 'Not Specified',
        phone: user ? user.phone : '+91 98765 00000',
        village_area: user ? user.village_area : 'Community Area',
        bp_systolic: 0,
        bp_diastolic: 0,
        blood_sugar: 0,
        heart_rate: 0,
        oxygen_level: 0,
        temperature: 0,
        symptoms_text: symptoms.length > 0 ? symptoms.join(', ') : 'None',
        medical_history: historyList.length > 0 ? historyList.join(', ') : 'No prior medical history'
    };

    try {
        const response = await fetch('/api/assessments', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });

        const data = await response.json();
        if (!response.ok) throw new Error(data.error || 'AI assessment failed');

        if (window.AuthNotificationModule) {
            window.AuthNotificationModule.notify('AI Health Assessment complete!', 'success');
        }

        // Close modal and reset form
        document.getElementById('assess-modal').classList.remove('active');
        if (form) form.reset();

        // Render Report on the patient page
        renderReport(data.assessment, false);

        // Refresh past assessments history
        if (window.DbHistoryModule && user) {
            window.DbHistoryModule.loadHistory(user.id);
        }

    } catch (err) {
        if (window.AuthNotificationModule) {
            window.AuthNotificationModule.notify(err.message, 'error');
        } else {
            alert(err.message);
        }
    } finally {
        submitBtn.disabled = false;
        submitBtn.textContent = '⚡ Run AI Diagnostic Assessment';
    }
}

// 2. Render Assessment Report on screen or in modal
function renderReport(a, openModal) {
    if (!a) return;

    // Determine badge color
    let badgeColor = '#10b981';
    let badgeClass = 'badge-low';
    if (a.risk_level === 'Critical') {
        badgeColor = '#ef4444';
        badgeClass = 'badge-critical';
    } else if (a.risk_level === 'High') {
        badgeColor = '#f97316';
        badgeClass = 'badge-high';
    } else if (a.risk_level === 'Moderate') {
        badgeColor = '#eab308';
        badgeClass = 'badge-moderate';
    }

    // A. Populate In-Page Report Card
    const inPageBox = document.getElementById('ai-report-display');
    if (inPageBox) {
        const scoreEl = document.getElementById('diag-score');
        const circleEl = document.getElementById('diag-circle');
        const badgeEl = document.getElementById('diag-badge');
        const docEl = document.getElementById('diag-doctor');
        const actEl = document.getElementById('diag-actions');

        if (scoreEl) scoreEl.textContent = a.risk_score;
        if (circleEl) circleEl.style.background = badgeColor;
        if (badgeEl) {
            badgeEl.className = 'badge ' + badgeClass;
            badgeEl.textContent = a.risk_level + ' Risk';
        }
        if (docEl) docEl.textContent = a.recommended_doctor || 'General Physician';
        if (actEl) actEl.textContent = a.immediate_actions || 'Consult physician.';

        // Populate chronic diseases
        const diseasesBox = document.getElementById('diag-diseases');
        if (diseasesBox) {
            diseasesBox.innerHTML = '';
            (a.possible_chronic_diseases || []).forEach(function (d) {
                const span = document.createElement('span');
                span.className = 'pill-tag';
                span.textContent = d;
                diseasesBox.appendChild(span);
            });
        }

        // Populate warning symptoms
        const symList = document.getElementById('diag-other-sym');
        if (symList) {
            symList.innerHTML = '';
            (a.other_symptoms_to_watch || []).forEach(function (s) {
                const li = document.createElement('li');
                li.textContent = s;
                symList.appendChild(li);
            });
        }

        // Populate precautions
        const precList = document.getElementById('diag-precautions');
        if (precList) {
            precList.innerHTML = '';
            (a.precautions || []).forEach(function (p) {
                const li = document.createElement('li');
                li.textContent = p;
                precList.appendChild(li);
            });
        }

        inPageBox.classList.add('active');
        // patient.html is a dedicated page now, so this box is always visible when present.
        inPageBox.scrollIntoView({ behavior: 'smooth' });
    }

    // B. Populate Report Modal (for ASHA workers and history view)
    const modal = document.getElementById('report-modal');
    if (modal) {
        const mScore = document.getElementById('modal-diag-score');
        const mCircle = document.getElementById('modal-diag-circle');
        const mBadge = document.getElementById('modal-diag-badge');
        const mPatient = document.getElementById('modal-report-patient-name');
        const mMeta = document.getElementById('modal-report-meta');
        const mTime = document.getElementById('modal-report-timestamp');
        const mDoctor = document.getElementById('modal-diag-doctor');
        const mActions = document.getElementById('modal-diag-actions');

        if (mScore) mScore.textContent = a.risk_score;
        if (mCircle) mCircle.style.background = badgeColor;
        if (mBadge) {
            mBadge.className = 'badge ' + badgeClass;
            mBadge.textContent = a.risk_level + ' Risk';
        }
        if (mPatient) mPatient.textContent = (a.patient_name || 'Patient') + ' - Clinical Assessment Report';
        if (mMeta) mMeta.textContent = 'Age: ' + (a.age || '--') + ' | Area: ' + (a.village_area || '--') + ' | Phone: ' + (a.phone || '--');
        if (mTime) mTime.textContent = a.created_at || 'Just now';
        if (mDoctor) mDoctor.textContent = a.recommended_doctor || 'General Physician';
        if (mActions) mActions.textContent = a.immediate_actions || 'Consult physician.';

        // Modal diseases
        const mDiseases = document.getElementById('modal-diag-diseases');
        if (mDiseases) {
            mDiseases.innerHTML = '';
            (a.possible_chronic_diseases || []).forEach(function (d) {
                const span = document.createElement('span');
                span.className = 'pill-tag';
                span.textContent = d;
                mDiseases.appendChild(span);
            });
        }

        // Modal warning symptoms
        const mSym = document.getElementById('modal-diag-other-sym');
        if (mSym) {
            mSym.innerHTML = '';
            (a.other_symptoms_to_watch || []).forEach(function (s) {
                const li = document.createElement('li');
                li.textContent = s;
                mSym.appendChild(li);
            });
        }

        // Modal precautions
        const mPrec = document.getElementById('modal-diag-precautions');
        if (mPrec) {
            mPrec.innerHTML = '';
            (a.precautions || []).forEach(function (p) {
                const li = document.createElement('li');
                li.textContent = p;
                mPrec.appendChild(li);
            });
        }

        if (openModal) {
            modal.classList.add('active');
        }
    }
}

// Export module functions
window.AiAssessmentModule = {
    renderReport: renderReport
};
