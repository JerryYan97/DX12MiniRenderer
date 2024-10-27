#include "EventManager.h"
#include "../Utils/crc32.h"

// ================================================================================================================
HEvent::HEvent(
    const HEventArguments& arg,
    const std::string& type)
    : m_isHandled(false)
{
    m_typeHash = crc32(type.data());
    m_arg = arg;
}

// ================================================================================================================
void HEventManager::RegisterListener(
    const std::string&      type,
    NoParameterEventFuncPtr listenFunc)
{
    size_t typeHash = crc32(type.data());

    if (m_eventListenerMap.find(typeHash) != m_eventListenerMap.end())
    {
        // There are listeners for the target event type. Add the entity handle into the listener list.
        m_eventListenerMap[typeHash].push_back(listenFunc);
    }
    else
    {
        // No listener for the target event type. Create the listener list and add the entity into it.
        m_eventListenerMap[typeHash] = std::list<NoParameterEventFuncPtr>();
        m_eventListenerMap[typeHash].push_back(listenFunc);
    }
}

// ================================================================================================================
void HEventManager::SendEvent(
    HEvent& hEvent)
{
    if (m_eventListenerMap.find(hEvent.GetEventType()) != m_eventListenerMap.end())
    {
        std::list<NoParameterEventFuncPtr>& list = m_eventListenerMap[hEvent.GetEventType()];
        for (auto& itr : list)
        {
            itr();
        }
    }
}