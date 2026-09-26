/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QImage>
#include <algorithm>
#include <cstdint>

namespace ShaderCanvas
{
constexpr unsigned MaxDimension = 4096;
constexpr uint64_t MaxPixels = 8ull * 1024 * 1024;
inline bool ValidSize(unsigned w, unsigned h)
{
    return w && h && w <= MaxDimension && h <= MaxDimension && uint64_t(w)*h <= MaxPixels;
}
inline QRgb Sample(const QImage& image, unsigned x, unsigned y, unsigned width, unsigned height)
{
    if(image.isNull() || !width || !height) return qRgb(0,0,0);
    const auto px = std::min<unsigned>(image.width()-1, ((uint64_t(x)*2+1)*image.width())/(uint64_t(width)*2));
    const auto py = std::min<unsigned>(image.height()-1, ((uint64_t(y)*2+1)*image.height())/(uint64_t(height)*2));
    return image.pixel(px,py);
}
// Matches ColorUtils::apply_adjustments, computed once into channel lookup tables.
inline QImage AdjustedBGRA(const QImage& source, float brightness, int temperature, int tint)
{
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    unsigned char table[3][256];
    const double factors[] = {1.0 + temperature/255.0, 1.0 + tint/255.0, 1.0 - temperature/255.0};
    brightness = std::clamp(brightness, 0.0f, 1.0f);
    for(int c=0;c<3;++c) for(int v=0;v<256;++v)
        table[c][v]=static_cast<unsigned char>(static_cast<int>(std::clamp<int>(v*factors[c],0,255)*brightness));
    for(int y=0;y<result.height();++y)
    {
        auto* pixels = reinterpret_cast<QRgb*>(result.scanLine(y));
        for(int x=0;x<result.width();++x)
            pixels[x]=qRgb(table[0][qRed(pixels[x])],table[1][qGreen(pixels[x])],table[2][qBlue(pixels[x])]);
    }
    return result;
}
}
