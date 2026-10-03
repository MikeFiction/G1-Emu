// Runs FILTERtek offline at 96 kHz on a float32 input (G1 full scale = 1.0) and writes the output
// in the same units. frun type slope gaincontrol freq res in.f32 out.f32
#include "FilterTek.cpp"	// built with -I <UZZ-VCV-RACK>/src
Plugin* pluginInstance = nullptr;
#include <cstdio>
#include <vector>
int main(int argc, char** argv) {
    if (argc < 8) return 1;
    FilterTek m;
    m.params[FilterTek::TYPE_PARAM].setValue(atof(argv[1]));
    m.params[FilterTek::SLOPE_PARAM].setValue(atof(argv[2]));
    m.params[FilterTek::GAIN_CONTROL_PARAM].setValue(atof(argv[3]));
    m.params[FilterTek::FREQ_PARAM].setValue(atof(argv[4]));
    m.params[FilterTek::RES_PARAM].setValue(atof(argv[5]));
    FILE* f = fopen(argv[6], "rb"); std::vector<float> in;
    float v; while (fread(&v, sizeof v, 1, f) == 1) in.push_back(v); fclose(f);
    Module::ProcessArgs a; a.sampleRate = 96000; a.sampleTime = 1.f / 96000; a.frame = 0;
    m.inputs[FilterTek::IN_L_INPUT].channels = 1;
    std::vector<float> out;
    for (float x : in) {
        m.inputs[FilterTek::IN_L_INPUT].setVoltage(x * 20.f);
        m.process(a);
        out.push_back(m.outputs[FilterTek::OUT_L_OUTPUT].getVoltage() / 20.f);
    }
    f = fopen(argv[7], "wb"); fwrite(out.data(), sizeof(float), out.size(), f); fclose(f);
    return 0;
}
