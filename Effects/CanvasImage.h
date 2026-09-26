/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QImage>
#include <QPainter>
#include <QRect>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Canvas pixels and physical LEDs are independent. A plan stores one sample per
// real LED, not one virtual LED per source pixel. Coordinates are normalized UV.
namespace effect_canvas
{
constexpr std::uint64_t MAX_PIXELS = 4096ULL * 4096;
inline bool ValidSize(int width, int height)
{
    return width >= 16 && height >= 16 && width <= 4096 && height <= 4096
        && std::uint64_t(width) * height <= MAX_PIXELS;
}

struct Region
{
    double x = 0, y = 0, width = 1, height = 1, rotation = 0;
    bool flip_x = false, flip_y = false;
};

inline bool ValidRegion(const Region& r)
{
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width)
        && std::isfinite(r.height) && std::isfinite(r.rotation)
        && std::abs(r.x) <= 16 && std::abs(r.y) <= 16 && r.width > 0 && r.height > 0
        && r.width <= 16 && r.height <= 16 && std::abs(r.rotation) <= 3600;
}

struct Sample
{
    unsigned int led;
    double u, v;
};

inline std::vector<Sample> BuildPlan(unsigned int width, unsigned int height,
    unsigned int count, const unsigned int* map, bool reverse, const Region& region)
{
    std::vector<Sample> result;
    if(!width || !height || !count || !ValidRegion(region)
       || std::uint64_t(width) * height > MAX_PIXELS || count > MAX_PIXELS) return result;
    // Repeated matrix cells retain the old last-cell-wins behavior.
    std::vector<int> indices(count, -1);
    result.reserve(std::min<std::uint64_t>(count, std::uint64_t(width) * height));
    const double radians = region.rotation * 3.14159265358979323846 / 180.0;
    const double cosine = std::cos(radians), sine = std::sin(radians);
    for(unsigned int y = 0; y < height; ++y)
    for(unsigned int x = 0; x < width; ++x)
    {
        const unsigned int led = map ? map[std::size_t(y) * width + x] : x;
        if(led >= count) continue; // Includes OpenRGB's 0xFFFFFFFF matrix holes.
        const double dx = ((((reverse != region.flip_x) ? width - x - 1 : x) + 0.5) / width - 0.5) * region.width;
        const double dy = (((region.flip_y ? height - y - 1 : y) + 0.5) / height - 0.5) * region.height;
        const Sample sample{led, region.x + region.width * 0.5 + cosine * dx - sine * dy,
                                region.y + region.height * 0.5 + sine * dx + cosine * dy};
        if(indices[led] < 0)
        {
            indices[led] = static_cast<int>(result.size());
            result.push_back(sample);
        }
        else result[indices[led]] = sample;
    }
    return result;
}

inline QRgb SamplePixel(const QImage& image, const Sample& sample)
{
    if(image.isNull() || sample.u < 0 || sample.u >= 1 || sample.v < 0 || sample.v >= 1)
        return qRgb(0, 0, 0);
    // Qt's fast scaler chooses the lower pixel on an exact centre boundary.
    const double x = sample.u * image.width() - 1e-7;
    const double y = sample.v * image.height() - 1e-7;
    return image.pixel(std::clamp(int(x), 0, image.width() - 1),
                       std::clamp(int(y), 0, image.height() - 1));
}

inline QImage Normalize(const QImage& input, bool crop, const QRect& rectangle, int width, int height)
{
    if(input.isNull() || !ValidSize(width, height)) return {};
    if(crop && (rectangle.width() <= 0 || rectangle.height() <= 0))
    {
        QImage black(width, height, QImage::Format_RGB32);
        black.fill(Qt::black);
        return black;
    }
    // Draw the source rectangle into bounded owned storage. A large/off-screen
    // crop must not allocate a second crop-sized image before reducing it.
    QImage normalized(width, height, QImage::Format_RGB32);
    normalized.fill(Qt::black);
    QPainter painter(&normalized);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(QRectF(0, 0, width, height), input, crop ? QRectF(rectangle) : QRectF(input.rect()));
    painter.end();
    return normalized;
}

template<class Adjust>
QImage PublicationImage(const QImage& source, Adjust adjust)
{
    QImage result = source.convertToFormat(QImage::Format_RGB32);
    result.detach();
    for(int y = 0; y < result.height(); ++y)
    {
        auto* pixels = reinterpret_cast<QRgb*>(result.scanLine(y));
        for(int x = 0; x < result.width(); ++x)
            pixels[x] = adjust(pixels[x]) | 0xFF000000u;
    }
    return result;
}
}
