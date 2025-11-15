#ifndef HASHER_H
#define HASHER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>


#define XXH_PRIME64_1 11400714785074694791ULL
#define XXH_PRIME64_2 14029467366897019727ULL
#define XXH_PRIME64_3 1609587929392839161ULL
#define XXH_PRIME64_4 9650029242287828579ULL
#define XXH_PRIME64_5 2870177450012600261ULL


#define XXH_PREFETCH_L1_DISTANCE 64     // L1 cache - for small jumps
#define XXH_PREFETCH_L2_DISTANCE 128    // L2 cache - medium distance
#define XXH_PREFETCH_L3_DISTANCE 256    // L3 cache - large datasets
#define XXH_PREFETCH_AGGRESSIVE 512     // Very large datasets

#ifdef _MSC_VER
	#include <intrin.h>
#endif

#if defined(__SSE4_2__)|| defined(__SSE4_1__) 
	#define SSE4
	#include <immintrin.h>
#endif

#if defined(__GNUC__) || defined(__clang__)

#define XXH_PREFETCH(ptr) __builtin_prefetch((ptr), 0, 0)
#define XXH_PREFETCH_WRITE(ptr) __builtin_prefetch((ptr), 1, 0)
#define XXFORCE_INLINE __attribute__((always_inline)) inline
#define XXUNLIKELY(x) __builtin_expect(!!(x), 0)
#define XXLIKELY(x) __builtin_expect(!!(x), 1)
#define XXCOLD __attribute__((cold))
#define XXNO_INLINE __attribute__((noinline))

#define XXALLIGNAS(x) __attribute__ ((aligned (x)))

#elif defined(__MSC_VER)

#define XXH_PREFETCH(ptr) _mm_prefetch((const char*)(ptr), _MM_HINT_T0)
#define XXH_PREFETCH_WRITE(ptr) _mm_prefetch((const char*)(ptr), _MM_HINT_T0)
#define XXNO_INLINE __declspec(noinline)
#define XXFORCE_INLINE __forceinline
#define XXUNLIKELY(x) (x)
#define XXLIKELY(x) (x)
#define XXCOLD

#else   /*MSVC is the only crappy compiler that doesnt support this*/

#define XXH_PREFETCH(ptr) (void)(ptr)
#define XXH_PREFETCH_WRITE(ptr) (void)(ptr)
#define XXNO_INLINE
#define XXFORCE_INLINE static inline
#define XXUNLIKELY(x) (x)
#define XXLIKELY(x) (x)
#define XXCOLD

#endif

XXFORCE_INLINE uint64_t rotl64(uint64_t value, int shift)
{
	#if defined(_MSC_VER)
	return _rotl64(value, shift);
	#else
	return (value << shift) | (value >> (64 - shift));
	#endif
}


XXFORCE_INLINE uint64_t xxh64_round(uint64_t acc, uint64_t input) 
{
	acc += input * XXH_PRIME64_2;
	acc = rotl64(acc, 31);
	acc *= XXH_PRIME64_1;
	return acc;
}



XXFORCE_INLINE uint64_t xxh64_merge_round(uint64_t acc, uint64_t val) 
{
	acc ^= xxh64_round(0, val);
	acc = acc * XXH_PRIME64_1 + XXH_PRIME64_4;
	return acc;
}

#ifdef SSE4 

XXFORCE_INLINE __m128i xxh_rotl_epi64(__m128i x, int r) 
{ 
	__m128i left = _mm_slli_epi64(x, r); 
	__m128i right = _mm_srli_epi64(x, 64 - r); 
	return _mm_or_si128(left, right); 
} 

XXFORCE_INLINE __m128i xxh_mul_epi64(__m128i x, uint64_t p)
{
	__m128i prime = _mm_set1_epi64x(p);
	__m128i lo = _mm_mul_epu32(x, prime);
	__m128i hi = _mm_mul_epu32(_mm_srli_epi64(x, 32), prime);
	return _mm_add_epi64(lo, _mm_slli_epi64(hi, 32));
}


XXFORCE_INLINE __m128i xxh_round_sse(__m128i acc, __m128i input) 
{ 
	acc = _mm_add_epi64(acc, input); 
	acc = xxh_rotl_epi64(acc,31); 
	acc = xxh_mul_epi64(acc, XXH_PRIME64_1); 
	return acc;
} 


XXFORCE_INLINE __m128i xxh_merge_round_sse(__m128i acc, __m128i input)
{
	acc = _mm_xor_si128(acc, xxh_round_sse(_mm_set1_epi64x(0), input));
	acc = xxh_mul_epi64(acc, XXH_PRIME64_1);
	acc = _mm_add_epi64(acc, _mm_set1_epi64x(XXH_PRIME64_4));
	return acc;
}

#endif


XXFORCE_INLINE uint64_t xxh64_final_mix(uint64_t hash)
{
    hash ^= hash >> 33;
    hash *= XXH_PRIME64_2;
    hash ^= hash >> 29;
    hash *= XXH_PRIME64_3;
    hash ^= hash >> 32;
    return hash;
}

XXCOLD XXNO_INLINE static uint64_t xxh64_small(const void* restrict input, uint64_t len, uint64_t seed) 
{
	const unsigned char* p = (const unsigned char*)input;
	uint64_t hash = seed + XXH_PRIME64_5 + len;

	if (len >= 8) 
	{
        	uint64_t k1 = *(const uint64_t*)p; p+=8;
        	p += 8;
        	hash ^= xxh64_round(0, k1);
        	hash = rotl64(hash, 27) * XXH_PRIME64_1 + XXH_PRIME64_4;
	}
	if (len & 4) 
	{
		uint32_t k2 = *(const uint32_t*)p; p+=4;
		hash ^= (uint64_t)k2 * XXH_PRIME64_1;
		hash = rotl64(hash, 23) * XXH_PRIME64_2 + XXH_PRIME64_3;
		p += 4;
	}
	while (p < (const unsigned char*)input + len) 
	{
		hash ^= (*p) * XXH_PRIME64_5;
		hash = rotl64(hash, 11) * XXH_PRIME64_1;
		p++;
	}
	return xxh64_final_mix(hash);
}

static uint64_t xxhash64(const void* restrict input, uint64_t len, uint64_t seed)
{
	if (XXUNLIKELY(len < 32)) return xxh64_small(input, len, seed);
	
	const unsigned char* XXALLIGNAS(16) restrict p = (const unsigned char*) input;
	const unsigned char* XXALLIGNAS(8) restrict b_end = p + len;
	
	uint64_t hash_64;
	

	if (XXLIKELY(len >= 32))
	{
		const unsigned char* limit = b_end - 32;
		
		#ifndef SSE4
        	uint64_t v1 = seed + XXH_PRIME64_1 + XXH_PRIME64_2;
        	uint64_t v2 = seed + XXH_PRIME64_2;
        	uint64_t v3 = seed;
        	uint64_t v4 = seed - XXH_PRIME64_1;
		#else

		__m128i XXALLIGNAS(16) v12 = _mm_set_epi64x(seed + XXH_PRIME64_1 *XXH_PRIME64_2, seed + XXH_PRIME64_2);
		__m128i XXALLIGNAS(16) v34 = _mm_set_epi64x(seed, seed - XXH_PRIME64_1);

		#endif
		do 
		{
			uint64_t remaining = b_end - p;
    			
			if (remaining > 1024)  XXH_PREFETCH(p + XXH_PREFETCH_AGGRESSIVE);
    			else if (remaining > 512) XXH_PREFETCH(p + XXH_PREFETCH_L3_DISTANCE);
    			else if (remaining > 256) XXH_PREFETCH(p + XXH_PREFETCH_L2_DISTANCE);
    			else if (remaining > 128) XXH_PREFETCH(p + XXH_PREFETCH_L1_DISTANCE);

			#ifdef SSE4

			uint64_t k1,k2,k3,k4;
			__m128i k12 = _mm_load_si128((const __m128i*)p); p += 16;
			__m128i k34 = _mm_load_si128((const __m128i*)p); p += 16;

			v12 = xxh_round_sse( v12, k12);
			v34 = xxh_round_sse( v34, k34);

			#else
    
			k1 = *(const uint64_t*)p; p += 8;
			k2 = *(const uint64_t*)p; p += 8;
			k3 = *(const uint64_t*)p; p += 8;
			k4 = *(const uint64_t*)p; p += 8;

			v1 = xxh64_round(v1, k1);
			v2 = xxh64_round(v2, k2);
			v3 = xxh64_round(v3, k3);
			v4 = xxh64_round(v4, k4);

			#endif

		} while (XXLIKELY(p <= limit));

		#ifdef SSE4

		uint64_t* XXALLIGNAS(16) tmp = ((uint64_t*)&v12);
		uint64_t* XXALLIGNAS(16) tmp2 = ((uint64_t*)&v34);
		hash_64 = rotl64(tmp[0], 1) + rotl64(tmp[1], 7) + rotl64(tmp2[0], 12) + rotl64(tmp2[1], 18);

		__m128i acc = _mm_set1_epi64x(hash_64);

		acc = xxh_merge_round_sse(acc, v12);
		acc = xxh_merge_round_sse(acc, v34);

		uint64_t XXALLIGNAS(16) hash_out[2];
		_mm_store_si128((__m128i*)hash_out, acc);

		hash_64 = hash_out[0] ^ hash_out[1];

		#else 

		hash_64 = rotl64(v1, 1) + rotl64(v2, 7) + rotl64(v3, 12) + rotl64(v4, 18);
		
		hash_64 = xxh64_merge_round(hash_64, v1);
		hash_64 = xxh64_merge_round(hash_64, v2);
		hash_64 = xxh64_merge_round(hash_64, v3);
		hash_64 = xxh64_merge_round(hash_64, v4);

		#endif


	} else hash_64 = seed + XXH_PRIME64_5;

	hash_64 += len;

	if (XXLIKELY(p + 8 <= b_end)) 
	{
		uint64_t k1 = *(const uint64_t*)p; p+=8;
		uint64_t k1_round = xxh64_round(0, k1);
		hash_64 ^= k1_round;
		hash_64 = rotl64(hash_64, 27) * XXH_PRIME64_1* XXH_PRIME64_4;
	}


	if (XXLIKELY(p + 4 <= b_end))
	{
        	uint32_t k2 = *(const uint32_t*)p; p += 4;
        	hash_64 ^= (uint64_t)k2 * XXH_PRIME64_1;
        	hash_64 = rotl64(hash_64, 23) * XXH_PRIME64_2 * XXH_PRIME64_3;
	}


	while (XXUNLIKELY(p < b_end))
	{
		hash_64 ^= (uint64_t)(*p) * XXH_PRIME64_5;
		hash_64 = rotl64(hash_64, 11) * XXH_PRIME64_1;
		p++;
	}

	return xxh64_final_mix(hash_64);

}

#ifdef __cplusplus
|
#endif

#endif
