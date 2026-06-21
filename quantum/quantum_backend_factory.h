#pragma once

#include "quantum_interface.h"
#include "classical_fallback_backend.h"
#ifdef VN_USE_CUDA_QUANTUM
#include "cuda_quantum_backend.h"
#endif
#ifdef VN_USE_NATIVE_CUDA
#include "native_quantum_backend.h"
#endif
#include <memory>
#include <cstdlib>
#include <string>

namespace vn {
namespace quantum {

class QuantumBackendFactory {
public:
    static std::unique_ptr<QuantumBackend> create_from_env() {
        const char* env = std::getenv("VN_QUANTUM_BACKEND");
        if (!env) return create(QuantumBackendType::CLASSICAL_FALLBACK);
        std::string val(env);
        if (val == "cuda")   return create(QuantumBackendType::CUDA_QUANTUM);
        if (val == "native") return create(QuantumBackendType::NATIVE_QUANTUM);
        if (val == "auto")   return create_auto();
        return create(QuantumBackendType::CLASSICAL_FALLBACK);
    }

    static std::unique_ptr<QuantumBackend> create(QuantumBackendType type) {
        switch (type) {
            case QuantumBackendType::CLASSICAL_FALLBACK:
                return std::make_unique<ClassicalFallbackBackend>();
            case QuantumBackendType::CUDA_QUANTUM:
#ifdef VN_USE_CUDA_QUANTUM
                return std::make_unique<CudaQuantumBackend>();
#else
                return std::make_unique<ClassicalFallbackBackend>();
#endif
            case QuantumBackendType::NATIVE_QUANTUM:
#ifdef VN_USE_NATIVE_CUDA
                return std::make_unique<NativeQuantumBackend>();
#else
                return std::make_unique<ClassicalFallbackBackend>();
#endif
            default:
                return std::make_unique<ClassicalFallbackBackend>();
        }
    }

    static std::unique_ptr<QuantumBackend> create_auto() {
#ifdef VN_USE_CUDA_QUANTUM
        auto cuda_backend = std::make_unique<CudaQuantumBackend>();
        if (cuda_backend->is_available()) return cuda_backend;
#endif
        return std::make_unique<ClassicalFallbackBackend>();
    }

    static std::unique_ptr<QuantumBackend>& instance_storage() {
        static std::unique_ptr<QuantumBackend> backend = create_from_env();
        return backend;
    }

    static QuantumBackend& instance() {
        return *instance_storage();
    }

    static void reset(QuantumBackendType type) {
        instance_storage() = create(type);
    }
};

} // namespace quantum
} // namespace vn
