#pragma once

#include "RayLib.h"
#include "Math/Vector4.h"

namespace rt {

/**
 * Class representing 2D image/texture.
 */
class RAYLIB_API Bitmap
{
public:
    // TODO
    static constexpr Uint32 MaxSize = 4096;

    enum class Format
    {
        Unknown,
        B8G8R8A8_Uint,
        R32G32B32A32_Float,

        // TODO monochromatic, compressed, half-float, etc.
    };

    Bitmap();
    ~Bitmap();
    Bitmap(Bitmap&&) = default;
    Bitmap& operator = (Bitmap&&) = default;

    RT_FORCE_INLINE const void* GetData() const { return mData; }
    RT_FORCE_INLINE Uint32 GetWidth() const { return (Uint32)mWidth; }
    RT_FORCE_INLINE Uint32 GetHeight() const { return (Uint32)mHeight; }
    RT_FORCE_INLINE Format GetFormat() const { return mFormat; }

    // initialize bitmap with data (or clean if passed nullptr)
    Bool Init(Uint32 width, Uint32 height, Format format, const void* data = nullptr);

    // release memory
    void Release();

    // calculate number of bits per pixel for given format
    static size_t BitsPerPixel(Format format);

    // set single pixel
    // TODO: this probably will be too slow
    void SetPixel(Uint32 x, Uint32 y, const math::Vector4& value);

    // get single pixel
    // TODO: this probably will be too slow
    math::Vector4 GetPixel(Uint32 x, Uint32 y) const;

    // write whole pixels row
    void WriteHorizontalLine(Uint32 y, const math::Vector4* values);
    void WriteVerticalLine(Uint32 x, const math::Vector4* values);

    // read whole pixels row
    void ReadHorizontalLine(Uint32 y, math::Vector4* outValues) const;
    void ReadVerticalLine(Uint32 x, math::Vector4* outValues) const;


    // fill with zeros
    void Zero();

    // fast Box blur
    static Bool VerticalBoxBlur(Bitmap& target, const Bitmap& src, const Uint32 radius);
    static Bool HorizontalBoxBlur(Bitmap& target, const Bitmap& src, const Uint32 radius);

    static Bool Blur(Bitmap& target, const Bitmap& src, const Float sigma, const Uint32 n);

private:

    Bitmap(const Bitmap&) = delete;
    Bitmap& operator = (const Bitmap&) = delete;

    static void BoxBlur_Internal(math::Vector4* targetLine, const math::Vector4* srcLine,
                                 const Uint32 radius, const Uint32 width, const Float factor);

    void* mData;
    Uint16 mWidth;
    Uint16 mHeight;
    Format mFormat;
};


} // namespace rt