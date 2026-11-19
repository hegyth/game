#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FVelrathModuleSmokeTest,
    "VELRATH.Smoke.ModuleLoads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVelrathModuleSmokeTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("The VELRATH runtime module is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("VELRATH")));
    return true;
}

#endif

