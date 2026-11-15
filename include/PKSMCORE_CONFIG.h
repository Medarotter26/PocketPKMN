// Build configuration for PKSM-Core (lib/PKSM-Core, GPL-3.0), see its README.
// Its text tables and species data are read at run time from the app's romfs.
#pragma once

#define _PKSMCORE_LANG_FOLDER     "romfs:/i18n/"
#define _PKSMCORE_PERSONAL_FOLDER "romfs:/personal/"
// devkitARM's newlib has no getline(); it provides __getline() in stdio.h.
#define _PKSMCORE_GETLINE_FUNC __getline
// WalkerBox calls PKSM-Core from one thread only.
#define _PKSMCORE_DISABLE_THREAD_SAFETY
