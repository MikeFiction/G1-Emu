#pragma once

// A small rolling log, shared by the standalone and the plugin (issues #59, #63, #65: a user's
// own account of "no audio" or "connection lost" is a guess without knowing what the emulator
// actually saw). Every line is timestamped and appended to logPath(); kept under a few hundred KB
// by dropping the oldest lines, so it never grows without bound over a long session.
//
// Report issue (app/gui/Panel.cpp) opens its folder, so it is right there to attach.

#include <string>

namespace g1app
{
	std::string logPath();
	void logEvent(const std::string& _line);
}
