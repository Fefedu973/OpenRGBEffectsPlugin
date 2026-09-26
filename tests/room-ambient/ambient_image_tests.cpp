/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "CanvasImage.h"
#include "CanvasRegions.h"
#include "CanvasRouting.h"
#include "ColorUtils.h"
#include <FrameSurface/FrameSurface.h>
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
    using namespace effect_canvas;
    unsigned int assertions = 0;
    auto check = [&](bool condition){ ++assertions; if(!condition) { std::cerr << "Failed check " << assertions << '\n'; std::abort(); } };
    check(ValidSize(800, 600));
    check(ValidSize(4096, 4096));
    check(!ValidSize(0, 600) && !ValidSize(4097, 600) && !ValidSize(800, -1));
    check(!ValidRegion({0, 0, -1, 1, 0}));
    check(!ValidRegion({0, 0, 1, 1, std::numeric_limits<double>::quiet_NaN()}));
    const nlohmann::json valid_region = {{"selector", {{"vendor","Fixture"},{"serial","test-only"},{"zone_idx",0}}},
                                       {"rect",{{"x",0.0},{"y",0.0},{"width",0.5},{"height",1.0}}}};
    auto invalid_region = valid_region;
    invalid_region["rect"]["width"] = -1;
    check(ValidRegions(nlohmann::json::array({valid_region, invalid_region})).size() == 1);
    invalid_region = valid_region;
    invalid_region["selector"]["serial"] = "";
    check(ValidRegions(nlohmann::json::array({invalid_region})).empty());
    invalid_region["selector"]["name"] = "Fixture";
    invalid_region["selector"]["location"] = "USB:synthetic";
    check(ValidRegions(nlohmann::json::array({invalid_region})).size() == 1);
    invalid_region["selector"]["is_segment"] = "bad-type";
    check(ValidRegions(nlohmann::json::array({invalid_region})).empty());
    check(ValidRegions(nlohmann::json::array({nullptr, 15, "invalid"})).empty());

    QImage source(120, 90, QImage::Format_RGB32);
    for(int y = 0; y < source.height(); ++y)
        for(int x = 0; x < source.width(); ++x) source.setPixel(x, y, qRgb(x * 2, y * 2, 51));
    QImage normalized = Normalize(source, false, {}, 800, 600);
    check(normalized.size() == QSize(800, 600));
    check(normalized.sizeInBytes() == 1920000);
    const auto samples = BuildPlan(15, 1, 15, nullptr, false, {});
    check(samples.size() == 15); // A full800x600 image does not become480000 LEDs.
    check(qRed(SamplePixel(normalized, samples.front())) < qRed(SamplePixel(normalized, samples.back())));
    const auto reversed = BuildPlan(15, 1, 15, nullptr, true, {});
    check(SamplePixel(normalized, samples.front()) == SamplePixel(normalized, reversed.back()));

    // Preserve Qt's nearest-neighbour zone mapping on representative odd/even sizes.
    for(const QSize size : {QSize(1,1), QSize(7,1), QSize(15,1), QSize(7,5), QSize(30,12)})
    {
        std::vector<unsigned int> map(size.width() * size.height());
        for(unsigned int i = 0; i < map.size(); ++i) map[i] = i;
        const auto plan = BuildPlan(size.width(), size.height(), unsigned(map.size()), map.data(), false, {});
        const QImage legacy = normalized.scaled(size);
        for(const auto& sample : plan)
            check(SamplePixel(normalized, sample) == legacy.pixel(sample.led % size.width(), sample.led / size.width()));
    }

    const unsigned int sparse[] = {0, 0xFFFFFFFFu, 1, 99, 1, 2};
    const auto sparse_plan = BuildPlan(3, 2, 3, sparse, false, {});
    check(sparse_plan.size() == 3);
    check(sparse_plan[1].v == 0.75); // Repeated LED1 uses the final cell; holes/invalid LEDs skipped.
    const auto outside = BuildPlan(1, 1, 1, nullptr, false, {2, 0, 1, 1, 0});
    check(SamplePixel(normalized, outside.front()) == qRgb(0,0,0));
    const auto left_half = BuildPlan(2, 1, 2, nullptr, false, {0,0,0.5,1,0});
    check(left_half.back().u == 0.375);
    const auto rotated = BuildPlan(2, 1, 2, nullptr, false, {0,0,1,1,180});
    check(std::abs(rotated.front().u - 0.75) < 1e-9);
    const auto flipped = BuildPlan(2, 1, 2, nullptr, false, {0,0,1,1,0,true,false});
    check(flipped.front().u == 0.75);
    const auto cancelled_flip = BuildPlan(2,1,2,nullptr,true,{0,0,1,1,0,true,false});
    check(cancelled_flip.front().u == 0.25);
    check(BuildPlan(0, 1, 1, nullptr, false, {}).empty());

    QImage cropped = Normalize(source, true, QRect(60,0,60,90), 800,600);
    check(qRed(cropped.pixel(0,0)) >= 120);
    QImage black = Normalize(source, true, QRect(0,0,0,0),800,600);
    check(black.pixel(400,300) == qRgb(0,0,0));
    QImage exact = Normalize(normalized, false, {},800,600);
    const QRgb retained = exact.pixel(0,0);
    normalized.setPixel(0,0,qRgb(255,0,0));
    check(exact.pixel(0,0) == retained); // The mailbox retains an immutable captured image.

    auto adjusted = [&](float brightness, int temperature, int tint)
    {
        return PublicationImage(exact, [&](QRgb pixel)
        {
            const RGBColor value = ColorUtils::apply_adjustments(ToRGBColor(qRed(pixel),qGreen(pixel),qBlue(pixel)),brightness,temperature,tint);
            return qRgb(RGBGetRValue(value),RGBGetGValue(value),RGBGetBValue(value));
        });
    };
    const QImage off = adjusted(0,0,0), half = adjusted(0.5f,0,0), warm = adjusted(1,80,-40);
    check(off.pixel(500,400) == qRgb(0,0,0));
    check(qRed(half.pixel(500,400)) == qRed(exact.pixel(500,400))/2);
    check(exact.pixel(0,0) == retained); // Image-output adjustment never changes LED input.
    check(qRed(warm.pixel(500,400)) >= qRed(exact.pixel(500,400)));
    check(qAlpha(half.pixel(500,400)) == 255);

#ifdef EFFECTS_HAS_NATIVE_IMAGE_ROUTING
    struct Sink : room_image::RGBControllerImageInterface
    {
        bool supported = true;
        room_image::SubmitResult result = room_image::SubmitResult::Accepted;
        std::shared_ptr<const room_image::Frame> received;
        room_image::Mapping mapping;
        int submissions = 0;
        bool GetImageOutput(unsigned zone, room_image::Output& output) const override
        { output = {zone,320,200,30}; return supported; }
        room_image::SubmitResult SubmitImage(unsigned, std::shared_ptr<const room_image::Frame> frame,
                                             const room_image::Mapping& value, unsigned) override
        { ++submissions; received = std::move(frame); mapping = value; return result; }
    };
    QImage uniform(800,600,QImage::Format_RGB32); uniform.fill(qRgb(200,100,40));
    Router router;
    Sink first,second,unsupported,busy,invalid,legacy;
    unsupported.result = room_image::SubmitResult::Unsupported;
    busy.result = room_image::SubmitResult::Busy;
    invalid.result = room_image::SubmitResult::Invalid;
    legacy.supported = false;
    check(!router.Submit(nullptr,0,uniform,1,50,0,0,{},false));
    check(!router.Submit(&legacy,0,uniform,1,50,0,0,{},false));
    check(router.Submit(&first,0,uniform,1,50,0,0,{},false,0.5));
    check(router.Submit(&second,3,uniform,1,50,0,0,{},false));
    check(first.received == second.received && first.received->pixels == second.received->pixels);
    check(first.received->width == 800 && first.received->height == 600);
    const auto sampled = room_image::SampleBGRA(*first.received,first.mapping,0.5,0.5);
    check(qRed(sampled) == 50 && qGreen(sampled) == 25); // Global50% and zone50%, applied once each.
    check(!router.Submit(&unsupported,0,uniform,1,50,0,0,{},false));
    check(router.Submit(&busy,0,uniform,1,50,0,0,{},false));
    check(router.Submit(&invalid,0,uniform,1,50,0,0,{},false));
    check(router.Submit(&first,0,uniform,1,50,0,0,{},false) && first.submissions == 1); // Output cadence bounded.
    const auto retained_frame = first.received;
    router.frame.Update(uniform,2,0,0,0);
    check(router.frame.frame != retained_frame);
    check(retained_frame->pixels->at(2) == 100 && router.frame.frame->pixels->at(2) == 0);
    const auto cached = router.frame.frame;
    router.frame.Update(uniform,2,0,0,0);
    check(router.frame.frame == cached);
    router.SetRunning(false);
    Sink after_stop;
    check(router.Submit(&after_stop,0,uniform,3,100,0,0,{},false) && after_stop.submissions == 0);
    router.SetRunning(true);
    check(router.Submit(&after_stop,0,uniform,3,100,0,0,{},false) && after_stop.submissions == 1);
    Sink reverse_matrix;
    check(router.Submit(&reverse_matrix,0,uniform,4,100,0,0,{},true));
    double mapped_x,mapped_y;
    check(reverse_matrix.mapping.Point(0.25,0.25,mapped_x,mapped_y));
    const unsigned square[] = {0,1,2,3};
    const auto reversed_square = BuildPlan(2,2,4,square,true,{});
    check(mapped_x == reversed_square[0].u && mapped_y == reversed_square[0].v && mapped_y == 0.25);
#endif

#ifdef _WIN32
    const std::string channel = "ambient-test-" + std::to_string(GetCurrentProcessId());
    room_surface::Publisher publisher(channel);
    room_surface::Reader reader(channel);
    check(publisher.IsOpen());
    check(publisher.PublishBGRA(half.constBits(),half.sizeInBytes(),half.width(),half.height(),half.bytesPerLine()));
    room_surface::Frame frame;
    check(reader.ReadLatest(frame) == room_surface::FrameStatus::NewFrame);
    check(frame.width == 800 && frame.height == 600 && frame.bgra.size() == 1920000);
    const std::size_t offset = 400 * frame.stride + 500 * 4;
    check(frame.bgra[offset] == qBlue(half.pixel(500,400)) && frame.bgra[offset+2] == qRed(half.pixel(500,400)) && frame.bgra[offset+3] == 255);
#endif
    std::cout << assertions << " Ambient assertions passed (synthetic images/shared memory; no screen or hardware).\n";
}
