// Runs ADSRtek offline at 96 kHz and writes ENV / 10 V as float32.
// vcvrun shape A D S(0-127) R events seconds out.f32 [steps 0|1] [mode 0|1]
#include "AdsrTek.cpp"	// built with -I <UZZ-VCV-RACK>/src
Plugin* pluginInstance = nullptr;
#include <cstdio>
#include <vector>
int main(int argc, char** argv) {
    AdsrTek m;
    m.params[AdsrTek::SHAPE_PARAM].setValue(atof(argv[1]));
    m.params[AdsrTek::ATTACK_PARAM].setValue(atof(argv[2]));
    m.params[AdsrTek::DECAY_PARAM].setValue(atof(argv[3]));
    float s = atof(argv[4]);
    m.params[AdsrTek::SUSTAIN_PARAM].setValue(s >= 127.f ? 1.f : s / 128.f);
    m.params[AdsrTek::RELEASE_PARAM].setValue(atof(argv[5]));
    if (argc > 9) m.controlRateSteps = atoi(argv[9]);
    if (argc > 10) m.params[AdsrTek::MODE_PARAM].setValue(atof(argv[10]));
    if (argc > 11) m.gateCutsAttack = atoi(argv[11]);
    std::vector<std::pair<double, bool>> ev;
    for (const char* c = argv[6]; *c;) {
        ev.emplace_back(atof(c + 1), *c == '+');
        while (*c && *c != ',') ++c;
        if (*c == ',') ++c;
    }
    double secs = atof(argv[7]);
    Module::ProcessArgs a; a.sampleRate = 96000; a.sampleTime = 1.f / 96000; a.frame = 0;
    m.inputs[AdsrTek::GATE_INPUT].channels = 1;
    std::vector<float> out; bool g = false; size_t e = 0;
    for (long i = 0; i < (long)(secs * 96000); i++) {
        double t = i / 96000.0;
        while (e < ev.size() && t >= ev[e].first) { g = ev[e].second; e++; }
        m.inputs[AdsrTek::GATE_INPUT].setVoltage(g ? 10.f : 0.f);
        m.process(a);
        out.push_back(m.outputs[AdsrTek::ENV_OUTPUT].getVoltage() / 10.f);
    }
    FILE* f = fopen(argv[8], "wb");
    fwrite(out.data(), sizeof(float), out.size(), f);
    fclose(f);
    return 0;
}
