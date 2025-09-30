cmake_minimum_required(VERSION 3.12)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(MetronomeProject C)
add_library(metronome
    source/metronome.c
    3rd-party/cjson/cJSON.c
)
target_include_directories(metronome PRIVATE
    3rd-party/miniaudio
    3rd-party/cjson
)
if(UNIX)
    target_link_libraries(metronome PRIVATE
        m
        pthread
    )
endif()

project(MetronomeCLI C)
add_executable(metronome-cli
    source/metronome-cli.c
)
target_include_directories(metronome-cli PRIVATE
    3rd-party/miniaudio
    3rd-party/cjson
)
if(UNIX)
    target_link_libraries(metronome-cli PRIVATE
        metronome
        m
        pthread
    )
endif()

project(MetronomeTUI C)
add_executable(metronome-tui
    source/metronome-tui.c
)
target_include_directories(metronome-tui PRIVATE
    3rd-party/miniaudio
    3rd-party/cjson
)
if(UNIX)
    target_link_libraries(metronome-tui PRIVATE
        metronome
        ncurses
        m
        pthread
    )
endif()
