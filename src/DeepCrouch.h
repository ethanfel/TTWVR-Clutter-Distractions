// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cmath>
#include <cstdint>

namespace crouch {
enum class Stick : unsigned { invalid, released, down, held };

// Measure the uninterrupted hold where controller samples arrive. A release
// between two game frames still starts a fresh three-second hold.
struct Hold {
    bool down{};
    std::uint32_t start{}, last{};
    Stick sample(bool tracked,float x,float y,std::uint32_t now) {
        if (now-last>250) down=false;
        last=now;
        if (!tracked || !std::isfinite(x) || !std::isfinite(y)) {
            down=false;
            return Stick::invalid;
        }
        if (y>(down ? -0.50f : -0.70f) || std::abs(x)>-y) {
            down=false;
            return Stick::released;
        }
        if (!down) { down=true; start=now; }
        return now-start>=3000 ? Stick::held : Stick::down;
    }
};

enum class Level { off, standing, regular, deep };
struct Control {
    enum class Press { none, pass, regular, deep, suppress } press{};
    bool heldLong{}, neutralSeen{}, pulsing{};
    std::uint32_t pulseStart{};
    // The normal crouch binding toggles sneak on the down edge. While already
    // crouched, defer that edge until release so a long hold can go deeper.
    float filter(Level level,Stick stick,float y,std::uint32_t now,bool& rise) {
        rise=false;
        if (level==Level::off) { *this={}; return y; }
        if (stick==Stick::invalid) {
            press=Press::suppress; pulsing=false;
            return y;
        }
        if (level!=Level::regular) pulsing=false;
        if (stick==Stick::released) {
            if (press==Press::regular && !heldLong && level==Level::regular) {
                pulsing=true; pulseStart=now;
            }
            press=Press::none; heldLong=false; neutralSeen=true;
            // Keep the stand request alive until the host changes stance. A second
            // edge covers a short input cooldown. Cancel on fresh physical input.
            if (pulsing && std::abs(y)<.5f) {
                const auto elapsed=now-pulseStart;
                if (elapsed<140 || (elapsed>=300 && elapsed<470)) return -1.f;
                if (elapsed<300) return 0.f;
            }
            pulsing=false;
            return y;
        }
        pulsing=false;
        if (press==Press::none) {
            press=!neutralSeen || level==Level::standing ? Press::pass :
                (level==Level::regular ? Press::regular : Press::deep);
            rise=press==Press::deep;
        }
        if (stick==Stick::held) heldLong=true;
        return press==Press::pass ? y : 0.f;
    }
};

struct Stance {
    bool armed{}, active{};
    float original{}, applied{};
    // Returns whether the host target should change. Called on the game thread.
    bool update(Stick stick,bool manual,float target,float requested,float& next) {
        if (active) {
            if (manual && target==applied) return false;
            active=false;
            armed=false;
            if (target==applied) { next=original; return true; }
        }
        if (stick==Stick::invalid) { armed=false; return false; }
        if (stick==Stick::released) { armed=true; return false; }
        if (stick!=Stick::held || !armed || !manual) return false;
        armed=false;
        if (!std::isfinite(target) || target<=requested || target>3.f) return false;
        original=target; applied=requested; next=requested; active=true;
        return true;
    }
};

void bind(unsigned char* host);
bool available();
float filter(Stick stick,float y,std::uint32_t now,Control& control);
void update(Stick stick,float height,double now,void (*log)(const char*));
void suspend();
}
