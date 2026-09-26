/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "WindowsCaptureSupport.h"
#include "WindowsCaptureImagePool.h"
#include "WindowsScreenCapturer.h"
#include <QCoreApplication>
#include <array>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#define CHECK(value) do { if(!(value)) throw std::runtime_error(#value); } while(0)
using namespace WindowsCapture;

void TestPixels()
{
    // 3x2 numbered pixels, 4 bytes of non-pixel padding on each input row.
    std::array<unsigned char, 32> source;
    source.fill(0xE7);
    for(unsigned int y = 0; y < 2; ++y)
        for(unsigned int x = 0; x < 3; ++x)
        {
            auto* pixel = source.data() + y * 16 + 4 * x;
            pixel[0] = static_cast<unsigned char>(1 + y * 3 + x);
            pixel[1] = 17;
            pixel[2] = 23;
            pixel[3] = 0;
        }
    const auto original = source;
    const std::array<std::array<unsigned char, 6>, 4> expected{{
        {{1,2,3,4,5,6}}, {{4,1,5,2,6,3}}, {{6,5,4,3,2,1}}, {{3,6,2,5,1,4}}
    }};
    const Rotation rotations[] = {Rotation::Identity, Rotation::Clockwise90, Rotation::Clockwise180, Rotation::Clockwise270};
    for(unsigned int r = 0; r < 4; ++r)
    {
        const unsigned int width = SwapsAxes(rotations[r]) ? 2 : 3;
        const unsigned int height = SwapsAxes(rotations[r]) ? 3 : 2;
        const std::size_t pitch = width * 4 + 8;
        std::vector<unsigned char> destination(pitch * height + 8, 0xCD);
        CHECK(CopyBgra(source.data(), 16, 3, 2, rotations[r], destination.data(), pitch));
        for(unsigned int y = 0; y < height; ++y)
        {
            for(unsigned int x = 0; x < width; ++x)
            {
                const auto* pixel = destination.data() + pitch * y + 4 * x;
                CHECK(pixel[0] == expected[r][y * width + x]);
                CHECK(pixel[1] == 17 && pixel[2] == 23 && pixel[3] == 255);
            }
            for(std::size_t p = width * 4; p < pitch; ++p) CHECK(destination[pitch * y + p] == 0xCD);
        }
        for(std::size_t p = pitch * height; p < destination.size(); ++p) CHECK(destination[p] == 0xCD);
    }
    CHECK(source == original);
    std::array<unsigned char, 32> output{};
    CHECK(!CopyBgra(nullptr, 16, 3, 2, Rotation::Identity, output.data(), 16));
    CHECK(!CopyBgra(source.data(), 8, 3, 2, Rotation::Identity, output.data(), 16));
    CHECK(!CopyBgra(source.data(), 16, 3, 2, Rotation::Identity, output.data(), 8));
    CHECK(!CopyBgra(source.data(), 16, 0, 2, Rotation::Identity, output.data(), 16));
    CHECK(!ValidSize(0, 1080) && !ValidSize(16385, 1) && !ValidSize(16384, 16384));
    CHECK(ValidSize(7680, 4320));
}

void TestFrameLease()
{
    struct FakeDuplication { int releases = 0; long ReleaseFrame() { ++releases; return -17; } } fake;
    { FrameLease<FakeDuplication> frame(&fake); }
    CHECK(fake.releases == 1);
    { FrameLease<FakeDuplication> frame(&fake); CHECK(frame.Release() == -17); }
    CHECK(fake.releases == 2);
    try { FrameLease<FakeDuplication> frame(&fake); throw std::runtime_error("simulated staging failure"); }
    catch(const std::runtime_error&) {}
    CHECK(fake.releases == 3);
}

void TestRetryAndRate()
{
    RetryPolicy retry;
    const RetryPolicy::Time t{};
    CHECK(retry.ShouldTryDxgi(t));
    retry.Opened();
    CHECK(retry.UsesDxgi() && !retry.ShouldTryDxgi(t));
    retry.Failed(t); // ACCESS_LOST/device removed/open failed all follow same path
    CHECK(!retry.UsesDxgi());
    CHECK(!retry.ShouldTryDxgi(t + std::chrono::milliseconds(4999)));
    CHECK(retry.ShouldTryDxgi(t + std::chrono::seconds(5)));
    retry.Failed(t + std::chrono::seconds(5));
    CHECK(!retry.ShouldTryDxgi(t + std::chrono::seconds(9)));
    retry.Reset(); // a newly selected display must not inherit the old cooldown
    CHECK(retry.ShouldTryDxgi(t));
    CHECK(FramePeriod(0, false).count() == 1000000);
    CHECK(FramePeriod(60, false).count() == 16666);
    CHECK(FramePeriod(5000, false).count() == 4166);
    CHECK(FramePeriod(60, true).count() == 66666);
    CHECK(FramePeriod(5, true).count() == 200000);
}

void TestImageOwnership()
{
    ImagePool pool;
    std::array<QImage, 3> retained;
    for(int i = 0; i < 3; ++i)
    {
        QImage* frame = pool.Writable(8, 4);
        CHECK(frame != nullptr);
        frame->fill(qRgb(i * 40, 15, 30));
        retained[i] = *frame;
    }
    CHECK(pool.Writable(8, 4) == nullptr); // no mutation or fourth allocation
    const auto* address = retained[1].constBits();
    retained[1] = QImage();
    QImage* reused = pool.Writable(8, 4);
    CHECK(reused && reused->constBits() == address);
    reused->fill(qRgb(255, 0, 0));
    CHECK(retained[0].pixel(0, 0) == qRgb(0, 15, 30));
    CHECK(retained[2].pixel(0, 0) == qRgb(80, 15, 30));
    QImage* resized = pool.Writable(4, 8);
    CHECK(resized && resized->size() == QSize(4, 8));
    CHECK(retained[0].size() == QSize(8, 4));
}

void TestIdleLifecycle()
{
    // No QGuiApplication and never SetScreen: no output is enumerated, opened,
    // duplicated or read. Only the empty-target worker and atomic settings run.
    WindowsScreenCapturer capturer;
    std::atomic<int> frames{0};
    QObject::connect(&capturer, &ScreenCapturer::OnImage, &capturer, [&](const QImage&) { ++frames; }, Qt::DirectConnection);
    std::atomic<bool> changing{true};
    std::thread writer([&]
    {
        unsigned int value = 0;
        while(changing.load()) capturer.SetFrameRate(value++ % 241);
    });
    const auto began = std::chrono::steady_clock::now();
    for(int i = 0; i < 20; ++i)
    {
        capturer.Start();
        capturer.Start();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        capturer.Stop();
        capturer.Stop();
    }
    changing.store(false);
    writer.join();
    CHECK(frames.load() == 0);
    CHECK(std::chrono::steady_clock::now() - began < std::chrono::seconds(3));
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try
    {
        TestPixels();
        TestFrameLease();
        TestRetryAndRate();
        TestImageOwnership();
        TestIdleLifecycle();
        std::cout << "PASS: pixels/4 rotations/pitches, frame lease, retry/rate policy, bounded immutable image pool, idle lifecycle. No display captured.\n";
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
