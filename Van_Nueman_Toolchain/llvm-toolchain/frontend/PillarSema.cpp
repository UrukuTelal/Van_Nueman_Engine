//===-- PillarSema.cpp - Semantic Analysis for Pillar Types -----------===//
//
// Semantic analysis for Van_Nueman pillar types and attributes
// Validates PillarStateVector usage, scaled integers, etc.
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <string>
#include <vector>

namespace vn {

// Visitor to validate pillar-related AST nodes
class PillarASTVisitor {
public:
  PillarASTVisitor() {}

  // Check struct declarations for pillar_vector attribute
  bool VisitRecordDecl(const std::string& name, const std::vector<std::string>& fields) {
    // Validate: must have exactly 16 fields (one per pillar)
    if (fields.size() != 16) {
      std::cerr << "vncc: error: pillar_vector struct must have 16 fields, got "
                << fields.size() << "\n";
      return false;
    }
    return true;
  }

  // Check function declarations for fractal attribute
  bool VisitFunctionDecl(const std::string& name, int numParams) {
    if (numParams == 0) {
      std::cerr << "vncc: warning: fractal function has no parameters\n";
    }
    return true;
  }

  // Check variable declarations for scaled attribute
  bool VisitVarDecl(const std::string& name, const std::string& type) {
    // Validate: type must be integer
    if (type.find("int") == std::string::npos &&
        type.find("long") == std::string::npos) {
      std::cerr << "vncc: error: scaled attribute requires integer type\n";
      return false;
    }
    return true;
  }
};

} // namespace vn
