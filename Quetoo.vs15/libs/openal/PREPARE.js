if (!fso.FileExists("tmp2.zip"))
{
	log("Downloading OpenAL...");
	download("http://kcat.strangesoft.net/openal-binaries/openal-soft-1.18.2-bin.zip", "tmp2.zip");
}

log("Extracting OpenAL...");
unz("e tmp2.zip -aos \"openal-soft-1.18.2-bin/include/AL/*.h\" -oAL");
unz("e tmp2.zip -aos \"openal-soft-1.18.2-bin/libs/Win32/*.lib\" -olibs/Win32");
unz("e tmp2.zip -aos \"openal-soft-1.18.2-bin/libs/Win64/*.lib\" -olibs/x64");
unz("e tmp2.zip -aos \"openal-soft-1.18.2-bin/bin/Win32/*.dll\" -obin/Win32");
unz("e tmp2.zip -aos \"openal-soft-1.18.2-bin/bin/Win64/*.dll\" -obin/x64");
if (fso.FileExists("bin/Win32/OpenAL32.dll"))
	fso.DeleteFile("bin/Win32/OpenAL32.dll");
if (fso.FileExists("bin/x64/OpenAL32.dll"))
	fso.DeleteFile("bin/x64/OpenAL32.dll");
fso.MoveFile("bin/Win32/soft_oal.dll", "bin/Win32/OpenAL32.dll");
fso.MoveFile("bin/x64/soft_oal.dll", "bin/x64/OpenAL32.dll");