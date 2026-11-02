#include "../Source/RobotSim/SimCore.h"
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
    std::cout<<"遅延境界・順序逆転・初期化・共有制御・機体応答: 成功\n";
}
