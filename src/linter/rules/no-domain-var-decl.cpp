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
        ivis.add(vd.loc());
      }
    }
  };

  EVisitor evis;
  std::function<void(const MiniZinc::Location &loc)> add;

public:
  ItemVisitor(std::function<void(const MiniZinc::Location &loc)> add) : evis(*this), add(add) {}

  void vVarDeclI(MiniZinc::VarDeclI *vdi) {
    if (isNoDomainVar(*vdi->e())) {
      add(vdi->e()->loc());
    }
  }
  void vAssignI(MiniZinc::AssignI *ai) { top_down(evis, ai->e()); }
  void vConstraintI(MiniZinc::ConstraintI *ci) { top_down(evis, ci->e()); }
  void vFunctionI(MiniZinc::FunctionI *fi) { top_down(evis, fi->e()); }
};

class NoDomainVarDecl : public LintRule {
public:
  constexpr NoDomainVarDecl() : LintRule(13, "unbounded-variable") {}

private:
  virtual void do_run(const MiniZinc::Model *model,
                      std::vector<LintResult> &results) const override {
    auto add = [&results, t = this](const MiniZinc::Location &loc) {
      results.emplace_back(
          loc.filename().c_str(), t, "no explicit domain on variable declaration",
          LintResult::OneLineMarked{loc.firstLine(), loc.firstColumn(), loc.lastColumn()});
    };
    ItemVisitor ivis(add);
    // NOTE: assumes iter_items doesn't modify the model
    MiniZinc::iter_items(ivis, const_cast<MiniZinc::Model *>(model));
  }
};

} // namespace

REGISTER_RULE(NoDomainVarDecl)
