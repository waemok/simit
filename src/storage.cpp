#include "storage.h"

#include "ir.h"
#include "ir_visitor.h"

using namespace std;

namespace simit {
namespace ir {

// class TensorStorage
struct TensorStorage::Content {
  Kind kind;

  /// The target set that was used to assemble the system if the tensor is
  /// stored on a system, undefined otherwise.
  Expr systemTargetSet;
  Expr systemStorageSet;

  /// Whether the tensor needs storage allocated at runtime.
  bool needsInitialization;

  Content(Kind kind) : kind(kind) {}
};

TensorStorage::TensorStorage() : TensorStorage(Undefined) {
}

TensorStorage::TensorStorage(Kind kind, bool needsInitialization)
    : content(new Content(kind)) {
  content->needsInitialization = needsInitialization;
}

TensorStorage::TensorStorage(const Expr &targetSet)
    : TensorStorage(SystemDiagonal) {
  content->systemTargetSet = targetSet;
}

TensorStorage::TensorStorage(const Expr &targetSet, const Expr &storageSet)
    : TensorStorage(SystemReduced) {
  content->systemTargetSet = targetSet;
  content->systemStorageSet = storageSet;
}

TensorStorage::Kind TensorStorage::getKind() const {
  return content->kind;
}

bool TensorStorage::isSystem() const {
  switch (content->kind) {
    case DenseRowMajor:
      return false;
    case SystemNone:
    case SystemReduced:
    case SystemDiagonal:
      return true;
    case Undefined:
      ierror;
  }
  return false;
}

const Expr &TensorStorage::getSystemTargetSet() const {
  iassert(isSystem()) << "System storages require the target set be provided";
  return content->systemTargetSet;
}

const Expr &TensorStorage::getSystemStorageSet() const {
  iassert(isSystem()) << "System storages require the storage set be provided";
  return content->systemStorageSet;
}

bool TensorStorage::needsInitialization() const {
  return content->needsInitialization;
}

std::ostream &operator<<(std::ostream &os, const TensorStorage &ts) {
  switch (ts.getKind()) {
    case TensorStorage::Undefined:
      os << "Undefined";
      break;
    case TensorStorage::DenseRowMajor:
      os << "Dense Row Major";
      break;
    case TensorStorage::SystemNone:
      os << "System None";
      break;
    case TensorStorage::SystemReduced:
      os << "System Reduced";
      break;
    case TensorStorage::SystemDiagonal:
      os << "System Diagonal";
      break;
  }
  return os;
}

// class Storage
struct Storage::Content {
  std::map<Var,TensorStorage> storage;
};

Storage::Storage() : content(new Content) {
}

void Storage::add(const Var &tensor, TensorStorage tstorage) {
  content->storage.insert(std::pair<Var,TensorStorage>(tensor,tstorage));
}

bool Storage::hasStorage(const Var &tensor) const {
  return content->storage.find(tensor) != content->storage.end();
}

TensorStorage &Storage::get(const Var &tensor) {
  iassert(hasStorage(tensor)) << "no storage specified for tensor" << tensor;
  return content->storage.at(tensor);
}

const TensorStorage &Storage::get(const Var &tensor) const {
  return const_cast<Storage*>(this)->get(tensor);
}

struct Storage::Iterator::Content {
  std::map<Var,TensorStorage>::iterator it;
};

Storage::Iterator::Iterator(Storage::Iterator::Content *content)
    : content(content) {
}

Storage::Iterator::~Iterator() {
  delete content;
}

const Var &Storage::Iterator::operator*() {
  return content->it->first;
}

const Var *Storage::Iterator::operator->() {
  return &content->it->first;
}

Storage::Iterator& Storage::Iterator::operator++() {
  content->it++;
  return *this;
}

bool operator!=(const Storage::Iterator &l, const Storage::Iterator &r) {
  return l.content->it != r.content->it;
}

std::ostream &operator<<(std::ostream &os, const Storage &storage) {
  Storage::Iterator it = storage.begin();
  Storage::Iterator end = storage.end();
  if (it != end) {
    os << *it << " : " << storage.get(*it);
    ++it;
  }
  for (; it != end; ++it) {
    os << std::endl << *it << " : " << storage.get(*it);
  }
  return os;
}

Storage::Iterator Storage::begin() const {
  auto content = new Storage::Iterator::Content;
  content->it = this->content->storage.begin();
  return Storage::Iterator(content);
}

Storage::Iterator Storage::end() const {
  auto content = new Storage::Iterator::Content;
  content->it = this->content->storage.end();
  return Storage::Iterator(content);
}

// Free functions
class GetStorageVisitor : public IRVisitor {
public:
  GetStorageVisitor(Storage *storage) : storage(storage) {}

  void get(Func func) {
    for (auto &global : func.getEnvironment().globals) {
      if (global.first.getType().isTensor()) {
        determineStorage(global.first, false);
      }
    }

    for (auto &arg : func.getArguments()) {
      if (arg.getType().isTensor()) {
        determineStorage(arg, false);
      }
    }

    for (auto &res : func.getResults()) {
      if (res.getType().isTensor()) {
        determineStorage(res, false);
      }
    }

    func.accept(this);
  }

  void get(Stmt stmt) {
    stmt.accept(this);
  }

private:
  Storage *storage;
  
  using IRVisitor::visit;

  void visit(const VarDecl *op) {
    Var var = op->var;
    Type type = var.getType();
    iassert(!storage->hasStorage(var)) << "Redeclaration of variable";
    if (type.isTensor() && !isScalar(type)) {
      determineStorage(var, true);
    }
  }

  void visit(const AssignStmt *op) {
    Var var = op->var;
    Type type = var.getType();
    Type rhsType = op->value.type();
    
    if (type.isTensor() && !isScalar(type) && !storage->hasStorage(var)) {
      // TODO: this is a hack.  We really should carry around TensorStorage
      // as part of the type.
      iassert(type.isTensor());
      const TensorType *ttype = type.toTensor();
      if (!isElementTensorType(ttype) && ttype->order() > 1) {
        // assume system reduced storage
        determineStorage(var, !isa<Literal>(op->value), op->value);
        
      } else {
        bool needsInitialization = !isa<Literal>(op->value);
        determineStorage(var, needsInitialization);
      }
    }
  }

  void visit(const TensorWrite *op) {
    if (isa<VarExpr>(op->tensor)) {
      const Var &var = to<VarExpr>(op->tensor)->var;
      Type type = var.getType();
      if (type.isTensor() && !isScalar(type) && !storage->hasStorage(var)) {
        determineStorage(var);
      }
    }
  }

  void visit(const Map *op) {
    for (auto &var : op->vars) {
      Type type = var.getType();
      if (type.isTensor() && !isScalar(type) && !storage->hasStorage(var)) {
        // For now we'll store all assembled vectors as dense and other tensors
        // as system reduced
        TensorStorage tensorStorage;
        auto tensorType = type.toTensor();
        if (tensorType->order() == 1) {
          tensorStorage = TensorStorage(TensorStorage::DenseRowMajor);
        }
        else if (op->neighbors.defined()) {
          tensorStorage = TensorStorage(op->target, op->neighbors);
        }
        else {
          tensorStorage = TensorStorage(op->target);
        }
        iassert(tensorStorage.getKind() != TensorStorage::Undefined);
        storage->add(var, tensorStorage);
      }
    }
  }

  void determineStorage(Var var, bool initialize=true) {
    // Scalars don't need storage
    if (isScalar(var.getType())) return;

    // If all dimensions are ranges then we choose dense row major. Otherwise,
    // we choose system reduced storage order (for now).
    Type type = var.getType();
    iassert(type.isTensor());
    const TensorType *ttype = type.toTensor();

    TensorStorage tensorStorage;

    // Element tensor (dimensions are not simit sets) are dense. In addition
    // vectors are always dense.
    if (isElementTensorType(ttype) || ttype->order() == 1) {
      tensorStorage = TensorStorage(TensorStorage::DenseRowMajor, initialize);
    }
    else {
      not_supported_yet;
    }
    iassert(tensorStorage.getKind() != TensorStorage::Undefined);

    storage->add(var, tensorStorage);
  }
  
  //TODO: this should be replaced with something that uses the type of rhs
  void determineStorage(Var var, bool initialize, Expr rhs) {
    // class to find leaf vars of an Expr.
    class LeafVarsVisitor : public IRVisitor {
    public:
      std::set<Var> vars;
      using IRVisitor::visit;
      void visit(const VarExpr *op) {
        vars.insert(op->var);
      }
    };

    // find leaf vars in the rhs
    LeafVarsVisitor leafVars;
    rhs.accept(&leafVars);
 
    for (auto v : leafVars.vars) {
      if (storage->hasStorage(v)) {
        const TensorStorage varStorage = storage->get(v);
        if (varStorage.getKind() == TensorStorage::SystemReduced) {
          auto tensorStorage = TensorStorage(varStorage.getSystemTargetSet(),
                                             varStorage.getSystemStorageSet());
          storage->add(var, tensorStorage);
        }
      }
   
    }
  }
};

Storage getStorage(const Func &func) {
  Storage storage;
  updateStorage(func, &storage);
  return storage;
}

Storage getStorage(const Stmt &stmt) {
  Storage storage;
  updateStorage(stmt, &storage);
  return storage;
}

void updateStorage(const Func &func, Storage *storage) {
  GetStorageVisitor(storage).get(func);
}

void updateStorage(const Stmt &stmt, Storage *storage) {
  GetStorageVisitor(storage).get(stmt);
}

}}
