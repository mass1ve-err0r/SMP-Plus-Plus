//
// Created by Saadat Baig on 11.07.26.
//
#pragma once

#include <tl/expected.hpp>


namespace SMPPlusPlus
{
    struct Error
    {
        std::string message;
    };

    template<typename T>
    using Result = tl::expected<T, Error>;

    namespace Redefinitions
    {

    }

}
