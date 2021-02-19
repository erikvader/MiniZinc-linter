#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <minizinc/astiterator.hh>

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

class ItemVisitor : public MiniZinc::ItemVisitor {
private:
  class EVisitor : public MiniZinc::EVisitor {
  private:
    ItemVisitor &ivis;

  public:
    EVisitor(ItemVisitor &ivis) : ivis(ivis) {}
    void vVarDecl(const MiniZinc::VarDecl &vd) {
      if (isNoDomainVar(vd)) {
        // std::cout << vd << std::endl;
        ivis.vardecs.push_back(&vd);
      }
    }
  };

  EVisitor evis;
  std::vector<const MiniZinc::VarDecl *> &vardecs;

public:
  ItemVisitor(std::vector<const MiniZinc::VarDecl *> &vardecs) : evis(*this), vardecs(vardecs) {}

  void vVarDeclI(MiniZinc::VarDeclI *vdi) {
    if (isNoDomainVar(*vdi->e())) {
      vardecs.push_back(vdi->e());
    }
  }
  void vAssignI(MiniZinc::AssignI *ai) { top_down(evis, ai->e()); }
  void vConstraintI(MiniZinc::ConstraintI *ci) { top_down(evis, ci->e()); }
  void vFunctionI(MiniZinc::FunctionI *fi) { top_down(evis, fi->e()); }
};

class NoDomainVarDecl : public LintRule {
public:
  constexpr NoDomainVarDecl() : LintRule(13, "unbounded-variable") {}
  void run(MiniZinc::Model *model, std::vector<LintResult> &results) const override {
    std::vector<const MiniZinc::VarDecl *> vardecs;
    ItemVisitor ivis(vardecs);
    MiniZinc::iter_items(ivis, model);

    for (const auto &vd : vardecs) {
      const auto &loc = vd->loc();
      results.emplace_back(
          loc.filename().c_str(), this, "no explicit domain on variable declaration",
          LintResult::OneLineMarked{loc.firstLine(), loc.firstColumn(), loc.lastColumn()});
    }
  }
};

} // namespace

REGISTER_RULE(NoDomainVarDecl)
