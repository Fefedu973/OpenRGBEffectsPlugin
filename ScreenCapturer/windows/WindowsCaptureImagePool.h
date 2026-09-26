/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QImage>
#include <array>

namespace WindowsCapture
{
class ImagePool
{
public:
    QImage* Writable(int width, int height)
    {
        for(QImage& image : images)
        {
            if(!image.isNull() && !image.isDetached()) continue;
            if(image.size() != QSize(width, height)) image = QImage(width, height, QImage::Format_RGB32);
            return image.isNull() ? nullptr : &image;
        }
        /* A slow consumer must not cause unbounded allocation or overwrite
         * a frame it still owns. Drop this frame if all three are retained. */
        return nullptr;
    }
private:
    std::array<QImage, 3> images;
};
}
