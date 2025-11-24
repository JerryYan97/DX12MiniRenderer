set(EVENTSYSTEM_SRC
    EventManager.cpp
    EventManager.h
    PARENT_SCO)

target_sources(DX12MiniRenderer PRIVATE ${EVENTSYSTEM_SRC})