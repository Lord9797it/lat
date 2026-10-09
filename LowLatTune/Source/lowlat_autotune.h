#pragma once
// Pitch corrector a bassa latenza, scritto da zero con algoritmi pubblici:
//  - rilevamento pitch YIN (de Cheveigne & Kawahara, 2002) su segnale decimato x4
//  - pitch shifter a linea di ritardo con due tap, sincrono col periodo
// Nessun codice di terzi. Real-time safe in process(): niente allocazioni.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

class LowLatAutoTune {
public:
    // hopSamples: ogni quanti campioni si aggiorna il pitch (piu' basso = reazione piu' rapida)
    void prepare(double sampleRate, float minHz = 70.f, int hopSamples = 32) {
        sr = sampleRate;
        hop = std::max(8, hopSamples);
        minF = minHz;
        srD = sr / kDec;
        tauMax = (int)std::ceil(srD / minHz);
        scratch.assign(2 * tauMax + 2, 0.f);
        cmnd.assign(tauMax + 2, 1.f);
        ring.assign(1 << 13, 0.f);   rmask = (int)ring.size() - 1;
        dring.assign(1 << 12, 0.f);  dmask = (int)dring.size() - 1;
        allowed.fill(true);
        w = 0; dw = 0; dCount = 0; decCount = 0; hopCount = 0; acc = 0.f;
        p = 0.25f; curNote = -1; wasVoiced = false;
        logRatio = 0.f; ratioTarget = ratioSm = 1.f;
        periodSm = (float)(sr / 200.0);
        Ltarget = Lsm = kPeriods * periodSm;
        setRetuneMs(0.f);
    }

    // 12 note abilitate (0 = Do, 1 = Do#, ...). Tutte true = cromatico.
    void setScale(const std::array<bool, 12>& s) { allowed = s; }

    // 0 = effetto "hard tune" istantaneo; ~20-50 ms = correzione piu' naturale
    void setRetuneMs(float ms) {
        retuneMs = ms;
        alpha = ms <= 0.f ? 1.f : 1.f - std::exp(-(float)hop * 1000.f / ((float)sr * ms));
    }

    float process(float x) {
        ring[w & rmask] = x;
        acc += x;
        if (++decCount == kDec) {
            dring[dw++ & dmask] = acc / kDec;
            ++dCount; acc = 0.f; decCount = 0;
        }
        if (++hopCount >= hop) { hopCount = 0; analyse(); }

        ratioSm += 0.06f * (ratioTarget - ratioSm);   // ~16 campioni: niente zipper
        Lsm += 0.002f * (Ltarget - Lsm);

        p += (1.f - ratioSm) / Lsm;
        p -= std::floor(p);
        float pa = p, pb = p + 0.5f;
        if (pb >= 1.f) pb -= 1.f;
        float ya = readFrac(pa * Lsm + 2.f);
        float yb = readFrac(pb * Lsm + 2.f);
        float ga = 0.5f - 0.5f * std::cos(6.2831853f * pa);
        float gb = 0.5f - 0.5f * std::cos(6.2831853f * pb);
        ++w;
        return ya * ga + yb * gb;
    }

    // latenza algoritmica media in campioni (~ un periodo della nota)
    float latencySamples() const { return 0.5f * Lsm + 2.f; }

private:
    static constexpr int kDec = 4;
    static constexpr float kPeriods = 2.f;   // lunghezza finestra di shifting = 2 periodi
    static constexpr float kThr = 0.15f;     // soglia YIN
    static constexpr float kHyst = 0.3f;     // isteresi in semitoni

    float readFrac(float delay) const {
        int di = (int)delay;
        float t = delay - (float)di;
        int idx = w - di;
        float ym1 = ring[(idx + 1) & rmask], y0 = ring[idx & rmask];
        float y1 = ring[(idx - 1) & rmask], y2 = ring[(idx - 2) & rmask];
        float c1 = 0.5f * (y1 - ym1);
        float c2 = ym1 - 2.5f * y0 + 2.f * y1 - 0.5f * y2;
        float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }

    bool detect(float& periodD) {
        const int W = tauMax, n = W + tauMax;
        if (dCount < n) return false;
        for (int k = 0; k < n; ++k) scratch[k] = dring[(dw - n + k) & dmask];
        cmnd[0] = 1.f;
        float run = 0.f;
        for (int tau = 1; tau <= tauMax; ++tau) {
            float s = 0.f;
            for (int j = 0; j < W; ++j) { float d = scratch[j] - scratch[j + tau]; s += d * d; }
            run += s;
            cmnd[tau] = run > 1e-6f ? s * (float)tau / run : 1.f;
        }
        int tau = -1;
        for (int t = 2; t < tauMax; ++t) {
            if (cmnd[t] < kThr) {
                while (t + 1 < tauMax && cmnd[t + 1] < cmnd[t]) ++t;
                tau = t; break;
            }
        }
        if (tau < 0) return false;
        float a = cmnd[tau - 1], b = cmnd[tau], c = cmnd[tau + 1];
        float den = a - 2.f * b + c;
        periodD = (float)tau + (std::fabs(den) > 1e-9f ? 0.5f * (a - c) / den : 0.f);
        return true;
    }

    int nearestAllowed(float midi) const {
        int base = (int)std::floor(midi), best = -1;
        float bd = 1e9f;
        for (int n = base - 6; n <= base + 7; ++n) {
            if (!allowed[((n % 12) + 12) % 12]) continue;
            float d = std::fabs(midi - (float)n);
            if (d < bd) { bd = d; best = n; }
        }
        return best;
    }

    void analyse() {
        float pD;
        if (detect(pD)) {
            float period = pD * kDec;
            if (!wasVoiced) { periodSm = period; Lsm = kPeriods * period; }
            else periodSm += 0.3f * (period - periodSm);
            wasVoiced = true;
            float lo = (float)(sr / 1500.0), hi = (float)(sr / minF);
            periodSm = std::min(std::max(periodSm, lo), hi);
            Ltarget = kPeriods * periodSm;

            float f0 = (float)sr / period;
            float midi = 69.f + 12.f * std::log2(f0 / 440.f);
            int cand = nearestAllowed(midi);
            if (cand >= 0) {
                if (curNote < 0 || !allowed[((curNote % 12) + 12) % 12]) curNote = cand;
                else if (cand != curNote && std::fabs(midi - (float)cand) + kHyst < std::fabs(midi - (float)curNote))
                    curNote = cand;
                float targetLog = ((float)curNote - midi) / 12.f;   // in ottave
                logRatio += alpha * (targetLog - logRatio);
            }
        } else {
            wasVoiced = false; curNote = -1;
            logRatio += alpha * (0.f - logRatio);
        }
        ratioTarget = std::exp2(logRatio);
    }

    double sr = 48000, srD = 12000;
    int hop = 32, tauMax = 171;
    float minF = 70.f, retuneMs = 0.f, alpha = 1.f;
    std::vector<float> ring, dring, scratch, cmnd;
    int rmask = 0, dmask = 0, w = 0, dw = 0, dCount = 0, decCount = 0, hopCount = 0;
    float acc = 0.f, p = 0.25f, logRatio = 0.f, ratioTarget = 1.f, ratioSm = 1.f;
    float periodSm = 240.f, Ltarget = 480.f, Lsm = 480.f;
    int curNote = -1;
    bool wasVoiced = false;
    std::array<bool, 12> allowed;
};
