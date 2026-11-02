#include "SimKeyboardInput.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

RobotSim::OperatorButtons FSimKeyboardInput::Read(const APlayerController* PC)
{
    if (!PC) return {};
    return {PC->IsInputKeyDown(EKeys::W), PC->IsInputKeyDown(EKeys::S),
            PC->IsInputKeyDown(EKeys::A), PC->IsInputKeyDown(EKeys::D)};
}
bool FSimKeyboardInput::EmergencyStop(const APlayerController* PC)
{
    return PC && PC->IsInputKeyDown(EKeys::SpaceBar);
}
