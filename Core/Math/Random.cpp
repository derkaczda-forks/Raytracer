#include "PCH.h"
#include "Random.h"
#include "Transcendental.h"

namespace rt {
namespace math {

static RT_FORCE_INLINE Uint64 Rotl64(const Uint64 x, const int k)
{
    return (x << k) | (x >> (64 - k));
}

Random::Random()
{
    Reset();
}

void Random::Reset()
{
    for (Uint32 i = 0; i < 2; ++i)
    {
        mSeed[i] = ((Uint64)GetEntropy() << 32) | (Uint64)GetEntropy();
        mSeedSimd4[i] = VectorInt4(GetEntropy(), GetEntropy(), GetEntropy(), GetEntropy());
#ifdef RT_USE_AVX2
        mSeedSimd8[i] = VectorInt8(GetEntropy(), GetEntropy(), GetEntropy(), GetEntropy(), GetEntropy(), GetEntropy(), GetEntropy(), GetEntropy());
#endif // RT_USE_AVX2
    }
}

Uint32 Random::GetEntropy()
{
    Uint32 val = 0;
    while (_rdrand32_step(&val) == 0) {}
    return val;
}

Uint64 Random::GetLong()
{
    // xoroshiro128+ algorithm
    // http://xoshiro.di.unimi.it/xoroshiro128plus.c

    const Uint64 s0 = mSeed[0];
    Uint64 s1 = mSeed[1];
    const Uint64 result = s0 + s1;

    s1 ^= s0;
    mSeed[0] = Rotl64(s0, 24) ^ s1 ^ (s1 << 16);
    mSeed[1] = Rotl64(s1, 37);

    return result;
}

Uint32 Random::GetInt()
{
    return static_cast<Uint32>(GetLong());
}

float Random::GetFloat()
{
    Bits32 myrand;
    myrand.ui = (GetInt() & 0x007fffffu) | 0x3f800000u;
    return myrand.f - 1.0f;
}

double Random::GetDouble()
{
    return static_cast<double>(GetLong()) / static_cast<double>(std::numeric_limits<Uint64>::max());
}

const Float2 Random::GetFloat2()
{
    return GetVector4().ToFloat2();
}

const Float3 Random::GetFloat3()
{
    return GetVector4().ToFloat3();
}

float Random::GetFloatBipolar()
{
    Bits32 myrand;
    myrand.ui = (GetInt() & 0x007fffff) | 0x40000000;
    return myrand.f - 3.0f;
}

VectorInt4 Random::GetIntVector4()
{
    // NOTE: xoroshiro128+ is faster when using general purpose registers, because there's
    // no rotate left/right instruction in SSE2 (it's only in AVX512)

    // xorshift128+ algorithm
    const VectorInt4 s0 = mSeedSimd4[1];
    VectorInt4 s1 = mSeedSimd4[0];
    VectorInt4 v = _mm_add_epi64(s0, s1);
    s1 = _mm_slli_epi64(s1, 23);
    const VectorInt4 t0 = _mm_srli_epi64(s0, 5);
    const VectorInt4 t1 = _mm_srli_epi64(s1, 18);
    mSeedSimd4[0] = s0;
    mSeedSimd4[1] = (s0 ^ s1) ^ (t0 ^ t1);
    return v;
}

const Vector4 Random::GetVector4()
{
    VectorInt4 v = GetIntVector4();

    // setup float mask
    v &= VectorInt4(0x007fffffu);
    v |= VectorInt4(0x3f800000u);

    // convert to float and go from [1, 2) to [0, 1) range
    return v.CastToFloat() - VECTOR_ONE;
}

const Vector4 Random::GetVector4Bipolar()
{
    VectorInt4 v = GetIntVector4();

    // setup float mask
    v &= VectorInt4(0x007fffffu);
    v |= VectorInt4(0x40000000u);

    // convert to float and go from [2, 4) to [-1, 1) range
    return v.CastToFloat() - Vector4(3.0f);
}

#ifdef RT_USE_AVX2

VectorInt8 Random::GetIntVector8()
{
    // NOTE: xoroshiro128+ is faster when using general purpose registers, because there's
    // no rotate left/right instruction in AVX2 (it's only in AVX512)

    // xorshift128+ algorithm
    const VectorInt8 s0 = mSeedSimd8[1];
    VectorInt8 s1 = mSeedSimd8[0];
    VectorInt8 v = _mm256_add_epi64(s0, s1);
    s1 = _mm256_slli_epi64(s1, 23);
    const VectorInt8 t0 = _mm256_srli_epi64(s0, 5);
    const VectorInt8 t1 = _mm256_srli_epi64(s1, 18);
    mSeedSimd8[0] = s0;
    mSeedSimd8[1] = (s0 ^ s1) ^ (t0 ^ t1);

    return v;
}

#else

VectorInt8 Random::GetIntVector8()
{
    return VectorInt8{ GetIntVector4(), GetIntVector4() };
}

#endif // RT_USE_AVX2

const Vector8 Random::GetVector8()
{
    VectorInt8 v = GetIntVector8();

    // setup float mask
    v &= VectorInt8(0x007fffffu);
    v |= VectorInt8(0x3f800000u);

    // convert to float and go from [1, 2) to [0, 1) range
    return v.CastToFloat() - VECTOR8_ONE;
}

const Vector8 Random::GetVector8Bipolar()
{
    VectorInt8 v = GetIntVector8();

    // setup float mask
    v &= VectorInt8(0x007fffffu);
    v |= VectorInt8(0x40000000u);

    // convert to float and go from [1, 2) to [0, 1) range
    return v.CastToFloat() - Vector8(3.0f);
}

} // namespace math
} // namespace rt
