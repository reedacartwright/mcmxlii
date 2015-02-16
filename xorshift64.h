#pragma once
#ifndef XORSHIFT64_H
#define XORSHIFT64_H
/****************************************************************************
 *  Copyright (C) 2009-2015 Reed A. Cartwright, PhD <reed@cartwrig.ht>      *
 ****************************************************************************/

#ifndef __STDC_CONSTANT_MACROS
#	define __STDC_CONSTANT_MACROS 1
#endif
#ifndef __STDC_LIMIT_MACROS
#	define __STDC_LIMIT_MACROS 1
#endif

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#	include <sys/param.h>
#endif

#if defined(__APPLE__) && defined(__MACH__)
#	include <machine/endian.h>
#elif defined(BSD)
#	include <sys/endian.h>
#else
#	include <endian.h>
#endif

#if defined(_WIN64) || defined(_WIN32)
#	include <process.h>
 	inline int getpid() { return _getpid(); }
#else
#	include <unistd.h>
#endif


#include <cfloat>
#include <cstdint>
#include <algorithm>

#if __cpluscplus >= 201103L
#	include <random>
#endif

/*
A 64-bit Xorshift PRNG combined a Weyl Generator.
From ideas of Marsgalia, Brent, and others.

Passes all the BigCrush tests.

References:
http://www.jstatsoft.org/v08/i14/paper
http://wwwmaths.anu.edu.au/~brent/random.html
http://www.iro.umontreal.ca/~panneton/These.pdf
*/

class xorshift64 {
public:
	xorshift64(uint64_t seed1 = 0, uint64_t seed2 = 0) {	
		seed(seed1,seed2);
	}

	explicit xorshift64(std::pair<uint64_t,uint64_t> p) {
		seed(p.first,p.second);
	}

	void seed(uint64_t seed1 = 0, uint64_t seed2 = 0) {
		u = (seed1 == 0) ? UINT64_C(15191868757011070976) : seed1;
		w = (seed2 == 0) ? UINT64_C(0x61C8864680B583EB) : seed2;
		// Burn in the seed
		for(int i=0;i<256;++i)
			get_raw();
	}

	void seed(std::pair<uint64_t,uint64_t> p) {
		seed(p.first,p.second);
	}
	
	std::pair<uint64_t,uint64_t> get_state() const {
		return std::make_pair(u,w);
	}
	
	// Xorshift + Weyl Generator + some extra magic for low bits
	uint64_t get_raw() {
		u ^= (u << 5); u ^= (u >> 15); u ^= (u << 27);
		w += UINT64_C(0x61C8864680B583EB);
		return u+(w^(w>>27));
	}
	
	uint64_t get_uint64() {
		return get_raw();
	}
	
	uint32_t get_uint32() {
		return static_cast<uint32_t>(get_raw() >> 32);
	}

	std::pair<uint32_t,uint32_t> get_uint32_pair() {
		uint64_t u = get_raw();
		return std::make_pair(
			static_cast<uint32_t>(u >> 32),
			static_cast<uint32_t>(u)
		);
	}

	
	// Uniform [0,n) with 64-bits of precision
	// If n is a power of two, this only uses not-well-distributed low-bits
	uint64_t get_uint64(uint64_t n) {
		return get_uint64() % n;
	}
	
	// Uniform [0,n) exactly
	// This uses low bits which are not very random
	uint64_t get_uint64x(uint64_t n) {
		// Find a mask
		uint64_t v = n;
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v |= v >> 32;
		uint64_t u;
		do {
			u = get_uint64() & v;
		} while( u >= n);
		return u;
	}

	// Uniform [0,n) exactly
	// This uses high bits, but might not be as fast as get_uint64x
	uint64_t get_uint64z(uint64_t n) {
		// Find a shift
		uint64_t v = n-1;
		int r = 64;
		while(v >>= 1) {
			r--;
		}
		uint64_t u;
		do {
			u = get_uint64() >> r;
		} while( u >= n);
		return u;
	}

#if __FLOAT_WORD_ORDER == __BYTE_ORDER
	// Uniform [0,1)
	double get_double53() {
		union { uint64_t u; double d; } a;
		a.u = get_raw();
		a.u = (a.u >> 12) | UINT64_C(0x3FF0000000000000);
		double q = (a.u&2048) ? (1.0-(DBL_EPSILON/2.0)) : 1.0;
		return a.d-q;
	}

	// Uniform (0,1)
	double get_double52() {
		union { uint64_t u; double d; } a;
		a.u = get_raw();
		a.u = (a.u >> 12) | UINT64_C(0x3FF0000000000000);
		double q = (1.0-(DBL_EPSILON/2.0));
		return a.d-q;
	}
#else
	double get_double53() {
		uint64_t u = get_raw();
		int64_t n = static_cast<int64_t>(u >> 11);
		return n/9007199254740992.0;
	}
	double get_double52() {
		uint64_t u = get_raw();
		int64_t n = static_cast<int64_t>(u >> 11) | 0x1;
		return n/9007199254740992.0;
	}
#endif	
	
	// Uniform [0,max)
	uint64_t operator()() {
		return get_uint64();
	}
	
	// Uniform [0,n) with 64-bits of precision
	uint64_t operator()(uint64_t n) {
		return get_uint64(n);
	}
	
private:
	uint64_t u,w;
};

inline unsigned int create_random_seed() {
	// start with some well mixed bits
	unsigned int v = 0x6ba658b3;	
#if __cpluscplus >= 201103L
	// if properly implemented, this will produce 32 random bits
	v += std::random_device{}();
#endif
	// use 5-decimal pid for some randomness after spreading over 32-bits
	unsigned int p = static_cast<unsigned int>(getpid());
	v +=  p + (p << 15) + (p >> 3);
	v^=(v<<17); v^=(v>>13); v^=(v<<5);
	// use current time for more randomness
	v += static_cast<unsigned int>(time(NULL));
	v^=(v<<17); v^=(v>>13); v^=(v<<5);

	// Do four rounds of burn in.
	v^=(v<<17); v^=(v>>13); v^=(v<<5);
	v^=(v<<17); v^=(v>>13); v^=(v<<5);
	v^=(v<<17); v^=(v>>13); v^=(v<<5);
	v^=(v<<17); v^=(v>>13); v^=(v<<5);

	// return a 31-bit seed that is not zero
	v &= 0x7FFFFFFF; 
	return (v == 0) ? 0x6a27d958 : v;
}

#endif
