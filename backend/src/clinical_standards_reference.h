#pragma once
/**
 * ============================================================================
 * Module: Standard Clinical Reference Guidelines & Benchmarks
 * Grounding Framework: ICMR (Indian Council of Medical Research) & WHO NCD
 * Implemented by: Vivek Chamoli (AI Module)
 * ============================================================================
 * Description:
 *  - Provides authoritative clinical diagnostic benchmarks for non-communicable
 *    and chronic disease triage in Indian community healthcare.
 *  - Grounding Gemini Generative AI on standardized clinical knowledge prevents
 *    hallucinations, standardizes scoring bands, and guarantees clinical safety.
 *  - References:
 *     1. ICMR Standard Treatment Guidelines (Non-Communicable Diseases)
 *     2. WHO Package of Essential Noncommunicable (PEN) Disease Interventions
 *     3. National Programme for Prevention & Control of Cancer, Diabetes,
 *        Cardiovascular Diseases & Stroke (NPCDCS - India)
 * ============================================================================
 */

#include <string>

namespace health {

// ----------------------------------------------------------------------------
// Clinical Reference Guidelines Matrix (ICMR / WHO Triage Benchmarks)
// ----------------------------------------------------------------------------
inline std::string get_clinical_reference_prompt_section() {
    return R"(
[AUTHORITATIVE CLINICAL REFERENCE DATASET & TRIAGE BENCHMARKS (ICMR & WHO PEN)]
You must evaluate the patient strictly against these standardized medical evidence benchmarks:

1. CARDIOVASCULAR & HYPERTENSION PROTOCOLS (ICMR/WHO CVD Triage):
   - Key Risk Indicators: Central chest tightness/pressure, radiation to left arm/jaw, exertional dyspnea (breathlessness on climbing stairs/walking), orthopnea, bilateral pedal edema, history of chronic hypertension.
   - Scoring Threshold: Risk score 80-100 ('Critical') if chest pain radiates or rest dyspnea is present; 65-79 ('High') for isolated exertional angina/hypertension history.
   - Standard Specialist Referral: "Cardiologist".
   - Critical Red Flag: Immediate ECG and cardiac enzyme evaluation advised within 60 minutes.

2. METABOLIC & TYPE 2 DIABETES PROTOCOLS (ICMR Diabetes Care Guidelines):
   - Key Risk Indicators: Polydipsia (excessive thirst), polyuria (frequent urination, especially nocturia), polyphagia (frequent hunger), unexplained weight loss, slow-healing peripheral sores/ulcers, tingling/numbness in feet (peripheral neuropathy).
   - Scoring Threshold: Risk score 60-79 ('High') for classic osmotic symptoms; 80-100 ('Critical') if severe fatigue, altered sensorium, or acute infection is present.
   - Standard Specialist Referral: "Endocrinologist / Diabetologist".
   - Key Actions: Fasting Blood Glucose (FBG), Postprandial Glucose, HbA1c testing, diabetic foot inspection.

3. RESPIRATORY & PULMONARY PROTOCOLS (WHO PEN & National Tuberculosis Elimination Guidelines):
   - Key Risk Indicators: Chronic cough > 2 weeks, hemoptysis (coughing blood), nighttime low-grade fever, unexplained weight loss, wheezing, progressive breathlessness.
   - Scoring Threshold: Risk score 80-100 ('Critical') if hemoptysis or severe resting stridor/dyspnea is present; 60-75 ('High') for persistent productive cough > 2 weeks.
   - Standard Specialist Referral: "Pulmonologist".
   - Mandatory Public Health Rule: Sputum smear microscopy / CBNAAT for tuberculosis screening if cough > 2 weeks in community.

4. NEUROLOGICAL & CEREBROVASCULAR PROTOCOLS (WHO FAST Stroke Algorithm):
   - Key Risk Indicators: Facial asymmetry/droop, unilateral arm/leg weakness, slurred speech, sudden loss of balance, sudden thunderclap headache, transient vision darkening.
   - Scoring Threshold: Risk score 85-100 ('Critical').
   - Standard Specialist Referral: "Neurologist / Emergency Medicine".
   - Critical Red Flag: Golden-hour emergency transport for CT/MRI scan and thrombolysis assessment.

5. RENAL, HEPATIC & DIGESTIVE PROTOCOLS:
   - Key Risk Indicators: Periorbital facial puffiness, oliguria (reduced urine volume), generalized edema, yellowing of sclera/jaundice, severe right upper quadrant abdominal tenderness.
   - Scoring Threshold: Risk score 65-80 ('High' to 'Critical').
   - Standard Specialist Referral: "Nephrologist" or "Gastroenterologist".

6. MUSCULOSKELETAL & ARTHRITIS PROTOCOLS:
   - Key Risk Indicators: Morning joint stiffness > 45 minutes, symmetrical small joint swelling, difficulty with fine motor grip.
   - Scoring Threshold: Risk score 40-59 ('Moderate').
   - Standard Specialist Referral: "Rheumatologist / Orthopedic".

7. STANDARDIZED CLINICAL RISK SCORING BANDS (ICMR Triage System):
   - 0 - 39: 'Low' Risk -> Routine annual community screening, lifestyle counseling, balanced diet.
   - 40 - 59: 'Moderate' Risk -> Community Health Centre review within 7 days, baseline lab investigations.
   - 60 - 79: 'High' Risk -> Specialist consultation within 48-72 hours, bi-weekly ASHA worker home check.
   - 80 - 100: 'Critical' Risk -> Immediate tertiary referral / emergency ambulance dispatch, immediate ASHA alert.
)";
}

} // namespace health
