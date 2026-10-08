#pragma once

// A .pch file made into what the G1 takes, for the Presets page's Load .pch: the patch's name and
// the SysEx that uploads it into a slot, packet by packet. The work is Animatek NME's own code
// (app/nme: PchFileIO, PatchSerializer, UploadPacketizer), with its module descriptions built in.

#include <juce_core/juce_core.h>

#include <cstdint>
#include <string>
#include <vector>

namespace g1gui
{
	struct PchUpload
	{
		juce::String name;								// as the G1 will store it (the file's name)
		std::vector<std::vector<uint8_t>> frames;		// the upload's packets, in order
		std::vector<uint8_t> abort;						// the empty last packet that ends a transfer cut short
		juce::String error;								// why not, when it cannot be read
		bool ok() const { return error.isEmpty() && !frames.empty(); }
	};

	PchUpload preparePch(const juce::File& _file, int _slot);
}
