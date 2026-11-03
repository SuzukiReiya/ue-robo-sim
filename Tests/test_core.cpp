#include "../Source/RobotSim/SimCore.h"
#include "../Source/RobotSim/SimControl.h"
#include "../Source/RobotSim/SimInputBindings.h"
#include <cassert>
#include <iostream>
int main() {
    RobotSim::DelayQueue<int> q;
    int x=0;
    q.push(0,0.2,1); assert(!q.poll(0.199,x)); assert(q.poll(0.2,x)&&x==1);
    q.push(1,1,2); q.push(1.1,0,3);
    assert(q.poll(1.1,x)&&x==3); assert(!q.poll(2,x)&&x==3);
    q.clear(); q.push(0,0,4); assert(q.poll(0,x)&&x==4);
    q.push(3,0,5); q.push(3,0,6); assert(q.poll(3,x)&&x==6);
    auto c=RobotSim::blend({1,-1},{-1,1},0.25);
    assert(c.throttle==0.5&&c.turn==-0.5);
    RobotSim::Drive a,b;
    for(int i=0;i<120;++i) a.step({1,1},1.0/120,150,120,0.15);
    for(int i=0;i<60;++i) b.step({1,1},1.0/60,150,120,0.15);
    assert(std::abs(a.speed-b.speed)<1e-10);
    assert(a.speed>149&&a.speed<150);
    for(int i=0;i<240;++i) a.step({0,0},1.0/120,150,120,0.15);
    assert(std::abs(a.speed)<0.001);
    // 前進・後退の両方向に速度が立ち上がり、符号を反転できること。
    RobotSim::Drive reverse;
    for(int i=0;i<120;++i) reverse.step({-1,0},1.0/120,150,120,0.15);
    assert(reverse.speed < -149 && reverse.speed > -150);
    for(int i=0;i<120;++i) reverse.step({1,0},1.0/120,150,120,0.15);
    assert(reverse.speed > 149 && reverse.speed < 150);
    // キーボード状態と制御指令の対応はUE非依存で検証する。
    assert(RobotSim::toCommand({true,false,true,false}).throttle==1);
    assert(RobotSim::toCommand({true,true,true,true}).turn==0);
    RobotSim::CommandRouter router;
    router.submitButtons({true,false,false,true}, 0.0, 0.10);
    assert(router.resolve(0.09, 0.5).throttle==0);
    assert(router.resolve(0.10, 0.5).throttle==1);
    assert(router.applied().turn==1);
    assert(router.resolve(0.40, 0.5).throttle==0); // 入力途絶時は停止
    router.selectMode(RobotSim::Mode::Shared);
    router.submitButtons({true,false,false,false}, 1.0, 0.0);
    assert(router.resolve(1.0, 0.5).throttle==1); // 外部入力不在なら手動
    assert(router.submitExternal(1.01,10,{-1,1}));
    assert(!router.submitExternal(1.02,9,{1,1})); // 古いseqは拒否
    assert(router.resolve(1.02,0.5).throttle==0);
    assert(router.resolve(1.30,0.5).throttle==0); // 入力の期限切れ
    router.selectMode(RobotSim::Mode::Automatic);
    router.submitButtons({true,false,false,false}, 2.0, 0.0);
    assert(router.resolve(2.0,0.5).throttle==0); // 自動では手動を採用しない
    assert(router.submitExternal(2.0,11,{0.3,-0.5}));
    assert(router.resolve(2.0,0.5).turn==-0.5);
    router.emergencyStop();
    assert(router.resolve(2.01,0.5).throttle==0 && router.applied().turn==0);
    router.selectMode(RobotSim::Mode::Manual);
    router.submitButtons({true,false,false,false}, 3.0, 0.5);
    router.selectMode(RobotSim::Mode::Automatic);
    router.selectMode(RobotSim::Mode::Manual);
    assert(router.resolve(3.6,0.5).throttle==0); // モード切替前のキューは消去
    // WASD + マウス移動・左右クリックの割り当てを機体や描画に依存せず検証。
    RobotSim::InputBindings bindings;
    assert(bindings.valid());
    RobotSim::RawInput raw;
    raw.held[RobotSim::index(RobotSim::InputSource::W)] = true;
    raw.held[RobotSim::index(RobotSim::InputSource::D)] = true;
    raw.held[RobotSim::index(RobotSim::InputSource::MouseButtonLeft)] = true;
    auto action = RobotSim::mapInput(bindings, raw);
    assert(action.movement.throttle == 1 && action.movement.turn == 1);
    assert(action.fire);
    bindings.forward = RobotSim::InputSource::MouseUp;
    bindings.backward = RobotSim::InputSource::MouseDown;
    bindings.turnLeft = RobotSim::InputSource::MouseLeft;
    bindings.turnRight = RobotSim::InputSource::MouseRight;
    bindings.fire = RobotSim::InputSource::MouseButtonRight;
    assert(bindings.valid());
    raw = {};
    raw.mouseX = -6.0;
    raw.mouseY = -12.0;
    raw.held[RobotSim::index(RobotSim::InputSource::MouseButtonRight)] = true;
    action = RobotSim::mapInput(bindings, raw);
    assert(action.movement.throttle == 1);
    assert(action.movement.turn == -0.5);
    assert(action.fire);
    raw.mouseX = 0;
    raw.mouseY = 0;
    assert(RobotSim::mapInput(bindings, raw).movement.throttle == 0);
    bindings.fire = RobotSim::InputSource::MouseUp;
    assert(!bindings.valid()); // 同じ割り当ては複数の操作に使用させない。
    bindings.fire = RobotSim::InputSource::None;
    assert(bindings.valid()); // 射撃のみ未割当を許可。
    bindings.forward = RobotSim::InputSource::None;
    assert(!bindings.valid());
    for(std::size_t i=0; i<RobotSim::SourceCount; ++i) {
        RobotSim::InputSource decoded = RobotSim::InputSource::None;
        assert(RobotSim::parseSource(
            RobotSim::sourceToken(static_cast<RobotSim::InputSource>(i)), decoded));
        assert(decoded == static_cast<RobotSim::InputSource>(i));
    }
    RobotSim::InputSource invalid;
    assert(!RobotSim::parseSource("Escape", invalid)); // 予約キー
    RobotSim::CommandRouter remappedRouter;
    remappedRouter.submitCommand({-0.6,0.4}, 5.0, 0.1);
    assert(remappedRouter.resolve(5.1,0.5).throttle < -0.59);
    std::cout<<"遅延境界・順序逆転・初期化・共有制御・前後進・入力割当・マウス操作・モード裁定・機体応答: 成功\n";
}
