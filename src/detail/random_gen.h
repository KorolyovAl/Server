#pragma once

#include <random>

namespace detail {

    template <typename Type>
    inline Type GenerateRandomInt(Type min_inclusive, Type max_inclusive) {
        static thread_local std::mt19937 generator{std::random_device{}()};
        std::uniform_int_distribution<Type> distribution(min_inclusive, max_inclusive);
        return distribution(generator);
    }

    template <typename Type>
    inline Type GenerateRandomInt() {
        return GenerateRandomInt<Type>(std::numeric_limits<Type>::min(), std::numeric_limits<Type>::max());
    }

}