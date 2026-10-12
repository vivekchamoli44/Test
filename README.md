# Medico — AI-Powered Early Health Detection Center

A community disease-prevention and ASHA (Accredited Social Health Activist)
worker triage system. Patients record their symptoms and lifestyle history
through a web portal; the backend sends that data to **Google Gemini**,
which returns a structured clinical risk assessment (risk score, possible
chronic diseases, recommended specialist, precautions, and warning signs
to watch). ASHA workers get a dashboard of all patients sorted by risk,
so the highest-risk individuals in the community are triaged first.

## Tech Stack

| Layer      | Technology |
|------------|------------|
| Backend    | C++17, [Crow](https://crowcpp.org/) web framework |
| AI Engine  | Google Gemini API (`generateContent`, free-tier models only) |
| Database   | MySQL 8.0 (with an automatic in-memory fallback if MySQL isn't reachable) |
| Frontend   | Plain HTML, CSS, JavaScript (no framework, no build step) |
| HTTP calls to Gemini | Native Windows WinHTTP |

The backend is written **entirely in C++** — there is no Python, Node, or
other backend runtime anywhere in the project. The only JavaScript is
client-side browser code in `frontend/js/`.

## Project Structure

```
backend/
  src/
    main.cpp              Server entry point, config loading, route wiring
    models.h               User / Assessment / AIResult data structures
    auth_notification.h    Module 1
    ai_assessment.h         Module 2
    db_history.h            Module 3
    dashboard_triage.h       Module 4
  config.json             Server port, MySQL credentials, Gemini key (fallback)
  schema.sql              MySQL table definitions
  build.bat / run.bat     Build & run scripts (Windows, g++/MinGW)
  .env                    GEMINI_API_KEY and other secrets (not committed)
frontend/
  index.html              Login / Sign Up page
  patient.html            Patient dashboard page
  worker.html             ASHA worker dashboard page
  css/style.css
  js/
    auth_notification.js
    ai_assessment.js
    db_history.js
    dashboard_triage.js
```

Each backend header and frontend script pairs up by name with the module
it implements, so `auth_notification.h` ↔ `auth_notification.js`, and so on.

The frontend is three separate HTML pages rather than a single-page app:
visiting `/` serves the login page, and after signing in the browser
navigates to a real URL — `/patient.html` or `/worker.html` depending on
role. Each dashboard page guards itself on load (via `auth_notification.js`):
an unauthenticated visitor is sent back to the login page, and a patient
can't land on the worker page (or vice versa) — they're redirected to
their own dashboard instead.

## Modules & Ownership

The system is split into four modules, each owned end-to-end (API route +
frontend logic) by one team member:

### 1. Login, Sign Up & ASHA Worker: Send Notification
**Implemented by: Yashwardhan Rohilla**
Files: [`backend/src/auth_notification.h`](backend/src/auth_notification.h), [`frontend/js/auth_notification.js`](frontend/js/auth_notification.js)

- Patient and ASHA Worker registration (`POST /api/auth/register`) and login
  (`POST /api/auth/login`), with Gmail-address validation.
- Session token issuance and validation (`GET /api/auth/me`,
  `POST /api/auth/logout`).
- ASHA Worker action: dispatches a high-risk emergency alert notification
  to a patient/health sub-centre
  (`POST /api/worker/assessments/<id>/alert`).

### 2. AI Integration with Gemini & Patient: Start New Assessment
**Implemented by: Vivek Chamoli**
Files: [`backend/src/ai_assessment.h`](backend/src/ai_assessment.h), [`frontend/js/ai_assessment.js`](frontend/js/ai_assessment.js)

- Patient-facing "Start New Assessment" flow: collects reported symptoms,
  symptom duration/severity, and family/lifestyle history from the modal
  form (no manual vitals entry required).
- Builds a clinical prompt and sends it to the **Google Gemini API**
  (`POST /api/assessments`). Gemini is the sole diagnostic engine — there
  is no offline/rule-based fallback; if the API key is missing or every
  model attempt fails, the endpoint reports the failure (`503`) instead
  of fabricating a result.
- Tries a sequence of **free-tier** Gemini models (`gemini-flash-lite-latest`
  → `gemini-2.5-flash-lite` → `gemini-2.5-flash`), retrying a model
  briefly if it's momentarily overloaded, and moving to the next model
  immediately if its daily quota is exhausted.
- Parses Gemini's structured JSON reply into: `risk_score` (0–100),
  `risk_level` (Low/Moderate/High/Critical), `possible_chronic_diseases`,
  `recommended_doctor_specialty`, `other_symptoms_to_watch`,
  `precautions`, and `immediate_actions`.

### 3. Database and Testing & Patient: Show Previous Assessment
**Implemented by: Anjali**
Files: [`backend/src/db_history.h`](backend/src/db_history.h), [`frontend/js/db_history.js`](frontend/js/db_history.js)

- MySQL 8.0 connection, database/table auto-migration, and an in-memory
  storage fallback so the app stays fully functional if MySQL isn't
  available.
- Automated database integrity testing endpoint (`GET /api/db/test`).
- Patient-facing "Show Previous Assessments" history
  (`GET /api/assessments/my?patient_id=<id>`).

### 4. Patient & ASHA Worker Dashboard & ASHA Worker: High Risk Individual
**Implemented by: Nandini**
Files: [`backend/src/dashboard_triage.h`](backend/src/dashboard_triage.h), [`frontend/js/dashboard_triage.js`](frontend/js/dashboard_triage.js)

- ASHA Worker dashboard: fetches **real-time** patient assessments
  (`GET /api/worker/assessments`), sorted strictly from highest to lowest
  risk score, with live KPI counts (total screenings, critical-risk count,
  high-risk count, alerts dispatched). No seeded/demo data — only
  assessments patients have actually submitted appear here.
- Search and filter by patient name, area, or risk level.
- Contact Patient / outreach logging
  (`POST /api/worker/assessments/<id>/contact`).

## Setup & Running

### Prerequisites
- Windows with a MinGW-w64 g++ toolchain (the project was built against
  WinLibs POSIX/UCRT g++)
- MySQL 8.0 (optional — the server falls back to in-memory storage if it
  can't connect)
- A Google Gemini API key ([ai.google.dev](https://ai.google.dev))

### Configure
Copy the example config and fill in your own values — `backend/.env` and
`backend/config.json` hold real secrets, so they're git-ignored and must
never be committed:

```bash
cd backend
cp .env.example .env            # or: cp config.example.json config.json
# then edit .env / config.json and set your GEMINI_API_KEY, MySQL creds, etc.
```

```
GEMINI_API_KEY=your_key_here
SERVER_PORT=8080
MYSQL_HOST=127.0.0.1
MYSQL_PORT=3306
MYSQL_USER=root
MYSQL_PASSWORD=
MYSQL_DATABASE=ai_health_center
```

### Build & Run
```bash
cd backend
build.bat   # compiles src/main.cpp -> server.exe
run.bat     # builds (if needed) and starts the server
```

Then open **http://localhost:8080** in a browser.

## API Summary

| Method | Route | Module |
|--------|-------|--------|
| POST | `/api/auth/register` | 1 — Sign Up |
| POST | `/api/auth/login` | 1 — Login |
| GET  | `/api/auth/me` | 1 — Session check |
| POST | `/api/auth/logout` | 1 — Logout |
| POST | `/api/worker/assessments/<id>/alert` | 1 — Send Notification |
| POST | `/api/assessments` | 2 — Start New Assessment (Gemini AI) |
| GET  | `/api/assessments/my?patient_id=<id>` | 3 — Show Previous Assessments |
| GET  | `/api/db/test` | 3 — Database Testing |
| GET  | `/api/worker/assessments` | 4 — High-Risk Dashboard |
| POST | `/api/worker/assessments/<id>/contact` | 4 — Contact Patient |
| GET  | `/api/health` | System health check |

## Disclaimer

This is a triage and early-warning support tool, not a diagnostic device.
AI-generated risk assessments are meant to help ASHA workers prioritize
outreach and should always be followed up with a qualified clinician.
