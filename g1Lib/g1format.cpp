#include "g1format.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <utility>

namespace g1
{
	namespace
	{
		// _decimals places, then the zeros at the end dropped (1.20 -> "1.2", 100.00 -> "100"),
		// as JavaScript's ""+n did in nmformat.js.
		std::string num(const double _v, const int _decimals, const bool _strip = true)
		{
			if(_decimals <= 0)
				return std::to_string(static_cast<int>(std::round(_v)));
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.*f", _decimals, _v);
			std::string s(buf);
			if(_strip)
			{
				while(!s.empty() && s.back() == '0')
					s.pop_back();
				if(!s.empty() && s.back() == '.')
					s.pop_back();
			}
			return s;
		}

		std::string str(const int _v) { return std::to_string(_v); }

		template<size_t N> std::string pick(const char* const (&_table)[N], const int _v)
		{
			return _v >= 0 && _v < static_cast<int>(N) ? std::string(_table[_v]) : str(_v);
		}

		// ____________________________________________________________________________________
		// The formatters, as NME's (the names are its own)

		std::string fmtPlus1(const int _v) { return str(_v + 1); }
		std::string fmtMinus64(const int _v) { return str(_v - 64); }

		std::string fmtBipolar64(const int _v)
		{
			const int v = _v - 64;
			return v > 0 ? "+" + str(v) : str(v);
		}

		std::string fmtBPM(const int _v)
		{
			return str(_v <= 32 ? 2 * _v + 24 : _v <= 96 ? _v + 56 : 2 * _v - 40) + " bpm";
		}

		std::string fmtEnvRefLevel(const int _v) { return str(_v - 30) + "dB"; }
		std::string fmtCompressorLimiter(const int _v) { return _v == 24 ? "Off" : str(_v - 12) + "dB"; }
		std::string fmtCompressorThreshold(const int _v) { return _v == 42 ? "Off" : str(_v - 30) + "dB"; }

		std::string fmtDigitizerHz(const int _v)
		{
			const double f = 32.70 * std::pow(2.0, _v / 12.0);
			if(f < 100)		return num(f, 2) + " Hz";
			if(f < 1000)	return num(f, 1) + " Hz";
			if(f < 10000)	return num(f / 1000.0, 3) + " kHz";
			return num(f / 1000.0, 2) + " kHz";
		}

		std::string fmtDrumHz(const int _v)
		{
			const double f = 20.0 * std::pow(2.0, _v / 24.0);
			return f < 100 ? num(f, 1) + " Hz" : num(f, 0) + " Hz";
		}

		std::string fmtDrumPartials(const int _v)
		{
			static const char* const fractions[] = {"1:1", "2:1", "4:1"};
			if(_v % 48 == 0)
				return pick(fractions, _v / 48);
			return "x" + num(std::pow(2.0, _v / 48.0), 2);
		}

		std::string fmtEnvelopeLevelDivider(const int _v) { return _v == 127 ? "64" : num(_v * 0.5, 1); }

		std::string fmtEqHz(const int _v)
		{
			const double f = 471.0 * std::pow(2.0, (_v - 60) / 12.0);
			if(f < 1000)	return num(f, 0) + " Hz";
			if(f < 10000)	return num(f / 1000.0, 2) + " kHz";
			return num(f / 1000.0, 1) + " kHz";
		}

		std::string fmtNoteVelScaleGain(const int _v) { return str(_v - 24) + "dB"; }
		std::string fmtNoteQuantNotes(const int _v) { return _v == 0 ? "OFF" : _v >= 127 ? "----" : str(_v); }
		std::string fmtPartialGen(const int _v) { return str(_v + 1); }
		std::string fmtExpanderGate(const int _v) { return _v == 0 ? "Off" : str(_v - 84) + "dB"; }
		std::string fmtExpanderHold(const int _v) { return _v == 0 ? "Off" : str(_v * 4) + "ms"; }
		std::string fmtExpanderThreshold(const int _v) { return _v == 84 ? "Off" : str(_v - 84) + "dB"; }

		std::string fmtFilterHz2(const int _v)
		{
			const double f = 330.0 * std::pow(2.0, (_v - 60) / 12.0);
			if(f < 1000)	return num(f, 0) + " Hz";
			if(f < 10000)	return num(f / 1000.0, 2) + " kHz";
			return num(f / 1000.0, 1) + " kHz";
		}

		std::string fmtFreqkbt(const int _v) { return _v == 0 ? "Off" : "x" + num(_v / 63.5, 2); }

		std::string fmtLFOHz(const int _v)
		{
			const double f = 440.0 * std::pow(2.0, (_v - 177) / 12.0);
			if(f < 0.1)	return num(1.0 / f, 1) + " s";
			if(f < 10)	return num(f, 2) + " Hz";
			if(f < 100)	return num(f, 1) + " Hz";
			return num(f, 0) + " Hz";
		}

		std::string fmtLogicDelay(const int _v)
		{
			const double f = std::pow(2.0, _v / 9.0);
			if(f < 1000)	return num(f, 0) + " ms";
			if(f < 10000)	return num(f / 1000.0, 1) + " s";
			return num(f / 1000.0, 0) + " s";
		}

		// As a musician writes it (C#4, with 60 = C4), as NME's sequencer notes: its fmtNote,
		// faithful to nmformat.js, puts the sharp after the octave.
		std::string fmtNote(const int _v)
		{
			static const char* const names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
			return std::string(names[((_v % 12) + 12) % 12]) + str(static_cast<int>(std::floor(_v / 12.0)) - 1);
		}

		std::string fmtNoteScale(const int _v)
		{
			if(_v == 0)		return "0 (Oct)";
			if(_v == 127)	return "+/-64";
			const int key = (_v / 2) % 12 + _v % 2;
			const char* suffix = key == 0 ? " (Oct)" : key == 7 ? " (5th)" : key == 10 ? " (7th)" : "";
			return "+/-" + num(_v / 2.0, 1, false) + suffix;
		}

		std::string fmtOffset64_2(const int _v) { return _v == 127 ? "64" : str(_v - 64); }

		std::string fmtPartials(const int _v)
		{
			static const char* const fractions[] = {"1:32", "1:16", "1:8", "1:4", "1:2", "1:1", "2:1", "4:1", "8:1", "16:1", "32:1"};
			if((_v + 8) % 12 == 0 && _v / 12 >= 0 && _v / 12 < 11)
				return pick(fractions, _v / 12);
			return "x" + num(std::pow(2.0, (_v - 64) / 12.0), 3);
		}

		std::string fmtPatternStep(const int _v) { return _v == 0 ? "Off" : str(_v + 1); }
		std::string fmtPhase(const int _v) { return num(_v * 2.8125 - 180.0, 0); }

		std::string fmtSemitones(const int _v)
		{
			const int v = _v - 64;
			const int key = ((v + 72) % 12 + 12) % 12;
			return str(v) + (key == 0 ? "(Oct)" : key == 7 ? "(5th)" : key == 10 ? "(7th)" : "");
		}

		std::string fmtSmoothTime(const int _v) { return fmtLogicDelay(_v); }	// the same curve
		std::string fmtTimbre(const int _v) { return _v == 127 ? "Rnd" : str(_v + 1); }
		std::string fmtPulseWidth(const int _v) { return num((_v + 1) * (98.0 / 127.0), 1) + "%"; }

		std::string fmtOffOn(const int _v) { static const char* const t[] = {"Off", "On"}; return pick(t, _v); }

		std::string fmtCompanderRelease(const int _v)
		{
			static const char* const t[] = {
				"125m","129m","124m","139m","144m","149m","154m","159m","165m","171m","177m","183m","189m",
				"196m","203m","210m","218m","225m","233m","241m","250m","259m","268m","277m","287m","297m",
				"208m","319m","330m","342m","354m","366m","379m","392m","406m","420m","435m","451m","467m",
				"483m","500m","518m","536m","555m","574m","595m","616m","637m","660m","683m","707m","732m",
				"758m","785m","812m","841m","871m","901m","933m","966m","1.00s","1.04s","1.07s","1.11s",
				"1.15s","1.19s","1.23s","1.27s","1.32s","1.37s","1.41s","1.46s","1.52s","1.57s","1.62s",
				"1.68s","1.74s","1.80s","1.87s","1.93s","2.00s","2.07s","2.14s","2.22s","2.30s","2.38s",
				"2.46s","2.55s","2.64s","2.73s","2.83s","2.93s","3.03s","3.14s","3.25s","3.36s","3.48s",
				"3.61s","3.73s","3.86s","4.00s","4.14s","4.29s","4.44s","4.59s","4.76s","4.92s","5.10s",
				"5.28s","5.46s","5.66s","5.86s","6.06s","6.28s","6.50s","6.73s","6.96s","7.21s","7.46s",
				"7.73s","8.00s","8.28s","8.57s","8.88s","9.19s","9.51s","9.85s","10.2s"};
			return pick(t, _v);
		}

		std::string fmtChannelSelectOut1(const int _v) { static const char* const t[] = {"1", "2", "3", "4", "L", "R"}; return pick(t, _v); }
		std::string fmtChannelSelectOut2(const int _v) { static const char* const t[] = {"1/2", "3/4", "CVA"}; return pick(t, _v); }

		std::string fmtEnvelopeRelease(const int _v)
		{
			static const char* const t[] = {
				"Fast","41.4m","42.9m","44.4m","45.9m","47.6m","49.2m","51.0m","52.8m","54.6m","56.6m","58.6m",
				"60.6m","62.8m","65.0m","67.3m","69.6m","72.1m","74.6m","77.3m","80.0m","82.8m","85.7m","88.8m",
				"91.9m","95.1m","98.5m","102m","106m","109m","113m","117m","121m","126m","130m","135m","139m",
				"144m","149m","155m","160m","166m","171m","178m","184m","190m","197m","204m","211m","219m",
				"226m","234m","243m","251m","260m","269m","279m","288m","299m","309m","320m","331m","343m",
				"355m","368m","381m","394m","408m","422m","437m","453m","469m","485m","502m","520m","538m",
				"557m","577m","597m","618m","640m","663m","686m","710m","735m","761m","788m","816m","844m",
				"874m","905m","937m","970m","1.00s","1.04s","1.08s","1.11s","1.15s","1.19s","1.24s","1.28s",
				"1.33s","1.37s","1.42s","1.47s","1.52s","1.58s","1.63s","1.69s","1.75s","1.81s","1.87s",
				"1.94s","2.01s","2.08s","2.15s","2.23s","2.31s","2.39s","2.47s","2.56s","2.65s","2.74s",
				"2.84s","2.94s","3.04s","3.15s","3.26s"};
			return pick(t, _v);
		}

		// 0.02 ms a step, as NME's table (which rounds each to the hundredth).
		std::string fmtDelayTime(const int _v) { return num(_v * 2.65 / 127.0, 2, false) + "ms"; }

		std::string fmtOscillatorWave(const int _v) { static const char* const t[] = {"Sine", "Tri", "Saw", "Square"}; return pick(t, _v); }

		std::string fmtPhaserCenterFrequency(const int _v)
		{
			static const char* const t[] = {
				"100Hz","104Hz","108Hz","113Hz","117Hz","122Hz","127Hz","132Hz","138Hz","143Hz","149Hz",
				"155Hz","162Hz","168Hz","175Hz","182Hz","190Hz","197Hz","205Hz","214Hz","222Hz","231Hz",
				"241Hz","251Hz","261Hz","272Hz","283Hz","294Hz","306Hz","319Hz","332Hz","345Hz","359Hz",
				"374Hz","389Hz","405Hz","421Hz","439Hz","457Hz","475Hz","495Hz","515Hz","536Hz","558Hz",
				"580Hz","604Hz","629Hz","654Hz","681Hz","709Hz","738Hz","768Hz","799Hz","831Hz","865Hz",
				"901Hz","937Hz","976Hz","1.02kHz","1.06kHz","1.10kHz","1.14kHz","1.19kHz","1.24kHz","1.29kHz",
				"1.34kHz","1.40kHz","1.45kHz","1.51kHz","1.58kHz","1.64kHz","1.71kHz","1.78kHz","1.85kHz",
				"1.92kHz","2.00kHz","2.08kHz","2.17kHz","2.26kHz","2.35kHz","2.45kHz","2.55kHz","2.65kHz",
				"2.76kHz","2.87kHz","2.99kHz","3.11kHz","3.24kHz","3.37kHz","3.50kHz","3.65kHz","3.80kHz",
				"3.95kHz","4.11kHz","4.28kHz","4.45kHz","4.64kHz","4.82kHz","5.02kHz","5.23kHz","5.44kHz",
				"5.66kHz","5.89kHz","6.13kHz","6.38kHz","6.64kHz","6.91kHz","7.19kHz","7.49kHz","7.79kHz",
				"8.11kHz","8.44kHz","8.79kHz","9.14kHz","9.52kHz","9.91kHz","10.3kHz","10.7kHz","11.2kHz",
				"11.6kHz","12.1kHz","12.6kHz","13.1kHz","13.6kHz","14.2kHz","14.8kHz","15.4kHz","16.0kHz"};
			return pick(t, _v);
		}

		std::string fmtVowels(const int _v) { static const char* const t[] = {"A", "E", "I", "O", "U", "Y", "AA", "AE", "OE"}; return pick(t, _v); }

		std::string fmtEnvelopeAttack(const int _v)
		{
			static const char* const t[] = {
				"Fast","0.53m","0.56m","0.59m","0.63m","0.67m","0.71m","0.75m","0.79m","0.84m","0.89m",
				"0.94m","1.00m","1.06m","1.12m","1.19m","1.26m","1.33m","1.41m","1.50m","1.59m","1.68m",
				"1.78m","1.89m","2.00m","2.12m","2.24m","2.38m","2.52m","2.67m","2.83m","3.00m","3.17m",
				"3.36m","3.56m","3.78m","4.00m","4.24m","4.49m","4.76m","5.04m","5.34m","5.66m","5.99m",
				"6.35m","6.73m","7.13m","7.55m","8.00m","8.48m","8.98m","9.51m","10.1m","10.7m","11.3m",
				"12.0m","12.7m","13.5m","14.3m","15.1m","16.0m","17.0m","18.0m","19.0m","20.2m","21.4m",
				"22.6m","24.0m","25.4m","26.9m","28.5m","30.2m","32.0m","33.9m","35.9m","38.1m","40.3m",
				"42.7m","45.3m","47.9m","50.8m","53.8m","57.0m","60.4m","64.0m","67.8m","71.8m","76.1m",
				"80.6m","85.4m","90.5m","95.9m","102m","108m","114m","121m","128m","136m","144m","152m",
				"161m","171m","181m","192m","203m","215m","228m","242m","256m","271m","287m","304m",
				"323m","342m","362m","384m","406m","431m","456m","483m","512m","542m","575m","609m",
				"645m","683m","724m","767m"};
			return pick(t, _v);
		}

		std::string fmtCompanderRatio(const int _v)
		{
			static const char* const t[] = {
				"1.0:1","1.1:1","1.2:1","1.3:1","1.4:1","1.5:1","1.6:1","1.7:1","1.8:1","1.9:1","2.0:1",
				"2.2:1","2.4:1","2.6:1","2.8:1","3.0:1","3.2:1","3.4:1","3.6:1","3.8:1","4.0:1","4.2:1",
				"4.4:1","4.6:1","4.8:1","5.0:1","5.5:1","6.0:1","6.5:1","7.0:1","7.5:1","8.0:1","8.5:1",
				"9.0:1","9.5:1","10:1","11:1","12:1","13:1","14:1","15:1","16:1","17:1","18:1","19:1",
				"20:1","22:1","24:1","26:1","28:1","30:1","32:1","34:1","36:1","38:1","40:1","42:1",
				"44:1","46:1","48:1","50:1","55:1","60:1","65:1","70:1","75:1","80:1"};
			return pick(t, _v);
		}

		std::string fmtAdsrTime(const int _v)
		{
			static const char* const t[] = {
				"0.5m","0.7m","1.0m","1.3m","1.5m","1.8m","2.1m","2.3m","2.6m","2.9m","3.2m","3.5m",
				"3.9m","4.2m","4.6m","4.9m","5.3m","5.7m","6.1m","6.6m","7.0m","7.5m","8.0m","8.5m",
				"9.1m","9.7m","10m","11m","12m","13m","13m","14m","15m","16m","17m","19m","20m",
				"21m","23m","24m","26m","28m","30m","32m","35m","37m","40m","43m","47m","51m",
				"55m","59m","64m","69m","75m","81m","88m","95m","103m","112m","122m","132m","143m",
				"156m","170m","185m","201m","219m","238m","260m","283m","308m","336m","367m","400m",
				"436m","476m","520m","567m","619m","676m","738m","806m","881m","962m","1.1s","1.1s",
				"1.3s","1.4s","1.5s","1.6s","1.8s","2.0s","2.1s","2.3s","2.6s","2.8s","3.1s","3.3s",
				"3.7s","4.0s","4.4s","4.8s","5.2s","5.7s","6.3s","6.8s","7.5s","8.2s","9.0s","9.8s",
				"10.7s","11.7s","12.8s","14.0s","15.3s","16.8s","18.3s","20.1s","21.9s","24.0s","26.3s",
				"28.7s","31.4s","34.4s","37.6s","41.1s","45.0s"};
			return pick(t, _v);
		}

		std::string fmtMultiEnvSustain(const int _v) { static const char* const t[] = {"--", "L1", "L2", "L3", "L4"}; return pick(t, _v); }
		std::string fmtLFORange(const int _v) { static const char* const t[] = {"Sub", "Lo", "Hi"}; return pick(t, _v); }
		std::string fmtMorphKbAssignment(const int _v) { static const char* const t[] = {"None", "Velocity", "Note"}; return pick(t, _v); }

		// ____________________________________________________________________________________
		// Which parameter reads which way: module type, parameter index, formatter. Generated
		// from NME's data/modules.xml (each <parameter class="parameter"> with a formatter), plus
		// the few the G1's display reads where NME has no formatter (marked as such).

		using Fn = std::string (*)(int);
		struct Entry { uint8_t type, param; Fn fn; };

		constexpr Entry g_table[] = {
			{4, 1, fmtChannelSelectOut2},	// 2Output: destination
			{4, 2, fmtOffOn},	// 2Output: mute
			{5, 1, fmtChannelSelectOut1},	// 1Output: dest
			{5, 2, fmtOffOn},	// 1Output: mute
			{7, 0, fmtNote},	// OscA: freq coarse
			{7, 1, fmtMinus64},	// OscA: freq fine
			{7, 2, fmtFreqkbt},	// OscA: freq KBT
			{7, 3, fmtPulseWidth},	// OscA: Pulse width
			{7, 4, fmtOscillatorWave},	// OscA: waveform
			{7, 9, fmtOffOn},	// OscA: Mute
			{8, 0, fmtNote},	// OscB: Freq coarse
			{8, 1, fmtMinus64},	// OscB: Freq fine
			{8, 2, fmtFreqkbt},	// OscB: Freq KBT
			{8, 3, fmtOscillatorWave},	// OscB: waveform
			{8, 8, fmtOffOn},	// OscB: mute
			{9, 0, fmtNote},	// OscC: pitch coarse
			{9, 1, fmtMinus64},	// OscC: pitch fine
			{9, 2, fmtOffOn},	// OscC: pitch KBT
			{9, 5, fmtOffOn},	// OscC: mute
			{10, 1, fmtMinus64},	// OscSlvB: detune fine
			{10, 2, fmtPulseWidth},	// OscSlvB: PW
			{10, 4, fmtOffOn},	// OscSlvB: mute
			{11, 0, fmtMinus64},	// OscSlvC: detune coarse
			{11, 1, fmtMinus64},	// OscSlvC: detune fine
			{11, 3, fmtOffOn},	// OscSlvC: mute
			{12, 0, fmtMinus64},	// OscSlvD: detune coarse
			{12, 1, fmtMinus64},	// OscSlvD: detune fine
			{12, 3, fmtOffOn},	// OscSlvD: mute
			{13, 0, fmtMinus64},	// OscSlvE: detune coarse
			{13, 1, fmtMinus64},	// OscSlvE: detune fine
			{13, 3, fmtOffOn},	// OscSlvE: mute
			{14, 0, fmtMinus64},	// OscSlvA: detune coarse
			{14, 1, fmtMinus64},	// OscSlvA: detune fine
			{14, 4, fmtOffOn},	// OscSlvA: mute
			{15, 0, fmtMinus64},	// NoteSeqA: step 1
			{15, 1, fmtMinus64},	// NoteSeqA: step 2
			{15, 2, fmtMinus64},	// NoteSeqA: step 3
			{15, 3, fmtMinus64},	// NoteSeqA: step 4
			{15, 4, fmtMinus64},	// NoteSeqA: step 5
			{15, 5, fmtMinus64},	// NoteSeqA: step 6
			{15, 6, fmtMinus64},	// NoteSeqA: step 7
			{15, 7, fmtMinus64},	// NoteSeqA: step 8
			{15, 8, fmtMinus64},	// NoteSeqA: step 9
			{15, 9, fmtMinus64},	// NoteSeqA: step 10
			{15, 10, fmtMinus64},	// NoteSeqA: step 11
			{15, 11, fmtMinus64},	// NoteSeqA: step 12
			{15, 12, fmtMinus64},	// NoteSeqA: step 13
			{15, 13, fmtMinus64},	// NoteSeqA: step 14
			{15, 14, fmtMinus64},	// NoteSeqA: step 15
			{15, 15, fmtMinus64},	// NoteSeqA: step 16
			{15, 16, fmtPlus1},	// NoteSeqA: step count
			{15, 18, fmtOffOn},	// NoteSeqA: record
			{15, 19, fmtOffOn},	// NoteSeqA: pause
			{15, 20, fmtOffOn},	// NoteSeqA: loop
			{17, 0, fmtPlus1},	// EventSeq: stepcount
			{17, 1, fmtOffOn},	// EventSeq: active
			{17, 2, fmtOffOn},	// EventSeq: gate1
			{17, 3, fmtOffOn},	// EventSeq: gate2
			{17, 4, fmtOffOn},	// EventSeq: seq 1, step 1
			{17, 5, fmtOffOn},	// EventSeq: seq 1, step 2
			{17, 6, fmtOffOn},	// EventSeq: seq 1, step 3
			{17, 7, fmtOffOn},	// EventSeq: seq 1, step 4
			{17, 8, fmtOffOn},	// EventSeq: seq 1, step 5
			{17, 9, fmtOffOn},	// EventSeq: seq 1, step 6
			{17, 10, fmtOffOn},	// EventSeq: seq 1, step 7
			{17, 11, fmtOffOn},	// EventSeq: seq 1, step 8
			{17, 12, fmtOffOn},	// EventSeq: seq 1, step 9
			{17, 13, fmtOffOn},	// EventSeq: seq 1, step 10
			{17, 14, fmtOffOn},	// EventSeq: seq 1, step 11
			{17, 15, fmtOffOn},	// EventSeq: seq 1, step 12
			{17, 16, fmtOffOn},	// EventSeq: seq 1, step 13
			{17, 17, fmtOffOn},	// EventSeq: seq 1, step 14
			{17, 18, fmtOffOn},	// EventSeq: seq 1, step 15
			{17, 19, fmtOffOn},	// EventSeq: seq 1, step 16
			{17, 20, fmtOffOn},	// EventSeq: seq 2, step 1
			{17, 21, fmtOffOn},	// EventSeq: seq 2, step 2
			{17, 22, fmtOffOn},	// EventSeq: seq 2, step 3
			{17, 23, fmtOffOn},	// EventSeq: seq 2, step 4
			{17, 24, fmtOffOn},	// EventSeq: seq 2, step 5
			{17, 25, fmtOffOn},	// EventSeq: seq 2, step 6
			{17, 26, fmtOffOn},	// EventSeq: seq 2, step 7
			{17, 27, fmtOffOn},	// EventSeq: seq 2, step 8
			{17, 28, fmtOffOn},	// EventSeq: seq 2, step 9
			{17, 29, fmtOffOn},	// EventSeq: seq 2, step 10
			{17, 30, fmtOffOn},	// EventSeq: seq 2, step 11
			{17, 31, fmtOffOn},	// EventSeq: seq 2, step 12
			{17, 32, fmtOffOn},	// EventSeq: seq 2, step 13
			{17, 33, fmtOffOn},	// EventSeq: seq 2, step 14
			{17, 34, fmtOffOn},	// EventSeq: seq 2, step 15
			{17, 35, fmtOffOn},	// EventSeq: seq 2, step 16
			{18, 1, fmtMinus64},	// X-Fade: crossfade
			{20, 1, fmtAdsrTime},	// ADSR: attack
			{20, 2, fmtAdsrTime},	// ADSR: decay
			{20, 3, fmtEnvelopeLevelDivider},	// ADSR: sustain (the OS reads it as 0-64; NME gives no formatter)
			{20, 4, fmtAdsrTime},	// ADSR: release
			{20, 5, fmtOffOn},	// ADSR: invert
			{21, 0, fmtEnvelopeAttack},	// Compressor: attack
			{21, 1, fmtCompanderRelease},	// Compressor: release
			{21, 2, fmtCompressorThreshold},	// Compressor: treshold
			{21, 3, fmtCompanderRatio},	// Compressor: ratio
			{21, 4, fmtEnvRefLevel},	// Compressor: ref level
			{21, 5, fmtCompressorLimiter},	// Compressor: limiter
			{21, 6, fmtOffOn},	// Compressor: act
			{21, 7, fmtOffOn},	// Compressor: mon
			{21, 8, fmtOffOn},	// Compressor: bypass
			{22, 0, fmtPartialGen},	// PartialGen: partials
			{23, 0, fmtAdsrTime},	// Mod-Env: attack
			{23, 1, fmtAdsrTime},	// Mod-Env: decay
			{23, 2, fmtEnvelopeLevelDivider},	// Mod-Env: sustain (the OS reads it as 0-64; NME gives no formatter)
			{23, 3, fmtAdsrTime},	// Mod-Env: release
			{23, 8, fmtOffOn},	// Mod-Env: invert
			{24, 0, fmtLFOHz},	// LFOA: rate
			{24, 1, fmtLFORange},	// LFOA: range
			{24, 4, fmtOffOn},	// LFOA: mono
			{24, 6, fmtPhase},	// LFOA: phase
			{24, 7, fmtOffOn},	// LFOA: mute
			{25, 0, fmtLFOHz},	// LFOB: rate
			{25, 1, fmtLFORange},	// LFOB: range
			{25, 2, fmtPhase},	// LFOB: phase
			{25, 4, fmtOffOn},	// LFOB: mono
			{26, 0, fmtLFOHz},	// LFOC: rate
			{26, 1, fmtLFORange},	// LFOC: range
			{26, 4, fmtOffOn},	// LFOC: mono
			{26, 5, fmtOffOn},	// LFOC: mute
			{27, 0, fmtLFOHz},	// LFOSlvB: rate
			{28, 0, fmtLFOHz},	// LFOSlvC: rate
			{29, 0, fmtLFOHz},	// LFOSlvD: rate
			{30, 0, fmtLFOHz},	// LFOSlvE: rate
			{33, 0, fmtOffOn},	// ClkRndGen: mono
			{33, 1, fmtOffOn},	// ClkRndGen: col
			{34, 0, fmtLFOHz},	// RndStepGen: rate
			{36, 0, fmtLogicDelay},	// PosEdgeDelay: time
			{37, 0, fmtLogicDelay},	// LogicDelay: time
			{38, 0, fmtLogicDelay},	// Pulse: time
			{39, 0, fmtSmoothTime},	// Smooth: time
			{43, 0, fmtOffset64_2},	// Constant: value
			{43, 1, fmtOffOn},	// Constant: unipolar
			{44, 0, fmtOffOn},	// GainControl: shift
			{45, 0, fmtVowels},	// VocalFilter: left vowel
			{45, 1, fmtVowels},	// VocalFilter: middle vowel
			{45, 2, fmtVowels},	// VocalFilter: right vowel
			{45, 6, fmtMinus64},	// VocalFilter: frequency
			{46, 0, fmtAdsrTime},	// AHD: A
			{46, 1, fmtAdsrTime},	// AHD: H
			{46, 2, fmtAdsrTime},	// AHD: D
			{47, 1, fmtMinus64},	// Pan: pan
			{50, 0, fmtFilterHz2},	// FilterC: freq
			{50, 2, fmtOffOn},	// FilterC: gain control
			{49, 0, fmtFilterHz2},	// FilterD: freq
			{51, 1, fmtOffOn},	// FilterE: gain control
			{51, 3, fmtFilterHz2},	// FilterE: frequency
			{51, 9, fmtOffOn},	// FilterE: bypass
			{52, 0, fmtEnvelopeLevelDivider},	// Multi-Env: level 1
			{52, 1, fmtEnvelopeLevelDivider},	// Multi-Env: level 2
			{52, 2, fmtEnvelopeLevelDivider},	// Multi-Env: level 3
			{52, 3, fmtEnvelopeLevelDivider},	// Multi-Env: level 4
			{52, 4, fmtAdsrTime},	// Multi-Env: time 1
			{52, 5, fmtAdsrTime},	// Multi-Env: time 2
			{52, 6, fmtAdsrTime},	// Multi-Env: time 3
			{52, 7, fmtAdsrTime},	// Multi-Env: time 4
			{52, 8, fmtAdsrTime},	// Multi-Env: time 5
			{52, 9, fmtMultiEnvSustain},	// Multi-Env: sustain
			{54, 0, fmtPlus1},	// Quantizer: bits
			{57, 1, fmtOffOn},	// InvLevShift: inv
			{58, 0, fmtDrumHz},	// DrumSynth: MTune
			{58, 1, fmtDrumPartials},	// DrumSynth: STune
			{58, 15, fmtOffOn},	// DrumSynth: Mute
			{59, 0, fmtMinus64},	// CompareLev: level
			{64, 0, fmtLogicDelay},	// NegEdgeDelay: time
			{66, 0, fmtOffOn},	// ControlMixer: inv 1
			{66, 2, fmtOffOn},	// ControlMixer: inv 2
			{67, 0, fmtNote},	// NoteDetect: note
			{68, 0, fmtBPM},	// ClkGen: rate
			{68, 1, fmtOffOn},	// ClkGen: on/off
			{69, 0, fmtPlus1},	// ClkDiv: divider
			{71, 0, fmtEnvelopeAttack},	// EnvFollower: attack
			{71, 1, fmtEnvelopeRelease},	// EnvFollower: release
			{72, 0, fmtNoteScale},	// NoteScaler: transpose
			{75, 0, fmtBipolar64},	// NoteQuant: range
			{75, 1, fmtNoteQuantNotes},	// NoteQuant: notes
			{78, 1, fmtDelayTime},	// Delay: time
			{79, 5, fmtOffOn},	// 4-1Switch: mute
			{80, 0, fmtLFOHz},	// LFOSlvA: rate
			{80, 1, fmtPhase},	// LFOSlvA: phase
			{80, 3, fmtOffOn},	// LFOSlvA: mono
			{80, 4, fmtOffOn},	// LFOSlvA: mute
			{84, 0, fmtAdsrTime},	// AD-Env: attack
			{84, 1, fmtAdsrTime},	// AD-Env: decay
			{84, 2, fmtOffOn},	// AD-Env: gate
			{85, 1, fmtMinus64},	// OscSlvFM: fine
			{85, 2, fmtOffOn},	// OscSlvFM: -3oct
			{85, 4, fmtOffOn},	// OscSlvFM: mute
			{88, 2, fmtOffOn},	// 1-4Switch: mute
			{90, 16, fmtPlus1},	// NoteSeqB: step
			{90, 18, fmtOffOn},	// NoteSeqB: record
			{90, 19, fmtOffOn},	// NoteSeqB: play
			{90, 20, fmtOffOn},	// NoteSeqB: loop
			{91, 16, fmtPlus1},	// CtrlSeq: step
			{91, 17, fmtOffOn},	// CtrlSeq: uni
			{91, 18, fmtOffOn},	// CtrlSeq: loop
			{92, 0, fmtFilterHz2},	// FilterF: frequency
			{92, 6, fmtOffOn},	// FilterF: bypass
			{94, 2, fmtOffOn},	// StereoChorus: bypass
			{95, 0, fmtNote},	// PercOsc: pitch (the OS reads it as a note; NME gives no formatter)
			{95, 3, fmtOffOn},	// PercOsc: punch
			{95, 5, fmtMinus64},	// PercOsc: pitchfine
			{95, 6, fmtOffOn},	// PercOsc: mute
			{96, 0, fmtNote},	// FormantOsc: pitch (the OS reads it as a note; NME gives no formatter)
			{96, 1, fmtMinus64},	// FormantOsc: pitch fine
			{96, 2, fmtOffOn},	// FormantOsc: kbt
			{96, 3, fmtOffOn},	// FormantOsc: mute
			{96, 4, fmtTimbre},	// FormantOsc: timbre
			{97, 0, fmtNote},	// MasterOsc: pitch (the OS reads it as a note; NME gives no formatter)
			{97, 1, fmtMinus64},	// MasterOsc: pitch fine
			{97, 2, fmtOffOn},	// MasterOsc: kbt
			{98, 1, fmtOffOn},	// KeyQuant: cont
			{98, 2, fmtOffOn},	// KeyQuant: E
			{98, 3, fmtOffOn},	// KeyQuant: F
			{98, 4, fmtOffOn},	// KeyQuant: F#
			{98, 5, fmtOffOn},	// KeyQuant: G
			{98, 6, fmtOffOn},	// KeyQuant: G#
			{98, 7, fmtOffOn},	// KeyQuant: A
			{98, 8, fmtOffOn},	// KeyQuant: Bb
			{98, 9, fmtOffOn},	// KeyQuant: B
			{98, 10, fmtOffOn},	// KeyQuant: C
			{98, 11, fmtOffOn},	// KeyQuant: C#
			{98, 12, fmtOffOn},	// KeyQuant: D
			{98, 13, fmtOffOn},	// KeyQuant: D#
			{99, 2, fmtOffOn},	// PatternGen: low delta
			{99, 3, fmtPatternStep},	// PatternGen: step
			{99, 4, fmtOffOn},	// PatternGen: mono
			{100, 0, fmtNote},	// KeybSplit: lower
			{100, 1, fmtNote},	// KeybSplit: upper
			{102, 1, fmtOffOn},	// Phaser: lfo
			{102, 3, fmtPhaserCenterFrequency},	// Phaser: center freq
			{102, 6, fmtPlus1},	// Phaser: peaks
			{102, 8, fmtOffOn},	// Phaser: bypass
			{103, 0, fmtEqHz},	// EqMid: freq
			{103, 3, fmtOffOn},	// EqMid: bypass
			{104, 0, fmtEqHz},	// EqShelving: freq
			{104, 3, fmtOffOn},	// EqShelving: bypass
			{105, 0, fmtEnvelopeAttack},	// Expander: attack
			{105, 1, fmtCompanderRelease},	// Expander: release
			{105, 2, fmtExpanderThreshold},	// Expander: treshold
			{105, 3, fmtCompanderRatio},	// Expander: ratio
			{105, 4, fmtExpanderGate},	// Expander: gate
			{105, 5, fmtExpanderHold},	// Expander: hold
			{105, 6, fmtOffOn},	// Expander: sidechain activation
			{105, 7, fmtOffOn},	// Expander: sidechain monitor
			{105, 8, fmtOffOn},	// Expander: bypass
			{106, 0, fmtSemitones},	// OscSineBank: osc1 coarse
			{106, 1, fmtMinus64},	// OscSineBank: osc1 fine
			{106, 2, fmtSemitones},	// OscSineBank: osc1 level
			{106, 3, fmtSemitones},	// OscSineBank: osc2 coarse
			{106, 4, fmtMinus64},	// OscSineBank: osc2 fine
			{106, 5, fmtSemitones},	// OscSineBank: osc2 level
			{106, 6, fmtSemitones},	// OscSineBank: osc3 coarse
			{106, 7, fmtMinus64},	// OscSineBank: osc3 fine
			{106, 8, fmtSemitones},	// OscSineBank: osc3 level
			{106, 9, fmtSemitones},	// OscSineBank: osc4 coarse
			{106, 10, fmtMinus64},	// OscSineBank: osc4 fine
			{106, 11, fmtSemitones},	// OscSineBank: osc4 level
			{106, 12, fmtSemitones},	// OscSineBank: osc5 coarse
			{106, 13, fmtMinus64},	// OscSineBank: osc5 fine
			{106, 14, fmtSemitones},	// OscSineBank: osc5 level
			{106, 15, fmtSemitones},	// OscSineBank: osc6 coarse
			{106, 16, fmtMinus64},	// OscSineBank: osc6 fine
			{106, 17, fmtSemitones},	// OscSineBank: osc6 level
			{106, 18, fmtOffOn},	// OscSineBank: osc1 mute
			{106, 19, fmtOffOn},	// OscSineBank: osc2 mute
			{106, 20, fmtOffOn},	// OscSineBank: osc3 mute
			{106, 21, fmtOffOn},	// OscSineBank: osc4 mute
			{106, 22, fmtOffOn},	// OscSineBank: osc5 mute
			{106, 23, fmtOffOn},	// OscSineBank: osc6 mute
			{107, 0, fmtNote},	// SpectralOsc: freq coarse (the OS reads it as a note; NME reads Hz)
			{107, 1, fmtMinus64},	// SpectralOsc: freq fine
			{107, 3, fmtPartials},	// SpectralOsc: partials
			{107, 8, fmtOffOn},	// SpectralOsc: kbt
			{107, 9, fmtOffOn},	// SpectralOsc: mute
			{110, 0, fmtLFOHz},	// RandomGen: rate
			{113, 0, fmtMinus64},	// 1to2Fade: fade
			{114, 0, fmtMinus64},	// 2to1Fade: fade
			{115, 1, fmtNoteVelScaleGain},	// NoteVelScal: left gain
			{115, 2, fmtNote},	// NoteVelScal: breakpoint
			{115, 3, fmtNoteVelScaleGain},	// NoteVelScal: right gain
			{118, 0, fmtPlus1},	// Digitizer: quant bits
			{118, 1, fmtDigitizerHz},	// Digitizer: sample rate
			{118, 3, fmtOffOn},	// Digitizer: quant off
			{118, 4, fmtOffOn},	// Digitizer: sampling off
			{127, 0, fmtOffOn},	// PolyAreaIn: +6db
		};

		const std::map<std::pair<uint8_t, uint8_t>, Fn>& table()
		{
			static const auto map = []
			{
				std::map<std::pair<uint8_t, uint8_t>, Fn> m;
				for(const auto& e : g_table)
					m.emplace(std::make_pair(e.type, e.param), e.fn);
				return m;
			}();
			return map;
		}
	}

	std::string formatValue(const uint8_t _type, const uint8_t _param, const int _value)
	{
		const auto& t = table();
		const auto it = t.find({_type, _param});
		return it == t.end() ? str(_value) : it->second(_value);
	}

	bool hasHzReading(const uint8_t _type, const uint8_t _param)
	{
		switch(_type)
		{
		case 7: case 8: case 9: case 95: case 96: case 97: case 107:	// OscA, OscB, OscC, PercOsc, FormantOsc, MasterOsc, SpectralOsc
			return _param == 0;
		default:
			return false;
		}
	}

	std::string formatHz(const int _value)
	{
		const double f = 440.0 * std::pow(2.0, (_value - 69) / 12.0);
		if(f < 10)		return num(f, 2) + " Hz";
		if(f < 100)		return num(f, 1) + " Hz";
		if(f < 1000)	return num(f, 0) + " Hz";
		return num(f / 1000.0, 2) + " kHz";
	}
}
