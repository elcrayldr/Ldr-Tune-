#include <iostream>
#include <cmath>
#include <vector>
#include "YinPitchDetector.h"
#include "ScaleQuantizer.h"
#include "WsolaPitchShifter.h"

int main()
{
    std::cout << "CRAY LDR REALTIME TUNE - DSP UNIT TESTS\n\n";

    int passed = 0, failed = 0;
    auto test = [&](const char* name, bool ok)
    {
        if (ok) { std::cout << "[PASS] " << name << "\n"; ++passed; }
        else    { std::cerr << "[FAIL] " << name << "\n"; ++failed; }
    };

    const double sampleRate = 48000.0;
    const float twoPi = 6.28318530718f;

    {   // 1. YIN con 440 Hz
        CrayDSP::YinPitchDetector d;
        d.prepare(sampleRate, 256);
        CrayDSP::YinPitchDetector::PitchResult r;
        for (int i = 0; i < 4096; ++i)
            r = d.processSample(0.5f * std::sin(twoPi * 440.0f * static_cast<float>(i) / static_cast<float>(sampleRate)));
        test("1. YIN: lock a 440 Hz (error < 3 Hz)", r.isVoiced && std::abs(r.frequencyHz - 440.0f) < 3.0f);
    }

    {   // 2. Silencio
        CrayDSP::YinPitchDetector d;
        d.prepare(sampleRate, 256);
        CrayDSP::YinPitchDetector::PitchResult r;
        for (int i = 0; i < 512; ++i) r = d.processSample(0.0f);
        test("2. Silencio: sin voicing y confianza 0", !r.isVoiced && r.confidence == 0.0f);
    }

    {   // 3. C menor natural: 305 Hz -> Eb
        CrayDSP::ScaleQuantizer q;
        q.setKey(0);
        q.setScaleType(CrayDSP::ScaleQuantizer::ScaleType::NaturalMinor);
        q.setScaleMode(CrayDSP::ScaleQuantizer::ScaleMode::Strict);
        auto r = q.quantize(305.0f, 0.95f, true);
        test("3. Cuantizador C menor: 305 Hz -> Eb (indice 3)", r.targetNoteIndex == 3);
    }

    {   // 4. Histeresis
        CrayDSP::ScaleQuantizer q;
        q.setKey(0);
        q.setScaleType(CrayDSP::ScaleQuantizer::ScaleType::Chromatic);
        q.setHysteresisCents(15.0f);
        q.quantize(440.0f, 0.9f, true);
        auto r = q.quantize(440.0f * std::pow(2.0f, 8.0f / 1200.0f), 0.9f, true);
        test("4. Histeresis: 8 cents retenidos en A4", r.targetNoteIndex == 9);
    }

    {   // 5. WSOLA estable
        CrayDSP::WsolaPitchShifter s;
        s.prepare(sampleRate, 256);
        s.setShiftRatio(1.05946f);
        bool ok = true;
        int crossings = 0;
        float previous = 0.0f;
        constexpr int measurementStart = 2048;
        constexpr int measurementSamples = 6144;
        for (int i = 0; i < measurementStart + measurementSamples; ++i)
        {
            const float out = s.processSample(std::sin(twoPi * 220.0f * static_cast<float>(i) / static_cast<float>(sampleRate)));
            if (!std::isfinite(out) || std::abs(out) > 3.0f) ok = false;
            if (i >= measurementStart && previous <= 0.0f && out > 0.0f) ++crossings;
            previous = out;
        }
        const float measuredHz = static_cast<float>(crossings) * static_cast<float>(sampleRate) / static_cast<float>(measurementSamples);
        test("5. Pitch shifter output remains finite and bounded", ok);
        std::cout << "[INFO] Pitch shifter measured frequency: " << measuredHz << " Hz (target ~233.08 Hz)" << std::endl;
        test("6. Pitch ratio shifts 220 Hz toward 233 Hz", std::abs(measuredHz - 220.0f * 1.05946f) < 8.0f);
        test("7. CLASSIC reports 32-sample fixed delay", s.getLatencySamples() == 32);
    }

    {   // 7. Modo MODERN: grano 192
        CrayDSP::WsolaPitchShifter s;
        s.prepare(sampleRate, 256);
        s.setGrainSize(192);
        s.setShiftRatio(0.95f);
        bool ok = true;
        for (int i = 0; i < 4096 && ok; ++i)
        {
            float out = s.processSample(std::sin(twoPi * 150.0f * static_cast<float>(i) / static_cast<float>(sampleRate)));
            if (!std::isfinite(out) || std::abs(out) > 3.0f) ok = false;
        }
        test("8. MODERN reports 96-sample fixed delay and stable output", ok && s.getLatencySamples() == 96);
    }

    std::cout << "\nRESULTADOS: " << passed << " pasados, " << failed << " fallidos\n";
    return failed == 0 ? 0 : 1;
}
