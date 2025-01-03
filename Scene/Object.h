#pragma once
#include <string>

class Object
{
public:
    Object() {}
    virtual ~Object() {}

    virtual void Tick(float DeltaTime) {}

    void GetObjectType(std::string& Type) { Type = m_objectType; }
    unsigned int GetObjectTypeHash() { return m_objectTypeHash; }

protected:
    std::string m_objectType;
    std::string m_objectName;

    unsigned int m_objectTypeHash;

private:
    
};