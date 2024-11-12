#pragma once
#include <string>

class Object
{
public:
    Object() {}
    ~Object() {}

    virtual void Tick(float DeltaTime) {}

    void GetObjectType(std::string& Type) { Type = m_objectType; }

protected:
    std::string m_objectType;
    std::string m_objectName;

private:
    
};