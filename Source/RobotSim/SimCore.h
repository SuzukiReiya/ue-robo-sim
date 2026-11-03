#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>

// UEと独立した遅延キュー。期限が逆転しても古い指令で巻き戻さない。
namespace RobotSim {
struct Command { double throttle=0, turn=0, strafe=0; };
template<class T> class DelayQueue {
    struct Entry { double due; std::uint64_t seq; T value; };
    std::deque<Entry> entries;
    std::uint64_t next=0, delivered=0;
public:
    void push(double now, double delay, T value) {
        entries.push_back({now+std::max(0.0,delay), ++next, value});
        if(entries.size()>4096) entries.pop_front();
    }
    bool poll(double now, T& result) {
        bool found=false;
        for(auto it=entries.begin();it!=entries.end();) {
            if(it->due<=now) {
                if(it->seq>delivered) { result=it->value; delivered=it->seq; found=true; }
                it=entries.erase(it);
            } else ++it;
        }
        return found;
    }
    void clear(){entries.clear();next=delivered=0;}
};
inline Command blend(Command manual, Command external, double weight) {
    weight=std::clamp(weight,0.0,1.0);
    return {std::clamp(manual.throttle*(1-weight)+external.throttle*weight,-1.0,1.0),
            std::clamp(manual.turn*(1-weight)+external.turn*weight,-1.0,1.0),
            std::clamp(manual.strafe*(1-weight)+external.strafe*weight,-1.0,1.0)};
}
struct Drive {
    double speed=0, yawRate=0, lateralSpeed=0;
    void step(Command c, double dt, double maxSpeed, double maxYaw, double tau,
              double turnTau = -1.0) {
        const double a=1-std::exp(-dt/std::max(tau,0.001));
        speed+=(std::clamp(c.throttle,-1.0,1.0)*maxSpeed-speed)*a;
        const double angularTau = turnTau >= 0.0 ? turnTau : tau;
        const double yawAlpha = 1-std::exp(-dt/std::max(angularTau,0.001));
        yawRate+=(std::clamp(c.turn,-1.0,1.0)*maxYaw-yawRate)*yawAlpha;
        lateralSpeed+=(std::clamp(c.strafe,-1.0,1.0)*maxSpeed-lateralSpeed)*a;
    }
};
}
