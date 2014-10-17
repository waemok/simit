#include "ir_printer.h"

#include "ir.h"
#include "util.h"

using namespace std;

namespace simit {
namespace ir {

std::ostream &operator<<(std::ostream &os, const Func &function) {
  IRPrinter printer(os);
  printer.print(function);
  return os;
}

std::ostream &operator<<(std::ostream &os, const Expr &expr) {
  IRPrinter printer(os);
  printer.print(expr);
  return os;
}

std::ostream &operator<<(std::ostream &os, const Stmt &Stmt) {
  IRPrinter printer(os);
  printer.print(Stmt);
  return os;
}

std::ostream &operator<<(std::ostream &os, const IRNode &node) {
  IRPrinter printer(os);
  printer.print(node);
  return os;
}


// class IRPrinter
IRPrinter::IRPrinter(std::ostream &os, signed indent) : os(os), indentation(0) {
}

void IRPrinter::print(const Func &func) {
  if (func.defined()) {
    func.accept(this);
  }
}

void IRPrinter::print(const Expr &expr) {
  if (expr.defined()) {
    expr.accept(this);
  }
}

void IRPrinter::print(const Stmt &stmt) {
  if (stmt.defined()) {
    stmt.accept(this);
  }
}

void IRPrinter::print(const IRNode &node) {
  node.accept(this);
}

void IRPrinter::visit(const Literal *op) {
  // TODO: Fix value printing to print matrices and tensors properly
  switch (op->type.kind()) {
    case Type::Scalar: // fall-through
    case Type::Tensor: {


      size_t size;
      ScalarType::Kind componentType;
      if (op->type.kind() == Type::Scalar) {
        const ScalarType *type = op->type.toScalar();
        size = 1;
        componentType = type->kind;
      }
      else {
        assert(op->type.kind() == Type::Tensor);
        const TensorType *type = op->type.toTensor();
        size = type->size();
        componentType = type->componentType.toScalar()->kind;
      }

      switch (componentType) {
        case ScalarType::Int: { {
          const int *idata = static_cast<const int*>(op->data);
          if (size == 1) {
            os << idata[0];
          }
          else {
            os << "[" << idata[0];
            for (size_t i=0; i < size; ++i) {
              os << ", " << idata[i];
            }
            os << "]";
          }
          break;
        }
        case ScalarType::Float: {
          const double *fdata = static_cast<const double*>(op->data);
          if (size == 1) {
            os << fdata[0];
          }
          else {
            os << "[" << to_string(fdata[0]);
            for (size_t i=1; i < size; ++i) {
              os << ", " + to_string(fdata[i]);
            }
            os << "]";
          }
          break;
        }
        }
      }
      break;
    }
    case Type::Element:
      NOT_SUPPORTED_YET;
    case Type::Set:
      NOT_SUPPORTED_YET;
      break;
    case Type::Tuple:
      NOT_SUPPORTED_YET;
      break;
  }
  os << "\n";
}

void IRPrinter::visit(const Variable *op) {
  os << op->name;
}

void IRPrinter::visit(const Result *) {
  os << "result";
}

void IRPrinter::visit(const FieldRead *op) {
  print(op->elementOrSet);
  os << "." << op->fieldName;
}

void IRPrinter::visit(const TensorRead *op) {
  print(op->tensor);
  os << "(";
  auto indices = op->indices;
  if (indices.size() > 0) {
    print(indices[0]);
  }
  for (size_t i=1; i < indices.size(); ++i) {
    os << ",";
    print(indices[i]);
  }
  os << ")";
}

void IRPrinter::visit(const TupleRead *op) {
  print(op->tuple);
  os << "(";
  print(op->index);
  os << ")";
}

void IRPrinter::visit(const Map *op) {
  os << "map " << op->function;
  os << " to ";
  print(op->target);
  os << " with ";
  print(op->neighbors);
  os << " reduce " << op->reductionOp;
}

void IRPrinter::visit(const IndexedTensor *op) {
  print(op->tensor);
  if (op->indexVars.size() > 0) {
    os << "(" << util::join(op->indexVars,",") << ")";
  }
}

void IRPrinter::visit(const IndexExpr *op) {
  if (op->lhsIndexVars.size() != 0) {
    os << "(" + simit::util::join(op->lhsIndexVars, ",") + ") ";
  }
  print(op->rhs);
}

void IRPrinter::visit(const Call *op) {
  os << "Call";
}

void IRPrinter::visit(const Neg *op) {
  os << "-";
  print(op->a);
}

void IRPrinter::visit(const Add *op) {
  os << "(";
  print(op->a);
  os << " + ";
  print(op->b);
  os << ")";
}

void IRPrinter::visit(const Sub *op) {
  os << "(";
  print(op->a);
  os << " - ";
  print(op->b);
  os << ")";
}

void IRPrinter::visit(const Mul *op) {
  os << "(";
  print(op->a);
  os << " * ";
  print(op->b);
  os << ")";
}

void IRPrinter::visit(const Div *op) {
  os << "(";
  print(op->a);
  os << " / ";
  print(op->b);
  os << ")";
}

void IRPrinter::visit(const AssignStmt *op) {
  indent();
  os << util::join(op->lhs) << " = ";
  print(op->rhs);
  os << ";\n";
}

void IRPrinter::visit(const FieldWrite *op) {
  indent();
  print(op->elementOrSet);
  os << "." << op->fieldName << " = ";
  print(op->value);
  os << ";\n";
}

void IRPrinter::visit(const TensorWrite *op) {
  indent();
  print(op->tensor);
  os << "(";
  auto indices = op->indices;
  if (indices.size() > 0) {
    print(indices[0]);
  }
  for (size_t i=1; i < indices.size(); ++i) {
    os << ",";
    print(indices[i]);
  }
  os << ") = ";
  print(op->value);
  os << ";\n";
}

void IRPrinter::visit(const For *op) {
  indent();
  os << "for;\n";
}

void IRPrinter::visit(const IfThenElse *op) {
  indent();
  os << "ifthenelse;\n";
}

void IRPrinter::visit(const Block *op) {
  indent();
  print(op->first);
  print(op->rest);
}

void IRPrinter::visit(const Pass *op) {
  indent();
  os << "pass;\n";
}

void IRPrinter::visit(const Func *func) {
  os << "func " << func->getName() << "(";
  if (func->getArguments().size() > 0) {
    Expr arg = func->getArguments()[0];
    print(arg);
    os << " : " << arg.type();
  }
  for (size_t i=1; i < func->getArguments().size(); ++i) {
    Expr arg = func->getArguments()[i];
    os << ", ";
    print(arg);
    os << " : " << arg.type();
  }
  os << ")";

  if (func->getResults().size() > 0) {
    os << " -> (";
    print(func->getResults()[0]);
    os << " : " << func->getResults()[0].type();

    for (size_t i=1; i < func->getResults().size(); ++i) {
      Expr res = func->getResults()[i];
      os << ", ";
      print(res);
      os << " : " << res.type();
    }
    os << ")";
  }

  os << "\n";
  ++indentation;
  print(func->getBody());
  --indentation;
  os << "end";
}

void IRPrinter::indent() {
  for (unsigned i=0; i<indentation; ++i) {
    os << "  ";
  }
}

}} //namespace simit::ir
