#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace RobotSim {
namespace Aim {
constexpr double Pi = 3.14159265358979323846;
struct Vec3 {
    double x = 0, y = 0, z = 0;
};
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline double dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline double length(Vec3 a) { return std::sqrt(dot(a,a)); }
inline double radians(double deg) { return deg * Pi / 180.0; }
inline double degrees(double rad) { return rad * 180.0 / Pi; }
inline double deltaAngle(double a, double b) {
    return std::remainder(b-a, 360.0);
}
struct Candidate {
    int id = -1;
    Vec3 center;
    bool unobstructed = false;
    Vec3 outwardNormal; // パネルの発光面が向く方向（ワールド座標）
};
// パネル法線と「パネル→自機カメラ」の方向のなす角度を判定。
// パネルの裏面や横向きは、画面中央・近距離にあっても候補から除外。
inline bool facesViewer(Vec3 panelCenter, Vec3 outwardNormal, Vec3 viewer,
                        double maxFacingAngleDeg) {
    if (!std::isfinite(maxFacingAngleDeg) ||
        maxFacingAngleDeg < 0.0 || maxFacingAngleDeg > 90.0) return false;
    const Vec3 towardViewer = viewer - panelCenter;
    const double normalLength = length(outwardNormal);
    const double viewerDistance = length(towardViewer);
    if (!(normalLength > 1e-8 && viewerDistance > 1e-8)) return false;
    const double cosine = dot(outwardNormal,towardViewer) /
        (normalLength * viewerDistance);
    return cosine + 1e-10 >= std::cos(radians(maxFacingAngleDeg));
}
struct Selection {
    int id = -1;
    double score = 1e10;
    double yawDeg = 0;
    double pitchDeg = 0;
    bool found = false;
};
// 撮像空間の視野角とカメラ位置・角度に基づくデモ用の候補選択。
// 候補を撮像画像から抽出する認識器は将来の入替箇所。
inline Selection select(Vec3 camera, double cameraYawDeg, double cameraPitchDeg,
                        Vec3 muzzle, const std::vector<Candidate>& candidates,
                        double horizontalFovDeg = 90.0, double aspect = 640.0/360.0,
                        double maxDistanceCm = 600.0, double maxCenterRadius = 0.35,
                        double maxFacingAngleDeg = 45.0) {
    Selection best;
    if (!(aspect > 0 && horizontalFovDeg > 0 && horizontalFovDeg < 180 &&
          maxDistanceCm > 0 && maxCenterRadius > 0 &&
          std::isfinite(maxFacingAngleDeg) && maxFacingAngleDeg >= 0 &&
          maxFacingAngleDeg <= 90)) return best;
    const double y = radians(cameraYawDeg), p = radians(cameraPitchDeg);
    const Vec3 forward{std::cos(y)*std::cos(p), std::sin(y)*std::cos(p), std::sin(p)};
    const Vec3 right{-std::sin(y), std::cos(y), 0};
    const Vec3 up{-std::cos(y)*std::sin(p), -std::sin(y)*std::sin(p), std::cos(p)};
    const double tanHalfHorizontal = std::tan(radians(horizontalFovDeg) * 0.5);
    const double tanHalfVertical = tanHalfHorizontal / aspect;
    for (const auto& candidate : candidates) {
        if (!candidate.unobstructed ||
            !facesViewer(candidate.center, candidate.outwardNormal,
                         camera, maxFacingAngleDeg)) continue;
        const Vec3 v = candidate.center - camera;
        const double distance = length(v);
        const double depth = dot(v,forward);
        if (!(distance <= maxDistanceCm && depth > 1.0)) continue;
        const double nx = dot(v,right)/(depth*tanHalfHorizontal);
        const double ny = dot(v,up)/(depth*tanHalfVertical);
        const double radial = std::sqrt(nx*nx+ny*ny);
        if (radial > maxCenterRadius || radial >= best.score) continue;
        const Vec3 fromMuzzle = candidate.center - muzzle;
        best.found = true;
        best.score = radial;
        best.id = candidate.id;
        best.yawDeg = degrees(std::atan2(fromMuzzle.y,fromMuzzle.x));
        best.pitchDeg = degrees(std::atan2(fromMuzzle.z,
                         std::hypot(fromMuzzle.x,fromMuzzle.y)));
    }
    return best;
}
inline double turnCorrection(double yawDeg, double wantedYawDeg,
                             double yawRateDegPerSec, double maxYawDegPerSec) {
    if (maxYawDegPerSec <= 0) return 0;
    const double error = deltaAngle(yawDeg, wantedYawDeg);
    // 旋回慣性に対する速度フィードバックで行き過ぎを抑える。
    return std::clamp(error*0.08 - yawRateDegPerSec/maxYawDegPerSec*0.75,
                      -1.0, 1.0);
}
inline double pitchCorrection(double pitchDeg, double wantedPitchDeg) {
    return std::clamp(deltaAngle(pitchDeg,wantedPitchDeg)*0.13, -1.0, 1.0);
}
}
}
