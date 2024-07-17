#pragma once

#include <stdexcept>

#define ASSERT( condition, message )                                                                                                       \
    if( !( condition ) )                                                                                                                   \
    {                                                                                                                                      \
        throw std::logic_error( message );                                                                                                 \
    }
