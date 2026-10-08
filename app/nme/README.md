# Code from Animatek NME

These files are a copy of part of [Animatek NME](https://github.com/animatek/Animatek-NME)
(GPLv3, like G1-Emu, and by the same author), taken from its commit `c000cac` (2026-10-08). They
turn a `.pch` file into the packets the G1 takes, for the Presets page's **Load .pch**: `PchFileIO` reads the file, `PatchSerializer` makes the sections, `UploadPacketizer`
cuts and frames them. `modules.xml` is NME's `data/modules.xml`, the modules' descriptions those
need, built into the program.

They are kept **as they are in NME**, so they can be brought up to date by copying the same
files again; G1-Emu's own code around them is in `app/pchupload.*`. To update:

```bash
cd ../Nomad2026/source
for f in model/BitStreamWriter.h model/Descriptors.h model/ModuleDescriptions.cpp model/ModuleDescriptions.h \
         model/ModuleTags.cpp model/ModuleTags.h model/Patch.cpp model/Patch.h model/PatchSerializer.cpp \
         model/PatchSerializer.h model/PchFileIO.cpp model/PchFileIO.h model/SignalType.h \
         midi/UploadPacketizer.cpp midi/UploadPacketizer.h; do cp $f ../../G1-Emu/app/nme/$f; done
cp ../data/modules.xml ../../G1-Emu/app/nme/modules.xml
```

and write the new commit above.
