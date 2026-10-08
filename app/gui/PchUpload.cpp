#include "PchUpload.h"

#include "G1NmeData.h"
#include "midi/UploadPacketizer.h"
#include "model/ModuleDescriptions.h"
#include "model/PatchSerializer.h"
#include "model/PchFileIO.h"

namespace g1gui
{
	namespace
	{
		// NME's modules.xml, read once.
		const ModuleDescriptions& descriptions()
		{
			static const ModuleDescriptions descs = []
			{
				ModuleDescriptions d;
				d.loadFromXmlString(juce::String::fromUTF8(G1NmeData::modules_xml, G1NmeData::modules_xmlSize));
				return d;
			}();
			return descs;
		}
	}

	PchUpload preparePch(const juce::File& _file, const int _slot)
	{
		PchUpload up;
		if(descriptions().getModuleCount() == 0)
		{
			up.error = "the module descriptions are missing from this build";
			return up;
		}
		PchFileIO io(descriptions());
		const auto patch = io.readFile(_file);
		if(!patch)
		{
			up.error = "\"" + _file.getFileName() + "\" is not a patch G1-Emu can read";
			return up;
		}
		up.name = patch->getName().substring(0, 16);
		PatchSerializer serializer;
		const auto packets = UploadPacketizer::cut(serializer.serializeForUpload(*patch));
		for(size_t i = 0; i < packets.size(); ++i)
			up.frames.push_back(UploadPacketizer::frame(packets[i], i == 0, i + 1 == packets.size(), _slot));
		up.abort = UploadPacketizer::closeTransferFrame(_slot);
		if(up.frames.empty())
			up.error = "\"" + _file.getFileName() + "\" made no data to upload";
		return up;
	}
}
