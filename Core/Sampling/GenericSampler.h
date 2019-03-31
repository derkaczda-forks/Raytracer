#pragma once

#include "../RayLib.h"
#include "../Math/Vector4.h"

namespace rt {

namespace math {
class Random;
} // math

class GenericSampler
{
public:
    static constexpr Uint32 MaxDimension = 1024;

    GenericSampler(math::Random& fallbackGenerator);
    ~GenericSampler();

    void ResetFrame(const std::vector<float>& seed);

    void ResetPixel(const Uint32 salt);

    // get next sample
    float GetFloat();
    const math::Float2 GetFloat2();
    const math::Float3 GetFloat3();

private:

    Uint32 mDimension;
    Uint32 mSalt;
    Uint32 mSamplesGenerated;

    math::Random& mFallbackGenerator;

    float mSeed[MaxDimension];
};


} // namespace rt
