#pragma once
#include <string>
#include <unordered_map>
#include <any>
#include <list>

typedef std::unordered_map<size_t, std::any> HEventArguments; // Argument name -- Argument value

class HEvent
{
public:
    explicit HEvent(const HEventArguments& arg, const std::string& type);
    ~HEvent() {};

    size_t GetEventType() const { return m_typeHash; }
    HEventArguments& GetArgs() { return m_arg; }

private:
    HEventArguments m_arg;
    size_t m_typeHash;
    bool m_isHandled;
};

/*
class EventInteractionObject
{
public:
    EventInteractionObject() {};
    ~EventInteractionObject() {};
};
*/

typedef void(*NoParameterEventFuncPtr) ();

// Manage interested objects listening and registration.
class HEventManager
{
public:
    HEventManager()
        : m_eventListenerMap()
    {};
    ~HEventManager() {};

    void RegisterListener(const std::string& type, NoParameterEventFuncPtr listenFunc);
    void UnregisterListener(const std::string& type, NoParameterEventFuncPtr listenFunc) {}
    void SendEvent(HEvent& hEvent);

private:
    std::unordered_map<size_t, std::list<NoParameterEventFuncPtr>> m_eventListenerMap; // Event type -- Linked list of registered function to execute when the event happens.
};