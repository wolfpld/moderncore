#pragma once

#include "../util/NoCopy.hpp"

class Connector
{
public:
    Connector() = default;
    virtual ~Connector() = default;

    NoCopy( Connector );

    virtual void Render() = 0;
};
