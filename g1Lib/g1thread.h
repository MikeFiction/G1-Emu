#pragma once

#include <chrono>

// What the emulator's threads need from the OS beyond std::thread (issues #63, #65).
namespace g1
{
	// A sleep that lasts about what it is asked for. On Windows std::this_thread::sleep_for(200 us)
	// takes the system timer's period, 15.6 ms unless some program in the same process asked for
	// more (a DAW often does, a plugin host process may not): a worker allowed to run only a few ms
	// ahead of the audio then falls behind it at every sleep, and the host hears dropouts. A
	// high-resolution waitable timer there wakes after ~0.5 ms, with no change to the system's timer.
	void preciseSleep(std::chrono::microseconds _duration);

	// For a thread that has to keep up with the audio: on Windows 11 a thread of a process in the
	// background, or on battery or a power-saving plan, can be put under EcoQoS, which runs it slower
	// and, on a hybrid CPU, on the efficiency cores. This opts the calling thread out. Elsewhere,
	// nothing.
	void keepThreadAtFullSpeed();
}
