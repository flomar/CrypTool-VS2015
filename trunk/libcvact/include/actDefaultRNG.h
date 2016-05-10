//////////////////////////////////////////////////////////////////////////////////
// Name:      actDefaultRNG.h
// Product:   cv act library
// Purpose:   actDefaultRNG.h provides defaults for the class IRNGAlg.
//
// Copyright: (c) 2011 cv cryptovision GmbH
//            all rights reserved
// Licence:   The conditions for the use of this software are regulated 
//            in the cv act library licence agreement.
//////////////////////////////////////////////////////////////////////////////////

#ifndef ACT_DefaultRNG_h
#define ACT_DefaultRNG_h

namespace act
{
	class IRNGAlg;
	typedef IRNGAlg* (*CreateRNGPtr)();

	extern CreateRNGPtr CreateFastRNG;
	extern CreateRNGPtr CreateStrongRNG;

	// force default RNGs singleton initialization (including seed polling)
	extern void InitDefaultRNGs(bool initFastRNG = true, bool initStrongRNG = true);
}

#endif

