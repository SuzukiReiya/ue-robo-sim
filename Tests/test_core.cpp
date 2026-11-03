#include "../Source/RobotSim/SimCore.h"
#include "../Source/RobotSim/SimControl.h"
#include "../Source/RobotSim/SimAimMath.h"
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
    assert(action.movement.throttle == 1 && action.movement.strafe == 1 && action.movement.turn == 0);
    assert(action.fire);
    bindings.forward = RobotSim::InputSource::MouseUp;
    bindings.backward = RobotSim::InputSource::MouseDown;
    bindings.turnLeft = RobotSim::InputSource::MouseLeft;
    bindings.turnRight = RobotSim::InputSource::MouseRight;
    bindings.fire = RobotSim::InputSource::MouseButtonRight;
    // カスタム割当テストでは旧移動キーをピッチへ二重割当しない。
    bindings.pitchUp = RobotSim::InputSource::None;
    bindings.pitchDown = RobotSim::InputSource::None;
    bindings.aimAssist = RobotSim::InputSource::None;
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
    assert(bindings.valid()); // 射撃の未割当を許可。
    bindings.forward = RobotSim::InputSource::None;
    assert(bindings.valid()); // すべての操作で未割当を許可。
    for(std::size_t i=0; i<RobotSim::SourceCount; ++i) {
        RobotSim::InputSource decoded = RobotSim::InputSource::None;
        assert(RobotSim::parseSource(
            RobotSim::sourceToken(static_cast<RobotSim::InputSource>(i)), decoded));
        assert(decoded == static_cast<RobotSim::InputSource>(i));
    }
    RobotSim::InputSource invalid;
    assert(!RobotSim::parseSource("Escape", invalid)); // 予約キー
    RobotSim::CommandRouter fireRouter;
    RobotSim::OperatorActions fireInput;
    fireInput.fire = true;
    fireRouter.submitActions(fireInput, 6.0, 0.2);
    fireRouter.resolve(6.19, 0.5);
    assert(!fireRouter.fireActive()); // 指令遅延が満了するまで発射しない。
    fireRouter.resolve(6.20, 0.5);
    assert(fireRouter.fireActive());
    fireRouter.selectMode(RobotSim::Mode::Automatic);
    assert(!fireRouter.fireActive()); // 手動の射撃は自動モードに持ち込まない。
    fireRouter.selectMode(RobotSim::Mode::Manual);
    fireRouter.submitActions(fireInput, 7.0, 0.0);
    fireRouter.resolve(7.0, 0.5);
    fireRouter.emergencyStop();
    assert(!fireRouter.fireActive()); // 非常停止は発射も取り消す。
    // 横移動軸は前後進および旋回から独立し、共有制御でも補間される。
    auto mixed = RobotSim::blend({1,0,1}, {0,1,-1}, 0.25);
    assert(mixed.throttle == 0.75 && mixed.turn == 0.25 && mixed.strafe == 0.5);
    RobotSim::Drive sideways;
    for (int i=0;i<120;++i) sideways.step({0,0,1},1.0/120,150,120,0.15);
    assert(sideways.lateralSpeed>149 && sideways.speed==0 && sideways.yawRate==0);
    RobotSim::Tuning tweak;
    assert(tweak.valid());
    assert(tweak.moveSensitivity == 1.0 && tweak.turnSensitivity == 1.0);
    assert(tweak.maxMoveSpeedCmPerSec() == 150.0);
    assert(tweak.maxTurnRateDegPerSec() == 120.0);
    RobotSim::RawInput horizontal;
    horizontal.mouseX=2;
    RobotSim::InputBindings defaults;
    // 操作入力の変換は同一。感度は機体の移動速度・旋回速度へ作用する。
    assert(std::abs(RobotSim::mapInput(defaults, horizontal, tweak).movement.turn
                    - (2.0 / 12.0)) < 1e-8);
    tweak.turnSensitivity = 20.0;
    tweak.moveSensitivity = 20.0;
    assert(tweak.valid());
    assert(tweak.maxMoveSpeedCmPerSec() == 3000.0);
    assert(tweak.maxTurnRateDegPerSec() == 2400.0);
    RobotSim::Drive fast;
    for (int i=0; i<120; ++i)
        fast.step({1,1,0}, 1.0/120.0,
                  tweak.maxMoveSpeedCmPerSec(), tweak.maxTurnRateDegPerSec(),0.15);
    assert(fast.speed > 2990 && fast.yawRate > 2390);
    tweak.moveSensitivity = 0.1;
    tweak.turnSensitivity = 0.1;
    assert(tweak.maxMoveSpeedCmPerSec() == 15.0);
    assert(tweak.maxTurnRateDegPerSec() == 12.0);
    tweak.moveSensitivity = 0.09;
    assert(!tweak.valid());
    tweak.moveSensitivity = 20.1;
    assert(!tweak.valid());
    tweak.moveSensitivity = 1.0;
    tweak.turnSensitivity = 20.1;
    assert(!tweak.valid());
    tweak.turnSensitivity = 1.0;
    tweak.commandDelayMs = 250.0;
    tweak.videoDelayMs = 450.0;
    assert(tweak.valid());
    tweak.mouseButtonStrength=0.4;
    RobotSim::InputBindings clickable;
    clickable.strafeRight=RobotSim::InputSource::MouseButtonRight;
    clickable.turnRight=RobotSim::InputSource::None;
    assert(clickable.valid());
    horizontal.held[RobotSim::index(RobotSim::InputSource::MouseButtonRight)]=true;
    assert(std::abs(RobotSim::mapInput(clickable,horizontal,tweak).movement.strafe-0.4)<1e-8);
    tweak.displayFps=0;
    assert(!tweak.valid());
    // マウス上下とShiftホールドの入力変換・重複割当検証。
    RobotSim::InputBindings pitchBindings;
    assert(pitchBindings.valid());
    RobotSim::RawInput pitchRaw;
    pitchRaw.mouseY = -6.0;
    pitchRaw.held[RobotSim::index(RobotSim::InputSource::Shift)] = true;
    auto mappedPitch = RobotSim::mapInput(pitchBindings, pitchRaw);
    assert(std::abs(mappedPitch.movement.pitch-0.5)<1e-9);
    assert(mappedPitch.aimAssist && !mappedPitch.fire);
    pitchRaw.mouseY = 6.0;
    assert(std::abs(RobotSim::mapInput(pitchBindings, pitchRaw).movement.pitch+0.5)<1e-9);
    pitchBindings.aimAssist = RobotSim::InputSource::W;
    assert(!pitchBindings.valid());
    RobotSim::CommandRouter assistRouter;
    RobotSim::OperatorActions assisted;
    assisted.aimAssist=true;
    assisted.movement.pitch=0.6;
    assistRouter.submitActions(assisted, 0.0, 0.15);
    assistRouter.resolve(0.14, 0.5);
    assert(!assistRouter.assistActive());
    assistRouter.resolve(0.15, 0.5);
    assert(assistRouter.assistActive());
    assert(std::abs(assistRouter.applied().pitch-0.6)<1e-9);
    assistRouter.emergencyStop();
    assert(!assistRouter.assistActive());
    // カメラ上部・砲口下部による視差。画面中央であっても射線は上向き。
    namespace Aim = RobotSim::Aim;
    // 既存の中心距離・射線視差・遮蔽条件を満たす正面パネル。
    const Aim::Vec3 frontNormal{-1,0,0};
    const std::vector<Aim::Candidate> panels = {
        {0,{400,0,10},true,frontNormal},
        {1,{400,200,10},true,frontNormal}};
    auto selected = Aim::select({0,0,18},0,0,{17,0,8},panels);
    assert(selected.found && selected.id==0);
    assert(selected.pitchDeg>0 && selected.pitchDeg<2);
    assert(std::abs(selected.yawDeg)<1e-10);
    auto hidden = Aim::select({0,0,18},0,0,{17,0,8},
                              {{0,{400,0,10},false,frontNormal}});
    assert(!hidden.found);
    auto outside = Aim::select({0,0,18},0,0,{17,0,8},
                               {{0,{620,0,10},true,frontNormal}});
    assert(!outside.found);
    auto behind = Aim::select({0,0,18},0,0,{17,0,8},
                              {{0,{-200,0,10},true,frontNormal}});
    assert(!behind.found);
    auto farOffAxis = Aim::select({0,0,18},0,0,{17,0,8},
                                   {{0,{300,250,10},true,frontNormal}});
    assert(!farOffAxis.found);

    // パネル前面法線と自機方向の角度は、画面中心・距離と独立した必須条件。
    // 正面を0度、横向きを90度、背面を180度として角度の境界も検証する。
    const Aim::Vec3 viewer{0,0,10}, center{400,0,10};
    assert(Aim::facesViewer(center, {-1,0,0}, viewer, 45.0));
    assert(!Aim::facesViewer(center, {1,0,0}, viewer, 45.0));
    assert(!Aim::facesViewer(center, {0,1,0}, viewer, 45.0));
    assert(!Aim::facesViewer(center, {0,0,0}, viewer, 90.0));
    assert(!Aim::facesViewer(center, {-1,0,0}, viewer, -1.0));
    assert(!Aim::facesViewer(center, {-1,0,0}, viewer, 91.0));
    assert(Aim::facesViewer(center,
                            {-std::cos(Aim::radians(30)),std::sin(Aim::radians(30)),0},
                            viewer, 45.0));
    assert(!Aim::facesViewer(center,
                             {-std::cos(Aim::radians(60)),std::sin(Aim::radians(60)),0},
                             viewer, 45.0));
    // 自転する2機目が背面・斜めを見せたときは、カメラ中央でも選ばれない。
    auto turnedAway = Aim::select({0,0,10},0,0,{17,0,8},
                                  {{0,{400,0,10},true,{1,0,0}}});
    assert(!turnedAway.found);
    auto sideOn = Aim::select({0,0,10},0,0,{17,0,8},
                              {{0,{400,0,10},true,{0,1,0}}});
    assert(!sideOn.found);
    // 背面のほうが中心に近くても、正面を見せている別パネルを選ぶ。
    auto preferFacing = Aim::select({0,0,10},0,0,{17,0,8},{
        {0,{350,0,10},true,{1,0,0}},
        {1,{400,15,10},true,{-1,0,0}}});
    assert(preferFacing.found && preferFacing.id==1);
    auto strictAngle = Aim::select({0,0,10},0,0,{17,0,8},{
        {1,{400,0,10},true,
            {-std::cos(Aim::radians(30)), std::sin(Aim::radians(30)),0}}},
        90.0,640.0/360.0,600.0,0.35,20.0);
    assert(!strictAngle.found);
    auto relaxedAngle = Aim::select({0,0,10},0,0,{17,0,8},{
        {1,{400,0,10},true,
            {-std::cos(Aim::radians(30)), std::sin(Aim::radians(30)),0}}},
        90.0,640.0/360.0,600.0,0.35,40.0);
    assert(relaxedAngle.found);
    RobotSim::Tuning facingTuning;
    assert(facingTuning.valid() && facingTuning.aimFacingAngleDeg == 45.0);
    facingTuning.aimFacingAngleDeg = -1.0;
    assert(!facingTuning.valid());
    facingTuning.aimFacingAngleDeg = 91.0;
    assert(!facingTuning.valid());
    assert(Aim::turnCorrection(0,10,0,120)>0);
    assert(Aim::pitchCorrection(0,5)>0);
    assert(Aim::turnCorrection(0,0,0,120)==0);
    RobotSim::CommandRouter remappedRouter;
    remappedRouter.submitCommand({-0.6,0.4}, 5.0, 0.1);
    assert(remappedRouter.resolve(5.1,0.5).throttle < -0.59);
    // 旋回慣性：同じ操舵入力でも長い時定数は立ち上がりが緩やかで、
    // 手を離した後にも角速度が残る。並進速度の設定には影響しない。
    RobotSim::Drive sharp, inertial;
    constexpr double dt = 1.0 / 120.0;
    for (int i = 0; i < 60; ++i) {
        sharp.step({0,1,0}, dt, 150, 120, 0.15, 0.02);
        inertial.step({0,1,0}, dt, 150, 120, 0.15, 0.45);
    }
    assert(sharp.yawRate > 110);
    assert(inertial.yawRate > 50 && inertial.yawRate < 100);
    for (int i = 0; i < 60; ++i) {
        sharp.step({0,0,0}, dt, 150, 120, 0.15, 0.02);
        inertial.step({0,0,0}, dt, 150, 120, 0.15, 0.45);
    }
    assert(sharp.yawRate < 0.01);
    assert(inertial.yawRate > 15);
    assert(inertial.speed == 0 && inertial.lateralSpeed == 0);
    // 4枚すべて中心高さ150mm以下、下面は地表面より上。
    for (int profile = 0; profile < RobotSim::PanelPatternCount; ++profile) {
        assert(RobotSim::panelPatternIsValid(profile));
        for (int side = 0; side < 4; ++side) {
            const double center = RobotSim::panelHeightCm(profile, side);
            assert(center >= 6 && center <= 15);
        }
    }
    assert(RobotSim::panelHeightCm(4, 0) == 12);
    assert(RobotSim::panelHeightCm(4, 1) == 6);
    assert(!RobotSim::panelPatternIsValid(-1));
    assert(!RobotSim::panelPatternIsValid(5));
    RobotSim::Tuning panelTuning;
    assert(panelTuning.valid());
    panelTuning.turnInertiaMs = 1501;
    assert(!panelTuning.valid());
    panelTuning.turnInertiaMs = 450;
    panelTuning.panelPattern = 5;
    assert(!panelTuning.valid());
    std::cout<<"遅延境界・順序逆転・共有制御・前後進・感度上限・旋回慣性・被弾パネル高さ・ピッチ操作・照準補正・機体応答: 成功\n";
}
