cmake_minimum_required(VERSION 3.12)

project(MetronomeProject C)

add_executable(metronome
    source/main.c
)

target_include_directories(metronome PRIVATE
    3rd-party/miniaudio
)

if(UNIX)
    target_link_libraries(metronome PRIVATE
        m
        pthread
    )
endif()
