#ifndef VN_ATTRIBUTES_H
#define VN_ATTRIBUTES_H

#include <llvm/IR/Attributes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace vn {

// Custom attribute kinds for Van_Nueman
enum class VnAttributeKind {
  PillarVector,    // [[pillar_vector]] - marks a struct as Pillar State Vector
  Scaled,          // [[scaled(N)]] - marks integer as scaled by 2^N
  Fractal,         // [[fractal]] - function applies at multiple scales
  FractalEntity,   // [[fractal::entity]]
  FractalServer,   // [[fractal::server]]
  FractalFederation, // [[fractal::federation]]
  PillarInteraction // [[pillar_interaction(A, B)]]
};

// Helper to create pillar_vector attribute
inline llvm::Attribute createPillarVectorAttr(llvm::LLVMContext& Ctx) {
  return llvm::Attribute::get(Ctx, "pillar_vector");
}

// Helper to create scaled(N) attribute
inline llvm::Attribute createScaledAttr(llvm::LLVMContext& Ctx, int N) {
  return llvm::Attribute::get(Ctx, "scaled", std::to_string(N));
}

// Helper to create fractal attribute
inline llvm::Attribute createFractalAttr(llvm::LLVMContext& Ctx) {
  return llvm::Attribute::get(Ctx, "fractal");
}

// Check if a type has pillar_vector attribute
inline bool hasPillarVectorAttr(const llvm::Type* T) {
  // In practice, this would check metadata or struct attributes
  return false; // Placeholder
}

} // namespace vn

#endif // VN_ATTRIBUTES_H
