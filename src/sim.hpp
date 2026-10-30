// Pure C++ port of GD 2.2 classic physics (no Geode includes, so it can be unit-tested on a PC).
// Formulas come from StarryDawn72/open-source-gd-project (GD 2.2081 reconstructions),
// camila314/gdp (decompiled 2.2) and gd-sim, as collected in alpbayrak031-hash/GD-Remake.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace ia {

constexpr double kGroundOffset = 90.0;  // in-game y of level-space y = 0 (ground top)
constexpr double kDt = 0.25;            // each 240 Hz step advances 0.25 "frames"
constexpr double kDtScaled = 0.225;     // dt * 0.9, used for velocity and dy

struct Rect {
    float x0, y0, x1, y1;
};

struct RectList {
    std::vector<Rect> v;
    float maxW = 0.f;

    void clear() { v.clear(); maxW = 0.f; }
    void add(Rect r) { v.push_back(r); maxW = std::max(maxW, r.x1 - r.x0); }
    void finish() {
        std::sort(v.begin(), v.end(), [](Rect const& a, Rect const& b) { return a.x0 < b.x0; });
    }
    // calls f for every rect overlapping the box (strict); stops and returns true if f returns true
    template <class F>
    bool any(float x0, float y0, float x1, float y1, F&& f) const {
        auto it = std::lower_bound(v.begin(), v.end(), x0 - maxW,
            [](Rect const& r, float val) { return r.x0 < val; });
        for (; it != v.end() && it->x0 < x1; ++it) {
            if (it->x1 > x0 && it->y1 > y0 && it->y0 < y1) {
                if (f(*it)) return true;
            }
        }
        return false;
    }
};

struct World {
    RectList solids;    // blocks
    RectList hazards;   // spikes, saws
    RectList inter;     // orbs, pads, portals, slopes... (not modelled -> result Unknown)
};

enum class Mode { Cube, Ship, Ufo, Wave, Ball, Robot };
enum class Res { Safe, Dead, Unknown };

struct Sim {
    Mode mode = Mode::Cube;
    bool up = false, mini = false, hold = false, fixGravityBug = false;
    double x = 0, y = 0, lastY = 0, yv = 0;
    double xv = 5.193;          // speed * multiplier (per 60 fps frame)
    double yStart = 11.180032;  // cube jump velocity
    double grav = 0.958199;     // cube / robot gravity
    bool onGround = true, canJump = true, jumpUnused = true, isJumping = false, boostInvalid = false;
    double boost = 0;
    double floorY = kGroundOffset, ceilY = 0;
    bool hasCeil = false;
    double hw = 15, hh = 15;
    bool dead = false;
};

inline bool isFlying(Mode m) { return m == Mode::Ship || m == Mode::Ufo || m == Mode::Wave; }

inline bool fallingBugged(Sim const& s) {
    double t = s.grav + s.grav;
    if (s.fixGravityBug) return s.up ? s.yv > -t : s.yv < t;
    return s.up ? s.yv > t : s.yv < t;
}

inline void hitGround(Sim& s) {
    s.yv = 0;
    s.canJump = true;
    s.onGround = true;
    s.isJumping = false;
}

// PlayerObject::updateJump, classic branch
inline void updateJump(Sim& s, double dt) {
    double flip = s.up ? -1.0 : 1.0;
    bool holding = s.hold;
    bool isHoldingJump = holding;
    if (s.mode == Mode::Robot && !s.jumpUnused) isHoldingJump = false;
    bool flying = isFlying(s.mode);
    double gravity = (flying || s.mode == Mode::Ball) ? 0.958199 : s.grav;
    double scaleMod = s.mini ? 0.8 : 1.0;

    if (flying) {
        if (s.mini) scaleMod = 0.85;
        if (s.mode == Mode::Ship) {
            double shipMod;
            if (holding) shipMod = -1.0;
            else {
                shipMod = 0.8;
                if (!fallingBugged(s)) shipMod = 1.2;
            }
            double boostMod = (holding && fallingBugged(s)) ? 0.5 : 0.4;
            s.yv += -((gravity * dt * flip * shipMod * boostMod) / scaleMod);
            if (holding) s.onGround = false;
        } else if (s.mode == Mode::Ufo) {
            if (s.jumpUnused && holding) {
                s.jumpUnused = false;
                double jump = (s.mini ? 8.0 : 7.0) * flip * scaleMod;
                if ((s.up && jump < s.yv) || (!s.up && jump > s.yv)) s.yv = jump;
            }
            double birdMod = fallingBugged(s) ? 0.8 : 1.2;
            s.yv += -((gravity * dt * flip * birdMod * 0.5) / scaleMod);
            if (holding) s.onGround = false;
        } else { // wave: real motion computed in step()
            s.yv = s.xv * flip * (holding ? 1.0 : -1.0);
        }
        if (s.mode != Mode::Wave) {
            const double modeScale = 0.8;
            if (!s.up) {
                s.yv = std::max(s.yv, modeScale * -8.0 / scaleMod);
                s.yv = std::min(s.yv, 8.0 / scaleMod);
            } else {
                s.yv = std::max(s.yv, -8.0 / scaleMod);
                s.yv = std::min(s.yv, modeScale * 8.0 / scaleMod);
            }
        }
        if (fallingBugged(s)) s.isJumping = false;
    } else {
        double gravScale = 1.0;
        if (s.mode == Mode::Ball) gravScale = 0.6;
        else if (s.mode == Mode::Robot) gravScale = 0.9;

        if (isHoldingJump && s.canJump) {
            s.isJumping = true;
            s.onGround = false;
            s.canJump = false;
            s.jumpUnused = false;
            s.boostInvalid = false;
            s.boost = 0;
            double jump = s.yStart;
            if (s.mode == Mode::Robot) jump *= 0.5;
            s.yv = (jump * flip) * scaleMod;
            if (s.mode == Mode::Ball) {
                s.up = !s.up;          // flipGravity
                s.yv *= 0.5;
                s.canJump = false;
                s.yv *= 0.6;
                s.hold = false;
            }
        } else if (s.isJumping) {
            if (s.mode == Mode::Robot && holding && !s.boostInvalid) {
                if (s.boost < 1.5) {
                    s.boost += dt * 0.1;
                    s.yv += gravity * dt * flip * gravScale;
                }
            }
            s.yv += -(gravScale * gravity * dt * flip);
            if (fallingBugged(s)) {
                s.isJumping = false;
                s.onGround = false;
            }
        } else {
            if (fallingBugged(s)) s.canJump = false;
            s.yv += -(gravScale * gravity * dt * flip);
            if (!s.up) s.yv = std::max(s.yv, -15.0);
            else s.yv = std::min(s.yv, 15.0);
            if (fallingBugged(s)) {
                if (s.up ? (s.yv > 4.0) : (s.yv < -4.0)) s.onGround = false;
            }
        }
    }
}

// level floor and mode ceiling (gd-sim)
inline void bounds(Sim& s) {
    double ext = (s.mode == Mode::Wave) ? s.hh * 2.0 : s.hh;
    if (s.y - ext <= s.floorY) {
        if (s.mode == Mode::Wave) {
            s.y = s.floorY + ext;
        } else if (!s.up) {
            if (s.yv <= 0.0) { s.y = s.floorY + ext; hitGround(s); }
        } else if (s.y - ext < s.floorY) {
            if (!s.hasCeil) { s.dead = true; return; }
            s.y = s.floorY + ext;
            if (s.yv < 0.0) s.yv = 0.0;
        }
    }
    if (s.hasCeil && s.y + ext >= s.ceilY) {
        if (s.mode == Mode::Wave) {
            s.y = s.ceilY - ext;
        } else if (s.up) {
            if (s.yv >= 0.0) { s.y = s.ceilY - ext; hitGround(s); }
        } else if (s.y + ext > s.ceilY) {
            s.y = s.ceilY - ext;
            if (s.yv > 0.0) s.yv = 0.0;
        }
    }
}

inline bool touch(double ax0, double ay0, double ax1, double ay1, Rect const& o) {
    return ax1 > o.x0 && ax0 < o.x1 && ay1 > o.y0 && ay0 < o.y1;
}

// PlayerObject::collidedWithObjectInternal for static solid blocks
inline void solidCollide(Sim& s, Rect const& o) {
    double pminx = s.x - s.hw, pmaxx = s.x + s.hw, pminy = s.y - s.hh, pmaxy = s.y + s.hh;
    if (!touch(pminx, pminy, pmaxx, pmaxy, o)) return;

    bool fly = isFlying(s.mode);
    double thr = fly ? 6.0 : 10.0;
    double dy = s.y - s.lastY;
    double yv = s.yv;
    double g = s.up ? -1.0 : 1.0;
    bool lethal = dy * g > 0.0;

    if (s.mode == Mode::Wave) {
        lethal = true;
    } else {
        bool bumpsHead = fly || s.mode == Mode::Ball;
        if (s.up && !bumpsHead) {
            lethal = true;
        } else {
            double low = pminy + thr;
            if (low < o.y1 && low - dy < o.y1) {
                lethal = true;
            } else if (yv > 0.0) {
                lethal = false;
            } else {
                s.y = o.y1 + s.hh;
                if (!s.up) hitGround(s);
                else if (s.yv < 0.0) s.yv = 0.0;
                return;
            }
        }
        if (lethal && (s.up || bumpsHead)) {
            double high = pmaxy - thr;
            if (!(o.y0 < high && o.y0 < high - dy)) {
                if (yv >= 0.0) {
                    s.y = o.y0 - s.hh;
                    if (s.up) hitGround(s);
                    else if (s.yv > 0.0) s.yv = 0.0;
                    return;
                }
            }
        }
    }
    if (!lethal) return;
    const double k = 0.3;
    if (touch(s.x - s.hw * k, s.y - s.hh * k, s.x + s.hw * k, s.y + s.hh * k, o)) s.dead = true;
}

inline void step(Sim& s, World const& w, double hazScale, Res& res) {
    s.lastY = s.y;
    s.yv = std::clamp(s.yv, -1000.0, 1000.0);
    updateJump(s, kDtScaled);

    double dx = s.xv * kDt;
    double dy = s.yv * kDtScaled;
    if (s.mode == Mode::Wave) {
        double flip = s.up ? -1.0 : 1.0;
        dy = std::abs(dx) * flip * (s.hold ? 1.0 : -1.0);
        if (s.mini) dy += dy;
    }
    s.x += dx;
    s.y += dy;

    bounds(s);
    if (s.dead) { res = Res::Dead; return; }

    const float m = 4.f;
    float bx0 = float(s.x - s.hw) - m, bx1 = float(s.x + s.hw) + m;
    float by0 = float(s.y - s.hh) - m, by1 = float(s.y + s.hh) + m;
    if (w.inter.any(bx0, by0, bx1, by1, [&](Rect const& o) {
            return touch(s.x - s.hw, s.y - s.hh, s.x + s.hw, s.y + s.hh, o);
        })) {
        res = Res::Unknown;
        return;
    }

    w.solids.any(bx0, by0, bx1, by1, [&](Rect const& o) {
        solidCollide(s, o);
        return s.dead;
    });
    if (s.dead) { res = Res::Dead; return; }

    double hx = s.hw * hazScale, hy = s.hh * hazScale;
    if (w.hazards.any(float(s.x - hx), float(s.y - hy), float(s.x + hx), float(s.y + hy),
                      [](Rect const&) { return true; })) {
        s.dead = true;
        res = Res::Dead;
    }
}

inline Res run(Sim s, World const& w, int steps, double hazScale) {
    Res res = Res::Safe;
    for (int i = 0; i < steps; ++i) {
        step(s, w, hazScale, res);
        if (res != Res::Safe) return res;
    }
    return Res::Safe;
}

} // namespace ia
