# Microsoft Developer Studio Project File - Name="cnv_flacpcm" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=cnv_flacpcm - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "cnv_flacpcm.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "cnv_flacpcm.mak" CFG="cnv_flacpcm - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "cnv_flacpcm - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "cnv_flacpcm - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "$/Studio/cnv_flacpcm"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "cnv_flacpcm - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "cnv_flacpcm_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\include" /I "\Wasabi SDK\studio" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "cnv_flacpcm_EXPORTS" /D "USE_ASM" /D "WACLIENT_NOICONSUPPORT" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 libFLAC.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib msacm32.lib winmm.lib /nologo /dll /machine:I386 /out:"../../obj/bin/cnv_flacpcm.wac" /libpath:"../../obj/lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "cnv_flacpcm - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "cnv_flacpcm_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\..\include" /I "\Wasabi SDK\studio" /D "_DEBUG" /D "WACLIENT_NOICONSUPPORT" /D "REAL_STDIO" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "cnv_flacpcm_EXPORTS" /D "USE_ASM" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 libFLAC.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib msacm32.lib winmm.lib /nologo /dll /debug /machine:I386 /out:"../../obj/bin/cnv_flacpcm.wac" /pdbtype:sept /libpath:"../../obj/lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "cnv_flacpcm - Win32 Release"
# Name "cnv_flacpcm - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Wasabi SDK"

# PROP Default_Filter ""
# Begin Source File

SOURCE="\Wasabi SDK\studio\studio\assert.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\attribs\attribute.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\attribs\cfgitemi.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\studio\corecb.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\common\depend.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\common\memblock.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\common\nsGUID.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\common\pathparse.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\common\ptrlist.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\studio\services\servicei.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\common\std.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\common\string.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\studio\services\svc_mediaconverter.cpp"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\studio\services\svc_mediaconverter.h"
# End Source File
# Begin Source File

SOURCE="\Wasabi SDK\studio\studio\waclient.cpp"
# End Source File
# End Group
# Begin Source File

SOURCE=.\cnv_flacpcm.cpp
# End Source File
# Begin Source File

SOURCE=.\flacpcm.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\cnv_flacpcm.h
# End Source File
# Begin Source File

SOURCE=.\flacpcm.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\README.txt
# End Source File
# End Target
# End Project
