#pragma once

#include <memory>

namespace clib_util {
    namespace singleton {
        template <class T>
        class ISingleton {
        public:
            [[nodiscard]] static T* GetSingleton() {
                static T singleton;
                return std::addressof(singleton);
            }
        protected:
            ISingleton() = default;
            ISingleton(const ISingleton&) = delete;
            ISingleton(ISingleton&&) = delete;
            ISingleton& operator=(const ISingleton&) = delete;
            ISingleton& operator=(ISingleton&&) = delete;
            ~ISingleton() = default;
        };
    }
}
