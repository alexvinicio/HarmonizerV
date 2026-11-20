add_executable(HarmonizerV_Tests
    PitchTrackerTests.cpp
    ScaleHarmonizerTests.cpp
    TdPsolaTests.cpp
    SnapshotBankTests.cpp
    HarmonizerEngineTests.cpp
)

target_link_libraries(HarmonizerV_Tests
    PRIVATE
        HarmonizerV_DSP
        Catch2::Catch2WithMain
)

target_compile_features(HarmonizerV_Tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(HarmonizerV_Tests PRIVATE /W4)
else()
    target_compile_options(HarmonizerV_Tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME HarmonizerV_DSP_Tests COMMAND HarmonizerV_Tests)
