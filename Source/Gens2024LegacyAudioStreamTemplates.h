// TODO: Remove this file; we're going to replace this with clean acb patching
#ifndef START_ACB
#define START_ACB(name)
#endif

#ifndef START_CUE
#define START_CUE(id, name)
#endif

#ifndef TRACK
#define TRACK(soundElementPath)
#endif

#ifndef END_CUE
#define END_CUE
#endif

#ifndef END_ACB
#define END_ACB
#endif

START_ACB("SNG01_GHZ")
	START_CUE(801000, "Green_Hill_Classic")
	END_CUE
	START_CUE(801001, "Green_Hill_Classic_SpdUp")
	END_CUE
	START_CUE(801002, "Green_Hill_Generic")
		TRACK("EMBB007_GHZ_3D_Normal_wav.aax")
		TRACK("EMBB008_GHZ_3D_Fast_wav.aax")
		TRACK("EMBB008_GHZ_3D_Fast_FXd_wav.aax")
	END_CUE
END_ACB

#undef START_ACB
#undef START_CUE
#undef TRACK
#undef END_CUE
#undef END_ACB
