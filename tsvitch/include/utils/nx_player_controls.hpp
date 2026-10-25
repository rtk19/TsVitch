#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Ported from rtk19/nx-torrent-player/app/player.cpp (Apache-2.0).
namespace nxplayer {
inline constexpr std::array<double, 7> speeds{.5, .75, 1., 1.25, 1.5, 1.75, 2.};
inline double stepSpeed(double current, int direction) {
    size_t closest = 0;
    for (size_t i = 1; i < speeds.size(); ++i)
        if (std::abs(speeds[i] - current) < std::abs(speeds[closest] - current))
            closest = i;
    return speeds[std::clamp(int(closest) + direction, 0, int(speeds.size()) - 1)];
}
struct Scrub {
    bool active = false, wasPaused = false;
    double target = 0, duration = 0, held = 0;
    void begin(double position, double length, bool paused) {
        active = true;
        target = position;
        duration = length;
        wasPaused = paused;
        held = 0;
    }
    void move(double delta) { target = std::clamp(target + delta, 0., std::max(0., duration)); }
    void stick(double axis, double dt) {
        dt = std::clamp(dt, 0., .25);
        if (std::abs(axis) < .15) {
            held = 0;
            return;
        }
        held += dt;
        const double ramp = 1. + 11. * std::min(held / 4., 1.);
        move(axis * std::abs(axis) * 30. * ramp * dt);
    }
};
} // namespace nxplayer
