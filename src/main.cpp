#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <cmath>

#include "sim.hpp"

using namespace geode::prelude;

namespace {

struct State {
    ia::World world;
    bool want = false;    // what the user is really holding
    bool actual = false;  // what we've fed to the game
};
State g;

bool optEnabled()  { return Mod::get()->getSettingValue<bool>("enabled"); }
bool optBlock()    { return Mod::get()->getSettingValue<bool>("allow-block"); }
bool optAutoTap()  { return Mod::get()->getSettingValue<bool>("allow-auto-tap"); }
double optScale()  { return Mod::get()->getSettingValue<double>("hitbox-scale"); }
int optLookMs()    { return static_cast<int>(Mod::get()->getSettingValue<int64_t>("lookahead-ms")); }

bool optMode(ia::Mode m) {
    char const* key = "assist-cube";
    switch (m) {
        case ia::Mode::Cube:  key = "assist-cube"; break;
        case ia::Mode::Ship:  key = "assist-ship"; break;
        case ia::Mode::Ufo:   key = "assist-ufo"; break;
        case ia::Mode::Wave:  key = "assist-wave"; break;
        case ia::Mode::Ball:  key = "assist-ball"; break;
        case ia::Mode::Robot: key = "assist-robot"; break;
    }
    return Mod::get()->getSettingValue<bool>(key);
}

// objects the simulator does not model: touching one of these makes the result "unknown"
bool isInteractive(GameObjectType t) {
    switch (t) {
        case GameObjectType::Solid:
        case GameObjectType::Hazard:
        case GameObjectType::AnimatedHazard:
        case GameObjectType::Decoration:
        case GameObjectType::SecretCoin:
        case GameObjectType::Collectible:
        case GameObjectType::UserCoin:
        case GameObjectType::Special:
        case GameObjectType::EnterEffectObject:
        case GameObjectType::CollisionObject:
            return false;
        default:
            return true;
    }
}

void buildCache(GJBaseGameLayer* gl) {
    g.world.solids.clear();
    g.world.hazards.clear();
    g.world.inter.clear();
    for (auto obj : CCArrayExt<GameObject*>(gl->m_objects)) {
        auto t = obj->m_objectType;
        auto const& r = obj->getObjectRect();
        ia::Rect rr{r.origin.x, r.origin.y, r.origin.x + r.size.width, r.origin.y + r.size.height};
        if (t == GameObjectType::Solid) g.world.solids.add(rr);
        else if (t == GameObjectType::Hazard || t == GameObjectType::AnimatedHazard) g.world.hazards.add(rr);
        else if (isInteractive(t)) g.world.inter.add(rr);
    }
    g.world.solids.finish();
    g.world.hazards.finish();
    g.world.inter.finish();
}

bool initSim(ia::Sim& s, PlayerObject* p, GJBaseGameLayer* gl, bool hold, bool actualHeld) {
    if (gl->m_gameState.m_isDualMode || gl->m_isPlatformer) return false;
    if (p->m_isSideways || p->m_isGoingLeft || p->m_isSpider || p->m_isSwing) return false;

    if (p->m_isDart) s.mode = ia::Mode::Wave;
    else if (p->m_isShip) s.mode = ia::Mode::Ship;
    else if (p->m_isBird) s.mode = ia::Mode::Ufo;
    else if (p->m_isBall) s.mode = ia::Mode::Ball;
    else if (p->m_isRobot) s.mode = ia::Mode::Robot;
    else s.mode = ia::Mode::Cube;
    if (!optMode(s.mode)) return false;

    s.up = p->m_isUpsideDown;
    s.mini = p->m_vehicleSize < 0.99f;
    s.hold = hold;
    s.fixGravityBug = p->m_fixGravityBug;
    s.x = p->getPositionX();
    s.y = p->getPositionY();
    s.lastY = s.y;
    s.yv = p->m_yVelocity;
    s.xv = p->m_playerSpeed * p->m_speedMultiplier;
    s.yStart = p->m_yStart;
    s.grav = p->m_gravity;
    s.onGround = p->m_isOnGround;
    s.canJump = s.onGround;
    s.jumpUnused = !(actualHeld && hold);   // continuing a hold = this press is already used
    s.isJumping = !s.onGround && (s.up ? s.yv < 0.0 : s.yv > 0.0);
    s.boost = 0;
    s.boostInvalid = !(actualHeld && hold);

    double base = (s.mode == ia::Mode::Wave ? 5.0 : 15.0) * (s.mini ? 0.6 : 1.0);
    s.hw = s.hh = base;

    // floor and ceiling of the current "box" (gd-sim)
    double bnd = 0.0;
    if (s.mode == ia::Mode::Ship || s.mode == ia::Mode::Ufo || s.mode == ia::Mode::Wave) bnd = 300.0;
    else if (s.mode == ia::Mode::Ball) bnd = 240.0;
    if (bnd > 0.0) {
        double lvlY = s.y - ia::kGroundOffset;
        double fl = std::max(0.0, 30.0 * std::ceil((lvlY - (bnd / 2.0 + 30.0)) / 30.0));
        s.floorY = ia::kGroundOffset + fl;
        s.ceilY = s.floorY + bnd;
        s.hasCeil = true;
    } else {
        s.floorY = ia::kGroundOffset;
        s.hasCeil = false;
    }
    return true;
}

ia::Res simulate(PlayerObject* p, GJBaseGameLayer* gl, bool hold) {
    ia::Sim s;
    if (!initSim(s, p, gl, hold, g.actual)) return ia::Res::Unknown;
    int steps = std::max(2, optLookMs() * 240 / 1000);
    return ia::run(s, g.world, steps, optScale());
}

bool decide(PlayerObject* p, GJBaseGameLayer* gl, bool want) {
    if (ia::Res::Dead != simulate(p, gl, want)) return want;  // fine, or can't tell: don't touch
    if (ia::Res::Safe == simulate(p, gl, !want)) {
        if (want ? optBlock() : optAutoTap()) return !want;
    }
    return want;
}

bool isActiveLayer(GJBaseGameLayer* gl) {
    return optEnabled() && static_cast<GJBaseGameLayer*>(PlayLayer::get()) == gl;
}

} // namespace

class $modify(AssistGL, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (button == 1 && isPlayer1) {
            g.want = down;
            if (isActiveLayer(this) && !m_isPlatformer) {
                return; // swallow; processCommands applies the final input
            }
            g.actual = down;
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (isActiveLayer(this) && !m_isPlatformer && m_player1 && !m_player1->m_isDead) {
            bool d = decide(m_player1, this, g.want);
            if (d != g.actual) {
                GJBaseGameLayer::handleButton(d, 1, true);
                g.actual = d;
            }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

class $modify(AssistPL, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        g.want = false;
        g.actual = false;
        buildCache(this);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        g.actual = false;
    }
};
