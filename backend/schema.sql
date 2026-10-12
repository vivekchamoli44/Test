-- AI-Powered Early Health Detection Center
-- MySQL Schema Definition

CREATE DATABASE IF NOT EXISTS ai_health_center CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE ai_health_center;

-- Users Table (Patients and ASHA Healthcare Workers)
CREATE TABLE IF NOT EXISTS users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(100) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('patient', 'asha_worker') NOT NULL,
    full_name VARCHAR(150) NOT NULL,
    phone VARCHAR(20) NOT NULL,
    village_area VARCHAR(150) NOT NULL,
    age INT DEFAULT 0,
    gender VARCHAR(20) DEFAULT 'Not Specified',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Health Assessments Table
CREATE TABLE IF NOT EXISTS assessments (
    id INT AUTO_INCREMENT PRIMARY KEY,
    patient_id INT NOT NULL,
    patient_name VARCHAR(150) NOT NULL,
    age INT NOT NULL,
    gender VARCHAR(20) NOT NULL,
    phone VARCHAR(20) NOT NULL,
    village_area VARCHAR(150) NOT NULL,
    
    -- Clinical Biometrics / Vitals
    bp_systolic INT DEFAULT 120,
    bp_diastolic INT DEFAULT 80,
    blood_sugar INT DEFAULT 95,
    heart_rate INT DEFAULT 72,
    oxygen_level INT DEFAULT 98,
    temperature DECIMAL(4,1) DEFAULT 98.6,
    symptoms_text TEXT,
    medical_history TEXT,
    
    -- AI Generated Structured Predictions
    risk_score INT NOT NULL,               -- 0 to 100
    risk_level VARCHAR(30) NOT NULL,        -- 'Low', 'Moderate', 'High', 'Critical'
    possible_chronic_diseases TEXT,         -- JSON string array e.g. ["Type 2 Diabetes", "Hypertension"]
    recommended_doctor VARCHAR(100),        -- e.g. "Cardiologist", "Endocrinologist", "General Physician"
    other_symptoms_to_watch TEXT,           -- JSON string array e.g. ["Blurry vision", "Dizziness"]
    precautions TEXT,                       -- JSON string array or bullet points
    immediate_actions TEXT,                 -- Recommended next clinical steps
    
    -- ASHA Healthcare Alert Status
    alert_sent TINYINT(1) DEFAULT 0,
    alert_sent_at DATETIME NULL,
    alert_message TEXT NULL,
    asha_contacted TINYINT(1) DEFAULT 0,
    asha_notes TEXT NULL,
    
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (patient_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Alerts & Notifications Log
CREATE TABLE IF NOT EXISTS alert_logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    assessment_id INT NOT NULL,
    patient_id INT NOT NULL,
    worker_id INT NULL,
    alert_type VARCHAR(50) DEFAULT 'HIGH_RISK_HEALTH_WARNING',
    message TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (assessment_id) REFERENCES assessments(id) ON DELETE CASCADE,
    FOREIGN KEY (patient_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Insert Default Demo Accounts if not existing
-- Passwords below are plain/simple hash for easy first login (admin/patient)
INSERT IGNORE INTO users (id, username, password_hash, role, full_name, phone, village_area, age, gender) VALUES
(1, 'asha1', 'asha123', 'asha_worker', 'Sunita Devi (ASHA Worker)', '+91 98765 43210', 'Rampur Ward 4', 38, 'Female'),
(2, 'patient1', 'patient123', 'patient', 'Ramesh Kumar', '+91 91234 56789', 'Rampur Ward 4', 54, 'Male'),
(3, 'patient2', 'patient123', 'patient', 'Geeta Sharma', '+91 98111 22334', 'Rampur Ward 2', 61, 'Female');

-- Seed Sample Assessments so ASHA dashboard immediately demonstrates high-to-low risk triage
INSERT IGNORE INTO assessments 
(id, patient_id, patient_name, age, gender, phone, village_area, bp_systolic, bp_diastolic, blood_sugar, heart_rate, oxygen_level, temperature, symptoms_text, medical_history, risk_score, risk_level, possible_chronic_diseases, recommended_doctor, other_symptoms_to_watch, precautions, immediate_actions, alert_sent)
VALUES
(1, 2, 'Ramesh Kumar', 54, 'Male', '+91 91234 56789', 'Rampur Ward 4', 165, 105, 240, 98, 93, 99.1, 'Shortness of breath on walking, frequent urination at night, severe tingling in feet', 'Smoker for 15 years, family history of stroke', 88, 'Critical', '["Severe Hypertension Stage 2", "Uncontrolled Type 2 Diabetes", "Cardiovascular Disease Risk"]', 'Cardiologist & Endocrinologist', '["Sudden chest heaviness", "Numbness in arms/face", "Blurry vision"]', '["Immediately restrict sodium and salt intake", "Avoid all refined sugars and carbohydrates", "Avoid strenuous physical exertion until cleared"]', 'Urgent in-person clinical evaluation required within 24 hours. Measure fasting blood sugar and ECG immediately.', 0),

(2, 3, 'Geeta Sharma', 61, 'Female', '+91 98111 22334', 'Rampur Ward 2', 145, 92, 175, 82, 96, 98.6, 'Persistent fatigue, excessive thirst, swollen ankles in evening', 'Hypertension diagnosed 3 years ago', 68, 'High', '["Type 2 Diabetes Mellitus", "Hypertensive Heart Disease"]', 'Endocrinologist / General Physician', '["Swelling in lower extremities", "Extreme thirst", "Slow healing cuts"]', '["Follow diabetic diet plan", "Regular morning blood pressure monitoring", "Elevate feet when resting to reduce swelling"]', 'Schedule routine lab tests for HbA1c, Kidney Function (Creatinine/eGFR), and lipid panel within 3-5 days.', 0);
