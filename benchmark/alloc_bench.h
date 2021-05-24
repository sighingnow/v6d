#ifndef ALLOC_TEST_H
#define ALLOC_TEST_H

#include <stdio.h>

#include <cstdint>
#include <iostream>

#define FORCE_INLINE inline

class PRNG
{
	uint64_t seedVal;
public:
	PRNG() { seedVal = 0; }
	PRNG( size_t seed_ ) { seedVal = seed_; }
	void seed( size_t seed_ ) { seedVal = seed_; }

	/*FORCE_INLINE uint32_t rng32( uint32_t x )
	{
		// Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		return x;
	}*/
/*	FORCE_INLINE uint32_t rng32()
	{
		unsigned long long x = (seedVal += 7319936632422683443ULL);	
		x ^= x >> 32;
		x *= c;
		x ^= x >> 32;
		x *= c;
		x ^= x >> 32;
        return uint32_t(x);
	}*/
	FORCE_INLINE uint32_t rng32()
	{
		// based on implementation of xorshift by Arvid Gerstmann
		// see, for instance, https://arvid.io/2018/07/02/better-cxx-prng/
        uint64_t ret = seedVal * 0xd989bcacc137dcd5ull;
        seedVal ^= seedVal >> 11;
        seedVal ^= seedVal << 31;
        seedVal ^= seedVal >> 18;
        return uint32_t(ret >> 32ull);
	}

	FORCE_INLINE uint64_t rng64()
	{
        uint64_t ret = rng32();
		ret <<= 32;
		return ret + rng32();
	}
};

FORCE_INLINE size_t calcSizeWithStatsAdjustment( uint64_t randNum, size_t maxSizeExp )
{
	assert( maxSizeExp >= 3 );
	maxSizeExp -= 3;
	uint32_t statClassBase = (randNum & (( 1 << maxSizeExp ) - 1)) + 1; // adding 1 to avoid dealing with 0
	randNum >>= maxSizeExp;
	unsigned long idx;
	idx = __builtin_ctzll( statClassBase );
	assert( idx <= maxSizeExp );
	idx += 2;
	size_t szMask = ( 1 << idx ) - 1;
	return (randNum & szMask) + 1 + (((size_t)1)<<idx);
}

constexpr double Pareto_80_20_6[7] = {
	0.262144000000,
	0.393216000000,
	0.245760000000,
	0.081920000000,
	0.015360000000,
	0.001536000000,
	0.000064000000};

struct Pareto_80_20_6_Data
{
	uint32_t probabilityRanges[6];
	uint32_t offsets[8];
};

FORCE_INLINE
void Pareto_80_20_6_Init( Pareto_80_20_6_Data& data, uint32_t itemCount )
{
	data.probabilityRanges[0] = (uint32_t)(UINT32_MAX * Pareto_80_20_6[0]);
	data.probabilityRanges[5] = (uint32_t)(UINT32_MAX * (1. - Pareto_80_20_6[6]));
	for ( size_t i=1; i<5; ++i )
		data.probabilityRanges[i] = data.probabilityRanges[i-1] + (uint32_t)(UINT32_MAX * Pareto_80_20_6[i]);
	data.offsets[0] = 0;
	data.offsets[7] = itemCount;
	for ( size_t i=0; i<6; ++i )
		data.offsets[i+1] = data.offsets[i] + (uint32_t)(itemCount * Pareto_80_20_6[6-i]);
}

FORCE_INLINE
size_t Pareto_80_20_6_Rand( const Pareto_80_20_6_Data& data, uint32_t rnum1, uint32_t rnum2 )
{
	size_t idx = 6;
	if ( rnum1 < data.probabilityRanges[0] )
		idx = 0;
	else if ( rnum1 < data.probabilityRanges[1] )
		idx = 1;
	else if ( rnum1 < data.probabilityRanges[2] )
		idx = 2;
	else if ( rnum1 < data.probabilityRanges[3] )
		idx = 3;
	else if ( rnum1 < data.probabilityRanges[4] )
		idx = 4;
	else if ( rnum1 < data.probabilityRanges[5] )
		idx = 5;
	uint32_t rangeSize = data.offsets[ idx + 1 ] - data.offsets[ idx ];
	uint32_t offsetInRange = rnum2 % rangeSize;
	return data.offsets[ idx ] + offsetInRange;
}

#endif
