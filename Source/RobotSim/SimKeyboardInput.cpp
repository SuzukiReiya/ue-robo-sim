#include "SimKeyboardInput.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

RobotSim::RawInput FSimKeyboardInput::Read(const APlayerController* PC)
{
    RobotSim::RawInput Raw;
    if (!PC) return Raw;
    const FKey Keys[] = {
        EKeys::Invalid, EKeys::W, EKeys::A, EKeys::S, EKeys::D,
        EKeys::Q, EKeys::E, EKeys::R, EKeys::F, EKeys::Z, EKeys::X,
        EKeys::C, EKeys::V, EKeys::I, EKeys::J, EKeys::K, EKeys::L,
        EKeys::LeftShift, EKeys::LeftControl, EKeys::Up, EKeys::Down,
        EKeys::Left, EKeys::Right
    };
    for (int32 I = 1; I < UE_ARRAY_COUNT(Keys); ++I)
        Raw.held[I] = PC->IsInputKeyDown(Keys[I]);
    Raw.held[RobotSim::index(RobotSim::InputSource::MouseButtonLeft)] =
        PC->IsInputKeyDown(EKeys::LeftMouseButton);
    Raw.held[RobotSim::index(RobotSim::InputSource::MouseButtonRight)] =
        PC->IsInputKeyDown(EKeys::RightMouseButton);
    float X = 0, Y = 0;
    PC->GetInputMouseDelta(X, Y);
    Raw.mouseX = X;
    Raw.mouseY = Y;
    return Raw;
}
bool FSimKeyboardInput::EmergencyStop(const APlayerController* PC)
{
    return PC && PC->IsInputKeyDown(EKeys::SpaceBar);
}
