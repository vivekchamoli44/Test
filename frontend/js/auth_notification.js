/**
 * ============================================================================
 * Module 1: User Login, Sign Up & ASHA Notifications
 * Student: Yashwardhan Rohilla
 * ============================================================================
 * What this code does:
 *  1. Switches between Patient and ASHA Worker roles (on the login page).
 *  2. Sends login and registration requests to the C++ server.
 *  3. Stores the logged-in user in localStorage.
 *  4. Guards each page (index.html / patient.html / worker.html) so a
 *     visitor always lands on the page that matches their session state
 *     and role, navigating with real page loads instead of show/hide.
 *  5. Sends high-risk emergency notifications for ASHA workers.
 * ============================================================================
 */

let currentRole = 'patient';
let isRegisterMode = false;

// Which dashboard page a role belongs on.
function dashboardPageFor(role) {
    return role === 'asha_worker' ? 'worker.html' : 'patient.html';
}

// 1. Setup events when the page finishes loading
document.addEventListener('DOMContentLoaded', function () {
    const path = window.location.pathname;
    const isPatientPage = path.endsWith('patient.html');
    const isWorkerPage = path.endsWith('worker.html');
    const user = getCurrentUser();

    // --- Page guard: each page checks it's the right place for this visitor ---
    if (isPatientPage || isWorkerPage) {
        if (!user) {
            window.location.href = 'index.html';
            return;
        }
        const expectedPage = dashboardPageFor(user.role);
        if ((isPatientPage && expectedPage !== 'patient.html') ||
            (isWorkerPage && expectedPage !== 'worker.html')) {
            window.location.href = expectedPage;
            return;
        }
        setupNavUser(user);
        if (isPatientPage && window.DbHistoryModule) {
            window.DbHistoryModule.loadHistory(user.id);
        }
        if (isWorkerPage && window.DashboardTriageModule) {
            window.DashboardTriageModule.loadTriage();
        }
    } else if (user && user.role) {
        // Already logged in and sitting on the login page — go to the dashboard.
        window.location.href = dashboardPageFor(user.role);
        return;
    }

    // --- Login page form wiring (elements only exist on index.html) ---
    const tabPatient = document.getElementById('tab-patient');
    const tabWorker = document.getElementById('tab-worker');
    const toggleLink = document.getElementById('toggle-auth-btn');
    const authForm = document.getElementById('auth-form');

    // Tab switcher: Patient
    if (tabPatient) {
        tabPatient.addEventListener('click', function () {
            currentRole = 'patient';
            tabPatient.classList.add('active');
            tabWorker.classList.remove('active');
            if (authForm) authForm.reset();
        });
    }

    // Tab switcher: ASHA Worker
    if (tabWorker) {
        tabWorker.addEventListener('click', function () {
            currentRole = 'asha_worker';
            tabWorker.classList.add('active');
            tabPatient.classList.remove('active');
            if (authForm) authForm.reset();
        });
    }

    // Switch between Sign In and Sign Up form
    if (toggleLink) {
        toggleLink.addEventListener('click', function (e) {
            e.preventDefault();
            isRegisterMode = !isRegisterMode;
            if (authForm) authForm.reset();

            // Show or hide registration fields
            const regFields = document.querySelectorAll('.reg-field');
            regFields.forEach(function (field) {
                field.style.display = isRegisterMode ? 'block' : 'none';
            });

            document.getElementById('auth-title').textContent = isRegisterMode ? 'Create New Account' : 'Sign In';
            document.getElementById('auth-submit').textContent = isRegisterMode ? 'Sign Up' : 'Sign In';
            toggleLink.textContent = isRegisterMode ? 'Already have an account? Sign In' : 'New user? Sign Up';
        });
    }

    // Submit Auth Form
    if (authForm) {
        authForm.reset();
        authForm.addEventListener('submit', handleAuthSubmit);
    }

    // --- Logout (elements only exist on patient.html / worker.html) ---
    const logoutBtn = document.getElementById('logout-btn');
    if (logoutBtn) {
        logoutBtn.addEventListener('click', function () {
            localStorage.removeItem('auth_user');
            localStorage.removeItem('auth_token');
            window.location.href = 'index.html';
        });
    }
});

// 2. Handle Login / Sign Up Submit
async function handleAuthSubmit(e) {
    e.preventDefault();
    const submitBtn = document.getElementById('auth-submit');
    submitBtn.disabled = true;
    submitBtn.textContent = 'Please wait...';

    const email = document.getElementById('email').value.trim().toLowerCase();
    const password = document.getElementById('password').value.trim();

    if (!email || !email.includes('@')) {
        showToast('Please enter a valid Gmail address', 'error');
        submitBtn.disabled = false;
        submitBtn.textContent = isRegisterMode ? 'Sign Up' : 'Sign In';
        return;
    }

    try {
        if (isRegisterMode) {
            // Registration payload
            const payload = {
                email: email,
                username: email,
                password: password,
                role: currentRole,
                full_name: document.getElementById('reg_name').value.trim() || email.split('@')[0].replace(/[._-]+/g, ' ').replace(/\b\w/g, c => c.toUpperCase()),
                phone: document.getElementById('reg_phone').value.trim(),
                village_area: document.getElementById('reg_area').value.trim(),
                age: parseInt(document.getElementById('reg_age').value) || 30,
                gender: document.getElementById('reg_gender').value || 'Other'
            };

            const response = await fetch('/api/auth/register', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });

            const data = await response.json();
            if (!response.ok) throw new Error(data.error || 'Registration failed');

            saveSession(data.token, data.user);
            showToast('Account registered successfully!', 'success');
            openDashboard(data.user.role);

        } else {
            // Login payload
            const payload = {
                email: email,
                username: email,
                password: password
            };

            const response = await fetch('/api/auth/login', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });

            const data = await response.json();
            if (!response.ok) throw new Error(data.error || 'Login failed');

            saveSession(data.token, data.user);
            showToast('Welcome back, ' + data.user.full_name + '!', 'success');
            openDashboard(data.user.role);
        }

    } catch (err) {
        showToast(err.message, 'error');
    } finally {
        submitBtn.disabled = false;
        submitBtn.textContent = isRegisterMode ? 'Sign Up' : 'Sign In';
    }
}

// 3. Save User Session to Browser LocalStorage
function saveSession(token, user) {
    localStorage.setItem('auth_token', token);
    localStorage.setItem('auth_user', JSON.stringify(user));
}

// 4. Get Current User Object
function getCurrentUser() {
    try {
        return JSON.parse(localStorage.getItem('auth_user')) || null;
    } catch (e) {
        return null;
    }
}

// 5. Navigate to the dashboard page that matches this role
function openDashboard(role) {
    window.location.href = dashboardPageFor(role);
}

// Populate the nav bar's user pill on patient.html / worker.html
function setupNavUser(user) {
    const navInfo = document.getElementById('nav-user-info');
    if (navInfo) navInfo.style.display = 'flex';

    const nameEl = document.getElementById('nav-user-name');
    if (nameEl && user) {
        let displayName = user.full_name || '';
        // If full_name is missing or contains an email address, extract clean name
        if (!displayName || displayName.includes('@')) {
            let raw = (displayName || user.email || user.username || 'User').split('@')[0];
            displayName = raw.replace(/[._-]+/g, ' ').replace(/\b\w/g, function (c) {
                return c.toUpperCase();
            });
        }
        nameEl.textContent = displayName;
    }
}

// 7. Simple Toast Notification
function showToast(message, type) {
    const toast = document.createElement('div');
    toast.style.position = 'fixed';
    toast.style.bottom = '20px';
    toast.style.right = '20px';
    toast.style.backgroundColor = (type === 'error') ? '#ef4444' : '#10b981';
    toast.style.color = '#ffffff';
    toast.style.padding = '10px 18px';
    toast.style.borderRadius = '6px';
    toast.style.fontWeight = 'bold';
    toast.style.fontSize = '14px';
    toast.style.zIndex = '9999';
    toast.textContent = message;

    document.body.appendChild(toast);
    setTimeout(function () {
        toast.remove();
    }, 3000);
}

// 8. ASHA Worker Action: Send Emergency Alert
async function sendNotification(assessmentId, patientName, btnElement) {
    if (!confirm('Send emergency alert to ' + patientName + '?')) {
        return;
    }

    btnElement.disabled = true;
    btnElement.textContent = 'Sending...';

    try {
        const response = await fetch('/api/worker/assessments/' + assessmentId + '/alert', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                message: 'EMERGENCY ALERT: High risk flagged for ' + patientName + '. Please consult doctor immediately.'
            })
        });

        const data = await response.json();
        if (!response.ok) throw new Error(data.error || 'Failed to dispatch alert');

        showToast('Emergency alert sent successfully!', 'success');
        if (window.DashboardTriageModule) {
            window.DashboardTriageModule.loadTriage();
        }

    } catch (err) {
        showToast(err.message, 'error');
        btnElement.disabled = false;
        btnElement.textContent = '🚨 Send Alert';
    }
}

// Export module functions
window.AuthNotificationModule = {
    notify: showToast,
    sendNotification: sendNotification,
    getUser: getCurrentUser
};
