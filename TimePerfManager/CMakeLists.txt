set(TIMEPERF_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/TimePerfManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TimePerfManager.h
    PARENT_SCOPE)

target_sources(DX12MiniRenderer PRIVATE ${TIMEPERF_SRC})