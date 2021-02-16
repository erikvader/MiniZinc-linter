#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <minizinc/astiterator.hh>
#include <minizinc/prettyprinter.hh>

namespace {
using namespace LZN;

bool isNoDomainVar(const MiniZinc::VarDecl &vd) {
  auto t = vd.type();
  auto domain = vd.ti()->domain();
  return t.isvar() && t.st() == MiniZinc::Type::SetType::ST_PLAIN &&
         (t.bt() == MiniZinc::Type::BaseType::BT_INT ||
          t.bt() == MiniZinc::Type::BaseType::BT_FLOAT) &&
         t.dim() >= 0 && t.isPresent() && domain == nullptr;
}

class EVisitor : public MiniZinc::EVisitor {
public:
  void vVarDecl(const MiniZinc::VarDecl &vd) {
    if (isNoDomainVar(vd)) {
      std::cout << vd << std::endl;
    }
  }
};

class ItemVisitor : public MiniZinc::ItemVisitor {
private:
  EVisitor udv;

public:
  void vVarDeclI(MiniZinc::VarDeclI *vdi) {
    if (isNoDomainVar(*vdi->e())) {
      debugprint(vdi);
    }
  }
  void vAssignI(MiniZinc::AssignI *ai) { top_down(udv, ai->e()); }
  void vConstraintI(MiniZinc::ConstraintI *ci) { top_down(udv, ci->e()); }
  void vFunctionI(MiniZinc::FunctionI *fi) { top_down(udv, fi->e()); }
};

class NoDomainVarDecl : public LintRule {
public:
  constexpr NoDomainVarDecl() : LintRule(42, "hejsan") {}
  void run(MiniZinc::Model *model, std::vector<LintResult> &results) const override {
    ItemVisitor uvdv;
    MiniZinc::iter_items(uvdv, model);
  }
};

} // namespace

REGISTER_RULE(NoDomainVarDecl)
