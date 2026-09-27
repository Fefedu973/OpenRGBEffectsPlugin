// SPDX-License-Identifier: GPL-2.0-or-later
#include <iostream>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>

struct Controller
{
    std::string name = "Full Scale.json", serial = "virtual-serial";
    std::string description = "Virtual canvas", version = "1.0+ (git4)";
    std::string vendor = "OpenRGBVisualMapPlugin", location = "Somewhere over the rainbow";
    std::string GetName() const { return name; }
    std::string GetSerial() const { return serial; }
    std::string GetDescription() const { return description; }
    std::string GetVersion() const { return version; }
    std::string GetVendor() const { return vendor; }
    std::string GetLocation() const { return location; }
};

struct ControllerZone
{
    Controller* controller;
    unsigned int zone_idx = 0;
    bool reverse = false;
    unsigned int self_brightness = 100;
    bool is_segment = false;
    int segment_idx = -1;
    bool matches_json(const nlohmann::json&);
    nlohmann::json to_json();
};

// The runner extracts both bodies verbatim from ControllerZone.cpp.
#include "production_identity.inc"

int run_tests()
{
    unsigned tests = 0;
    auto check = [&](bool condition, const char* message) {
        if(!condition) throw std::runtime_error(message);
        ++tests;
    };
    Controller controller;
    ControllerZone zone{&controller};
    auto descriptor = zone.to_json();
    check(zone.matches_json(descriptor), "exact identity");
    descriptor["version"] = "1.0+ (git1)";
    check(zone.matches_json(descriptor), "updated plugin must retain the saved selection");
    controller.version = "new firmware";
    check(zone.matches_json(descriptor), "updated firmware must retain the saved selection");
    check(zone.to_json()["version"] == "new firmware", "version remains saved metadata");
    descriptor.erase("version");
    check(zone.matches_json(descriptor), "metadata absence does not change identity");
    for(const char* field : {"name", "serial", "description", "vendor", "location"})
    {
        auto changed = descriptor;
        changed[field] = "different";
        check(!zone.matches_json(changed), field);
    }
    auto changed = descriptor;
    changed["zone_idx"] = 1;
    check(!zone.matches_json(changed), "different zone");
    changed = descriptor;
    changed["is_segment"] = true;
    check(!zone.matches_json(changed), "segment versus whole zone");
    changed = descriptor;
    changed["segment_idx"] = 0;
    check(!zone.matches_json(changed), "different segment");
    changed = descriptor;
    changed.erase("location");
    check(!zone.matches_json(changed), "missing location");
    zone.is_segment = true;
    zone.segment_idx = 3;
    changed = zone.to_json();
    changed["version"] = "old";
    check(zone.matches_json(changed), "same segment after update");
    changed["segment_idx"] = 2;
    check(!zone.matches_json(changed), "neighboring segment rejected");
    zone.is_segment = false;
    zone.segment_idx = -1;
    changed = zone.to_json();
    changed.erase("is_segment");
    changed.erase("segment_idx");
    check(zone.matches_json(changed), "legacy whole-zone descriptor");

    // Existing bus-location policy is deliberately unchanged.
    controller.location = "HID: new-path";
    changed = zone.to_json();
    changed["location"] = "HID: old-path";
    check(zone.matches_json(changed), "HID path changes remain supported");
    controller.location = "other bus";
    check(!zone.matches_json(changed), "HID descriptor rejects another bus");
    controller.location = "I2C: i2c-9, 0x50";
    changed = zone.to_json();
    changed["location"] = "I2C: i2c-1, 0x50";
    check(zone.matches_json(changed), "I2C bus renumbering remains supported");
    changed["location"] = "I2C: i2c-1, 0x51";
    check(!zone.matches_json(changed), "I2C address mismatch");
    std::cout << "PASS: " << tests << " production identity checks\n";
    return 0;
}

int main()
{
    try { return run_tests(); }
    catch(const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
