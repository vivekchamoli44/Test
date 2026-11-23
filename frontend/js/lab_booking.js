/**
 * ============================================================================
 * Module: Diagnostic Lab Test Booking & Phlebotomy Triage (DSA Module)
 * ============================================================================
 * Implements:
 *  1. Interval Tree O(log N) slot availability & double-booking prevention
 *  2. Priority Queue (Max-Heap) for ASHA worker sample collection triage
 * ============================================================================
 */

(function () {
    let labCatalog = [];
    let selectedSlot = null;
    let isBookingSubmitting = false;

    // Helper: format YYYY-MM-DD in local time
    function formatDateYMD(d) {
        const year = d.getFullYear();
        const month = String(d.getMonth() + 1).padStart(2, '0');
        const day = String(d.getDate()).padStart(2, '0');
        return `${year}-${month}-${day}`;
    }

    function getTomorrowDateStr() {
        const d = new Date();
        d.setDate(d.getDate() + 1);
        return formatDateYMD(d);
    }

    function getTodayDateStr() {
        return formatDateYMD(new Date());
    }

    // ------------------------------------------------------------------------
    // Helper: Update booking summary card
    // ------------------------------------------------------------------------
    function updateBookingSummary() {
        const testSelect = document.getElementById('lab-test-select');
        const dateInput = document.getElementById('lab-date-select');
        const summaryBox = document.getElementById('lab-booking-summary');
        if (!summaryBox) return;

        const code = testSelect ? testSelect.value : '';
        const testItem = labCatalog.find(t => t.code === code);
        const date = dateInput ? dateInput.value : '';

        // If selectedSlot is not set, try reading from slotSelect directly
        let currentSlot = selectedSlot;
        if (!currentSlot) {
            const slotSelect = document.getElementById('lab-slot-select');
            if (slotSelect && slotSelect.value && slotSelect.selectedIndex >= 0) {
                const opt = slotSelect.options[slotSelect.selectedIndex];
                if (opt && opt.getAttribute('data-start')) {
                    currentSlot = {
                        start_mins: parseInt(opt.getAttribute('data-start'), 10),
                        end_mins: parseInt(opt.getAttribute('data-end'), 10),
                        label: opt.getAttribute('data-label') || opt.textContent.replace(/^[🟢❌]\s*/, '').replace(/\s*\(Available\)|\s*\(Booked.*\)/, '').trim(),
                        is_available: true
                    };
                }
            }
        }

        if (testItem && currentSlot) {
            summaryBox.innerHTML = `
                <span style="color: #15803d; font-weight: 700;">
                    ✓ Ready to Book: <strong>${testItem.name}</strong> on <strong>${date}</strong> at <strong>${currentSlot.label}</strong>
                </span>
            `;
        } else if (testItem && !currentSlot) {
            summaryBox.innerHTML = `
                <span style="color: #b45309; font-weight: 600;">
                    👉 Selected: <strong>${testItem.name}</strong>. Please choose a collection timing slot from dropdown #3.
                </span>
            `;
        } else if (!testItem && currentSlot) {
            summaryBox.innerHTML = `
                <span style="color: #5b65dc; font-weight: 600;">
                    👉 Slot selected: <strong>${currentSlot.label}</strong>. Please select a diagnostic test from dropdown #1.
                </span>
            `;
        } else {
            summaryBox.innerHTML = `
                <span style="color: var(--text-muted);">
                    Select a diagnostic test, date, and timing slot above to confirm your booking.
                </span>
            `;
        }
    }

    // ------------------------------------------------------------------------
    // Load Slots from Interval Tree API
    // ------------------------------------------------------------------------
    async function loadAvailableSlots(date) {
        const slotSelect = document.getElementById('lab-slot-select');
        const slotsContainer = document.getElementById('lab-slots-container');
        selectedSlot = null;

        if (slotSelect) {
            slotSelect.innerHTML = '<option value="">Checking slot availability via Interval Tree...</option>';
            slotSelect.disabled = true;
        }
        if (slotsContainer) {
            slotsContainer.innerHTML = '<div style="color: var(--text-muted); font-size: 0.85rem; padding: 10px;">Checking slot availability via Interval Tree...</div>';
        }
        updateBookingSummary();

        try {
            const res = await fetch('/api/lab/available-slots?date=' + encodeURIComponent(date));
            const data = await res.json();
            const slots = data.slots || [];

            if (slotSelect) {
                slotSelect.disabled = false;
                slotSelect.innerHTML = '';
                if (slots.length === 0) {
                    slotSelect.innerHTML = '<option value="">No slots available for this date</option>';
                    selectedSlot = null;
                } else {
                    let firstAvailable = null;

                    slots.forEach(slot => {
                        const opt = document.createElement('option');
                        opt.value = `${slot.start_mins}-${slot.end_mins}`;
                        opt.setAttribute('data-start', slot.start_mins);
                        opt.setAttribute('data-end', slot.end_mins);
                        opt.setAttribute('data-label', slot.label);

                        if (slot.is_available) {
                            opt.textContent = `🟢 ${slot.label} (Available)`;
                            if (!firstAvailable) {
                                firstAvailable = slot;
                            }
                        } else {
                            opt.textContent = `❌ ${slot.label} (Booked / Conflict)`;
                            opt.disabled = true;
                            opt.style.color = '#94a3b8';
                            opt.style.background = '#f1f5f9';
                        }
                        slotSelect.appendChild(opt);
                    });

                    // Automatically pre-select the first available slot!
                    if (firstAvailable) {
                        slotSelect.value = `${firstAvailable.start_mins}-${firstAvailable.end_mins}`;
                        selectedSlot = firstAvailable;
                    } else {
                        // All booked on this date
                        const noOpt = document.createElement('option');
                        noOpt.value = '';
                        noOpt.textContent = '⚠️ All slots booked for this date';
                        noOpt.selected = true;
                        slotSelect.insertBefore(noOpt, slotSelect.firstChild);
                        selectedSlot = null;
                    }
                }
            }

            if (slotsContainer) {
                slotsContainer.innerHTML = '';
                if (slots.length === 0) {
                    slotsContainer.innerHTML = '<div style="color: var(--text-muted); font-size: 0.85rem;">No slots available for this date.</div>';
                } else {
                    slots.forEach(slot => {
                        const btn = document.createElement('button');
                        btn.type = 'button';
                        const isSelected = selectedSlot && selectedSlot.start_mins === slot.start_mins;
                        btn.className = 'slot-btn' + (slot.is_available ? '' : ' slot-booked') + (isSelected ? ' selected' : '');
                        btn.disabled = !slot.is_available;
                        btn.innerHTML = `
                            <strong>${slot.label}</strong>
                            <small style="display:block; font-size:10px; opacity:0.8;">${slot.is_available ? 'Available' : 'Booked'}</small>
                        `;

                        if (slot.is_available) {
                            btn.addEventListener('click', function () {
                                document.querySelectorAll('.slot-btn').forEach(b => b.classList.remove('selected'));
                                btn.classList.add('selected');
                                selectedSlot = slot;
                                if (slotSelect) slotSelect.value = `${slot.start_mins}-${slot.end_mins}`;
                                updateBookingSummary();
                            });
                        }
                        slotsContainer.appendChild(btn);
                    });
                }
            }

            // Update summary with the newly selected slot
            updateBookingSummary();

        } catch (err) {
            console.error('[LabBooking] Failed to load slots:', err);
            if (slotSelect) {
                slotSelect.disabled = false;
                slotSelect.innerHTML = '<option value="">Failed to load slots</option>';
            }
            if (slotsContainer) {
                slotsContainer.innerHTML = '<div style="color: #ef4444; font-size: 0.85rem;">Failed to load slots: ' + err.message + '</div>';
            }
        }
    }

    // ------------------------------------------------------------------------
    // TOP-LEVEL SUBMIT HANDLER: Direct DOM queries, mutex guard & fail-safe validation
    // ------------------------------------------------------------------------
    async function handleBookingSubmit(e) {
        if (e) {
            if (typeof e.preventDefault === 'function') e.preventDefault();
            if (typeof e.stopPropagation === 'function') e.stopPropagation();
            if (typeof e.stopImmediatePropagation === 'function') e.stopImmediatePropagation();
        }

        // CRITICAL MUTEX: Prevent duplicate / double-dispatched booking calls
        if (isBookingSubmitting) {
            console.warn('[LabBooking] Booking already in progress. Ignoring duplicate click.');
            return;
        }

        const testSelect = document.getElementById('lab-test-select');
        const dateInput = document.getElementById('lab-date-select');
        const slotSelect = document.getElementById('lab-slot-select');
        const prepNote = document.getElementById('lab-prep-note');
        const fastingTag = document.getElementById('lab-fasting-tag');
        const bookBtn = document.getElementById('btn-confirm-lab-book');
        const summaryBox = document.getElementById('lab-booking-summary');

        const code = testSelect ? testSelect.value.trim() : '';
        const date = dateInput ? dateInput.value.trim() : '';

        // 1. Validate Test selection
        if (!code) {
            if (summaryBox) {
                summaryBox.innerHTML = '<span style="color: #dc2626; font-weight: 700;">⚠️ Please select a diagnostic test from dropdown #1 first.</span>';
                summaryBox.scrollIntoView({ behavior: 'smooth', block: 'center' });
            }
            if (testSelect) {
                testSelect.focus();
                testSelect.style.borderColor = '#dc2626';
                setTimeout(() => { if (testSelect) testSelect.style.borderColor = 'var(--border)'; }, 3000);
            }
            return;
        }

        // 2. Validate Date selection
        if (!date) {
            if (summaryBox) {
                summaryBox.innerHTML = '<span style="color: #dc2626; font-weight: 700;">⚠️ Please choose a preferred date for the appointment.</span>';
                summaryBox.scrollIntoView({ behavior: 'smooth', block: 'center' });
            }
            if (dateInput) {
                dateInput.focus();
                dateInput.style.borderColor = '#dc2626';
                setTimeout(() => { if (dateInput) dateInput.style.borderColor = 'var(--border)'; }, 3000);
            }
            return;
        }

        // 3. Validate & Resolve Slot selection with multi-tier fallback
        let slotToBook = selectedSlot;

        // Tier A: Read directly from slotSelect value if selectedSlot is missing
        if ((!slotToBook || !slotToBook.start_mins) && slotSelect && slotSelect.value && slotSelect.value.includes('-')) {
            const parts = slotSelect.value.split('-');
            const s = parseInt(parts[0], 10);
            const en = parseInt(parts[1], 10);
            if (!isNaN(s) && !isNaN(en)) {
                const selOption = slotSelect.options[slotSelect.selectedIndex];
                let label = selOption ? (selOption.getAttribute('data-label') || selOption.textContent) : `${s}-${en}`;
                label = label.replace(/^[🟢❌]\s*/, '').replace(/\s*\(Available\)|\s*\(Booked.*\)/, '').trim();
                slotToBook = {
                    start_mins: s,
                    end_mins: en,
                    label: label,
                    is_available: true
                };
                selectedSlot = slotToBook;
            }
        }

        // Tier B: Scan options in dropdown for first enabled/available slot
        if ((!slotToBook || !slotToBook.start_mins) && slotSelect && slotSelect.options.length > 0) {
            for (let i = 0; i < slotSelect.options.length; i++) {
                const opt = slotSelect.options[i];
                if (!opt.disabled && opt.value && opt.value.includes('-')) {
                    slotSelect.selectedIndex = i;
                    const parts = opt.value.split('-');
                    const s = parseInt(parts[0], 10);
                    const en = parseInt(parts[1], 10);
                    if (!isNaN(s) && !isNaN(en)) {
                        slotToBook = {
                            start_mins: s,
                            end_mins: en,
                            label: (opt.getAttribute('data-label') || opt.textContent).replace(/^[🟢❌]\s*/, '').replace(/\s*\(Available\)|\s*\(Booked.*\)/, '').trim(),
                            is_available: true
                        };
                        selectedSlot = slotToBook;
                        break;
                    }
                }
            }
        }

        if (!slotToBook || isNaN(slotToBook.start_mins) || isNaN(slotToBook.end_mins)) {
            if (summaryBox) {
                summaryBox.innerHTML = '<span style="color: #dc2626; font-weight: 700;">⚠️ All slots booked or unavailable on this date. Please choose another date in dropdown #2.</span>';
                summaryBox.scrollIntoView({ behavior: 'smooth', block: 'center' });
            }
            if (slotSelect) {
                slotSelect.focus();
                slotSelect.style.borderColor = '#dc2626';
                setTimeout(() => { if (slotSelect) slotSelect.style.borderColor = 'var(--border)'; }, 3000);
            }
            return;
        }

        // 4. Resolve patient credentials from session or localStorage
        let patientId = 2;
        let patientName = 'Patient';
        let phone = '+91 91234 56789';
        let villageArea = 'Rampur Ward 4';

        try {
            const rawUser = localStorage.getItem('auth_user') || localStorage.getItem('user');
            if (rawUser) {
                const parsed = JSON.parse(rawUser);
                if (parsed.id) patientId = parsed.id;
                if (parsed.full_name || parsed.username) patientName = parsed.full_name || parsed.username;
                if (parsed.phone) phone = parsed.phone;
                if (parsed.village_area) villageArea = parsed.village_area;
            }
        } catch (ignore) {}

        if (window.AuthNotificationModule && typeof window.AuthNotificationModule.getCurrentUser === 'function') {
            const currentUser = window.AuthNotificationModule.getCurrentUser();
            if (currentUser) {
                if (currentUser.id) patientId = currentUser.id;
                if (currentUser.full_name || currentUser.username) patientName = currentUser.full_name || currentUser.username;
                if (currentUser.phone) phone = currentUser.phone;
                if (currentUser.village_area) villageArea = currentUser.village_area;
            }
        }

        // Lock submission mutex
        isBookingSubmitting = true;
        if (bookBtn) {
            bookBtn.disabled = true;
            bookBtn.textContent = '⏳ Reserving slot...';
        }
        if (summaryBox) {
            summaryBox.innerHTML = '<span style="color: #5b65dc; font-weight: 700;">⏳ Verifying Interval Tree & booking collection slot...</span>';
        }

        try {
            const res = await fetch('/api/lab/book', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    patient_id: patientId,
                    patient_name: patientName,
                    phone: phone,
                    village_area: villageArea,
                    test_code: code,
                    booking_date: date,
                    slot_start: slotToBook.start_mins,
                    slot_end: slotToBook.end_mins
                })
            });

            const data = await res.json();
            if (!res.ok) {
                throw new Error(data.error || 'Failed to book slot.');
            }

            if (summaryBox) {
                summaryBox.innerHTML = `
                    <span style="color: #15803d; font-weight: 800;">
                        🎉 Booked Successfully! ${data.booking.test_name} on ${data.booking.booking_date} at ${data.booking.slot_label}.
                    </span>
                `;
            }

            if (window.AuthNotificationModule && typeof window.AuthNotificationModule.notify === 'function') {
                window.AuthNotificationModule.notify(`Lab test confirmed: ${data.booking.test_name} (${data.booking.slot_label})`, 'success');
            }

            alert(`✅ Lab Appointment Confirmed!\n\nTest: ${data.booking.test_name}\nDate: ${data.booking.booking_date}\nTiming: ${data.booking.slot_label}\nPatient: ${data.booking.patient_name}\nAlgorithm: Augmented Interval Tree (O(log N) conflict-free)`);

            // Reset test selection & slot
            if (testSelect) testSelect.value = '';
            if (fastingTag) fastingTag.style.display = 'none';
            if (prepNote) prepNote.textContent = 'Select a diagnostic test above to see medical preparation details.';
            document.querySelectorAll('.slot-btn').forEach(b => b.classList.remove('selected'));

            // Refresh available slots for that date (the booked slot will now show ❌ Booked)
            await loadAvailableSlots(date);

            // Refresh My Scheduled Lab Tests table
            await loadPatientBookings(patientId);

            // Auto-scroll to scheduled lab tests table so patient immediately sees their assessment
            const tableSection = document.getElementById('patient-lab-bookings-tbody');
            if (tableSection) {
                tableSection.scrollIntoView({ behavior: 'smooth', block: 'center' });
            }

        } catch (err) {
            console.error('[LabBooking] Booking error:', err);
            if (summaryBox) {
                summaryBox.innerHTML = `<span style="color: #dc2626; font-weight: 700;">❌ Booking Error: ${err.message}</span>`;
            }
            alert('Booking Error: ' + err.message);
            // Refresh slots in case a conflict occurred with someone else's booking
            if (date) {
                await loadAvailableSlots(date);
            }
        } finally {
            isBookingSubmitting = false;
            if (bookBtn) {
                bookBtn.disabled = false;
                bookBtn.textContent = 'Confirm Lab Appointment';
            }
        }
    }

    // ------------------------------------------------------------------------
    // 1. Patient Portal: Lab Test Booking & Slot Selection Initialization
    // ------------------------------------------------------------------------
    async function initPatientLabBooking() {
        const testSelect = document.getElementById('lab-test-select');
        const dateInput = document.getElementById('lab-date-select');
        const slotSelect = document.getElementById('lab-slot-select');
        const prepNote = document.getElementById('lab-prep-note');
        const fastingTag = document.getElementById('lab-fasting-tag');
        const bookBtn = document.getElementById('btn-confirm-lab-book');

        if (!testSelect || !dateInput) return;

        // Default to tomorrow's date, allow today as minimum
        if (!dateInput.value) {
            dateInput.value = getTomorrowDateStr();
        }
        dateInput.min = getTodayDateStr();

        // Fetch lab catalog
        try {
            const res = await fetch('/api/lab/catalog');
            const data = await res.json();
            labCatalog = data.catalog || [];

            testSelect.innerHTML = '<option value="">-- Select a Diagnostic Test --</option>';
            labCatalog.forEach(test => {
                const opt = document.createElement('option');
                opt.value = test.code;
                opt.textContent = `${test.name} (${test.sample_type})`;
                testSelect.appendChild(opt);
            });
        } catch (err) {
            console.error('Failed to load lab catalog:', err);
        }

        // On test selection change, update fasting & preparation instructions
        testSelect.addEventListener('change', function () {
            const code = testSelect.value;
            const item = labCatalog.find(t => t.code === code);
            if (item) {
                if (prepNote) prepNote.textContent = item.preparation_note;
                if (fastingTag) {
                    fastingTag.style.display = 'inline-block';
                    fastingTag.textContent = item.fasting_required ? '⚠️ Fasting Required' : '✓ No Fasting Required';
                    fastingTag.className = item.fasting_required ? 'badge badge-high' : 'badge badge-low';
                }
            } else {
                if (prepNote) prepNote.textContent = 'Select a diagnostic test above to see medical preparation details.';
                if (fastingTag) fastingTag.style.display = 'none';
            }
            updateBookingSummary();
        });

        // Listen for slot selection from dropdown
        if (slotSelect) {
            slotSelect.addEventListener('change', function () {
                const val = slotSelect.value;
                if (!val) {
                    selectedSlot = null;
                } else {
                    const selOption = slotSelect.options[slotSelect.selectedIndex];
                    const startMins = parseInt(selOption.getAttribute('data-start'), 10);
                    const endMins = parseInt(selOption.getAttribute('data-end'), 10);
                    let label = selOption.getAttribute('data-label') || selOption.textContent;
                    label = label.replace(/^[🟢❌]\s*/, '').replace(/\s*\(Available\)|\s*\(Booked.*\)/, '').trim();
                    selectedSlot = {
                        start_mins: startMins,
                        end_mins: endMins,
                        label: label,
                        is_available: true
                    };
                }
                updateBookingSummary();
            });
        }

        dateInput.addEventListener('change', function () {
            if (dateInput.value) {
                loadAvailableSlots(dateInput.value);
            }
        });

        // Initial slot load
        if (dateInput.value) {
            loadAvailableSlots(dateInput.value);
        }
        updateBookingSummary();

        // Attach listener to confirm button if present
        if (bookBtn) {
            bookBtn.onclick = handleBookingSubmit;
        }
    }

    // ------------------------------------------------------------------------
    // 2. Patient Portal: Render My Lab Bookings Table
    // ------------------------------------------------------------------------
    async function loadPatientBookings(patientId) {
        const tbody = document.getElementById('patient-lab-bookings-tbody');
        if (!tbody) return;

        let pid = patientId;
        if (!pid) {
            try {
                if (window.AuthNotificationModule) {
                    const u = window.AuthNotificationModule.getCurrentUser();
                    if (u && u.id) pid = u.id;
                }
                if (!pid) {
                    const raw = localStorage.getItem('auth_user') || localStorage.getItem('user');
                    if (raw) {
                        const parsed = JSON.parse(raw);
                        if (parsed.id) pid = parsed.id;
                    }
                }
            } catch (ignore) {}
        }
        if (!pid) pid = 2; // Default fallback

        try {
            const res = await fetch('/api/lab/my-bookings?patient_id=' + pid);
            const data = await res.json();
            let bookings = data.bookings || [];

            // If no bookings found for this specific pid, also check fallback pid 2
            if (bookings.length === 0 && pid !== 2) {
                try {
                    const fallbackRes = await fetch('/api/lab/my-bookings?patient_id=2');
                    const fallbackData = await fallbackRes.json();
                    if (fallbackData.bookings && fallbackData.bookings.length > 0) {
                        bookings = fallbackData.bookings;
                    }
                } catch (ignore) {}
            }

            if (bookings.length === 0) {
                tbody.innerHTML = `
                    <tr>
                        <td colspan="5" style="text-align: center; padding: 24px 16px; color: var(--text-muted); background: #fafafa; border-radius: 8px;">
                            <div style="font-size: 26px; margin-bottom: 6px;">📋</div>
                            <strong style="color: var(--text-main); font-size: 0.95rem;">No Scheduled Lab Tests Yet</strong>
                            <p style="margin: 4px 0 0; font-size: 0.82rem; color: var(--text-muted);">
                                Select a diagnostic test and timing slot above to book your collection appointment.
                            </p>
                        </td>
                    </tr>
                `;
                return;
            }

            tbody.innerHTML = '';
            bookings.forEach(b => {
                const tr = document.createElement('tr');
                tr.innerHTML = `
                    <td><strong>#${b.id}</strong></td>
                    <td>
                        <strong>${b.test_name}</strong>
                        <div style="font-size:11px; color:var(--text-muted);">${b.sample_type} Sample ${b.fasting_required ? '| <span style="color:#dc2626; font-weight:700;">Fasting Required</span>' : ''}</div>
                    </td>
                    <td>
                        <div>📅 ${b.booking_date}</div>
                        <div style="font-size:12px; font-weight:600; color:var(--primary-dark);">⏰ ${b.slot_label}</div>
                    </td>
                    <td>
                        <span class="badge ${b.status === 'Completed' ? 'badge-low' : b.status === 'Sample Collected' ? 'badge-moderate' : 'badge-high'}">
                            ${b.status}
                        </span>
                    </td>
                    <td>
                        <span style="font-size:11px; color:var(--text-muted);">${b.created_at || 'Recent'}</span>
                    </td>
                `;
                tbody.appendChild(tr);
            });
        } catch (err) {
            tbody.innerHTML = '<tr><td colspan="5" style="color:#ef4444; padding:12px;">Failed to load bookings: ' + err.message + '</td></tr>';
        }
    }

    // ------------------------------------------------------------------------
    // 3. ASHA Worker Dashboard: Phlebotomy Triage Queue (Ordered by Max-Heap)
    // ------------------------------------------------------------------------
    async function loadWorkerPhlebotomyQueue() {
        const tbody = document.getElementById('worker-lab-queue-tbody');
        const totalPill = document.getElementById('lab-kpi-total');
        const critPill = document.getElementById('lab-kpi-critical');
        const fastPill = document.getElementById('lab-kpi-fasting');

        if (!tbody) return;

        tbody.innerHTML = '<tr><td colspan="6" style="text-align:center; padding:16px; color:var(--text-muted);">Loading prioritized sample collection queue...</td></tr>';

        try {
            const auth = window.AuthNotificationModule;
            const res = await fetch('/api/worker/lab/triage-queue', {
                headers: auth ? auth.authHeaders() : {}
            });
            if (auth && auth.handleSessionExpired(res)) return;

            const data = await res.json();
            const queue = data.queue || [];

            if (totalPill) totalPill.textContent = data.total_pending_samples || 0;
            if (critPill) critPill.textContent = data.critical_priority_count || 0;
            if (fastPill) fastPill.textContent = data.fasting_samples_count || 0;

            if (queue.length === 0) {
                tbody.innerHTML = '<tr><td colspan="6" style="text-align:center; padding:16px; color:var(--text-muted);">No pending lab samples to collect in your area.</td></tr>';
                return;
            }

            tbody.innerHTML = '';
            queue.forEach(item => {
                const tr = document.createElement('tr');
                const isCritical = item.priority_score >= 80;

                tr.innerHTML = `
                    <td>
                        <div style="font-size:16px; font-weight:800; color:${isCritical ? '#dc2626' : '#5b65dc'};">
                            ${item.priority_score}
                        </div>
                        <span class="badge ${isCritical ? 'badge-critical' : 'badge-low'}" style="font-size:10px;">
                            ${isCritical ? 'Urgent' : 'Routine'}
                        </span>
                    </td>
                    <td>
                        <strong>${item.patient_name}</strong>
                        <span class="user-pill" style="font-size:11px; background:var(--primary-light); color:var(--primary-dark); padding:1px 6px; border-radius:4px;">ID: #${item.patient_id}</span>
                        <div style="font-size:12px; color:var(--text-muted);">📞 ${item.phone} | ${item.village_area}</div>
                    </td>
                    <td>
                        <strong>${item.test_name}</strong>
                        <div style="font-size:11px; color:var(--text-muted);">
                            Sample: <strong>${item.sample_type}</strong> ${item.fasting_required ? '• <strong style="color:#b91c1c;">Fasting Required</strong>' : ''}
                        </div>
                    </td>
                    <td>
                        <div>📅 ${item.booking_date}</div>
                        <div style="font-size:12px; font-weight:700; color:var(--primary-dark);">⏰ ${item.slot_label}</div>
                    </td>
                    <td>
                        <span class="badge ${item.status === 'Sample Collected' ? 'badge-moderate' : 'badge-high'}">
                            ${item.status}
                        </span>
                    </td>
                    <td>
                        ${item.status !== 'Sample Collected' ? `
                            <button class="btn btn-sm btn-primary action-collect-btn" style="padding:4px 8px; font-size:12px;">
                                ✓ Collect Sample
                            </button>
                        ` : '<span style="font-size:12px; color:#16a34a; font-weight:700;">✓ Sample Collected</span>'}
                    </td>
                `;

                const collectBtn = tr.querySelector('.action-collect-btn');
                if (collectBtn) {
                    collectBtn.addEventListener('click', async function () {
                        collectBtn.disabled = true;
                        collectBtn.textContent = 'Updating...';
                        try {
                            const patchRes = await fetch(`/api/worker/lab/${item.id}/status`, {
                                method: 'POST',
                                headers: auth ? auth.authHeaders() : {},
                                body: JSON.stringify({ status: 'Sample Collected' })
                            });
                            if (patchRes.ok) {
                                loadWorkerPhlebotomyQueue();
                            } else {
                                alert('Failed to update sample status.');
                                collectBtn.disabled = false;
                                collectBtn.textContent = '✓ Collect Sample';
                            }
                        } catch (e) {
                            alert('Network error updating status.');
                            collectBtn.disabled = false;
                        }
                    });
                }

                tbody.appendChild(tr);
            });
        } catch (err) {
            tbody.innerHTML = '<tr><td colspan="6" style="color:#ef4444; padding:12px;">Failed to load queue: ' + err.message + '</td></tr>';
        }
    }

    // Expose functions globally on window.LabBookingModule immediately
    window.LabBookingModule = window.LabBookingModule || {};
    window.LabBookingModule.submitBooking = handleBookingSubmit;
    window.LabBookingModule.initPatientLabBooking = initPatientLabBooking;
    window.LabBookingModule.loadPatientBookings = loadPatientBookings;
    window.LabBookingModule.loadWorkerPhlebotomyQueue = loadWorkerPhlebotomyQueue;

    // Delegated click event listener as a fail-safe backup for button clicks
    document.addEventListener('click', function (e) {
        if (e.target && (e.target.id === 'btn-confirm-lab-book' || e.target.closest('#btn-confirm-lab-book'))) {
            handleBookingSubmit(e);
        }
    });

    // Initialize based on current page
    function initModule() {
        const path = window.location.pathname;
        const isPatient = path.includes('patient') || document.getElementById('btn-confirm-lab-book') !== null;
        const isWorker = path.includes('worker') || document.getElementById('worker-lab-queue-tbody') !== null;

        if (isPatient) {
            initPatientLabBooking();
            let pid = 2;
            if (window.AuthNotificationModule && typeof window.AuthNotificationModule.getCurrentUser === 'function') {
                const u = window.AuthNotificationModule.getCurrentUser();
                if (u && u.id) pid = u.id;
            }
            loadPatientBookings(pid);
        } else if (isWorker) {
            loadWorkerPhlebotomyQueue();
        }
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initModule);
    } else {
        initModule();
    }
})();
