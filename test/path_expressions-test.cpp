#include "gtest/gtest.h"

#include "path_expressions.h"
#include "types.h"

using namespace std;
using namespace simit::pe;

TEST(PathExpression, EV) {
  Var e = Var("e");
  Var v = Var("v");
  PathExpression ev = EV::make(e, v);

  ASSERT_EQ(ev.getPathEndpoint(0), e);
  ASSERT_EQ(ev.getPathEndpoint(1), v);
  ASSERT_EQ(ev, ev);

  // Check that two different EV are equal (equal means that if the variables of
  // both EV expressions are bound to the same sets, the resulting bound
  // expressions are equal)
  Var f = Var("f");
  Var u = Var("u");
  PathExpression fu = EV::make(f, u);

  ASSERT_EQ(ev, fu);

  // Bind the same sets to ev and fu and compare them
  // TODO
  // ASSERT_EQ(boundev, boundfu);

  // Bind different sets to ev and fu and compare them
  // TODO
  // ASSERT_NQ(boundev, boundfu);
}

TEST(PathExpression, VE) {
  Var v = Var("v");
  Var e = Var("e");
  PathExpression ve = VE::make(v, e);

  ASSERT_EQ(ve.getPathEndpoint(0), v);
  ASSERT_EQ(ve.getPathEndpoint(1), e);
  ASSERT_EQ(ve, ve);

  // Check that two different VE are equal
  Var u = Var("u");
  Var f = Var("f");
  PathExpression uf = VE::make(u, f);

  ASSERT_EQ(ve, uf);
}

TEST(PathExpression, Formula) {
  Var vi("vi");
  Var  e("e");
  Var vj("vj");
  PathExpression ve = VE::make(vi, e);
  PathExpression ev = EV::make(e, vj);
  QuantifiedVar qe = QuantifiedVar(QuantifiedVar::Existential, e);
  PathExpression vev = And::make({vi,vj}, {qe}, ve, ev);

  ASSERT_EQ(vev.getPathEndpoint(0), vi);
  ASSERT_EQ(vev.getPathEndpoint(1), vj);
  ASSERT_EQ(vev, vev);

  Var ui("ui");
  Var  f("f");
  Var uj("uj");
  PathExpression uf = VE::make(ui, f);
  PathExpression fu = EV::make(f, uj);
  QuantifiedVar qf = QuantifiedVar(QuantifiedVar::Existential, f);
  PathExpression ufu = And::make({ui,uj}, {qf}, uf, fu);

  ASSERT_EQ(vev, ufu);

  // TODO: Test eve and compare eve with vev
  // TODO: Test or and compare or and and expressions
}