#pragma once

#include <stdexcept>
#include <format>

#define ASSERT( condition, message )                                                                                                       \
    if( !( condition ) )                                                                                                                   \
    {                                                                                                                                      \
        throw std::logic_error( message );                                                                                                 \
    }

#define ASSERT_FORMAT( condition, message )                                                                                                \
    if( !( condition ) )                                                                                                                   \
    {                                                                                                                                      \
        throw std::format_error( message );                                                                                                \
    }
