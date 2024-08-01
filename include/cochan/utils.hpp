#pragma once

#include <stdexcept>
#include <format>

#define COCHAN_ASSERT( condition, message )                                                                                                \
    if( !( condition ) )                                                                                                                   \
    {                                                                                                                                      \
        throw std::logic_error( message );                                                                                                 \
    }

#define COCHAN_ASSERT_FORMAT( condition, message )                                                                                         \
    if( !( condition ) )                                                                                                                   \
    {                                                                                                                                      \
        throw std::format_error( message );                                                                                                \
    }
