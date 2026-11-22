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

struct Stance {
    bool armed{}, active{};
    float original{}, applied{};
    // Returns whether the host target should change. Called on the game thread.
    bool update(Stick stick,bool manual,float target,float requested,float& next) {
        if (active) {
            if (manual && stick!=Stick::invalid && target==applied) return false;
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
void update(Stick stick,float height,void (*log)(const char*));
void suspend();
}
