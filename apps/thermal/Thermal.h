
#ifndef APPS_THERMAL_THERMAL_H_
#define APPS_THERMAL_THERMAL_H_

#include "graph.h"
#include "program.h"
#include "mesh.h"
#include <cmath>

#include "cgnslib.h"

#include "ParameterManager.h"
#include "ParameterManagerMacros.h"
#include "ThermalParameterManager.h"

using namespace simit;

#define TPM ThermalParameterManager

class Thermal {
public:
  Thermal(std::string paramFile, std::string CGNSFileName,
          std::string zoneName, int index_zone);
  virtual ~Thermal();

  // Parameter Manager
  TPM PM;
  int Xsize,Ysize,Zsize;

  // Mesh graphs
  Set *points;
  Set *quads;		//(points, points, points, points);
  Set *faces;		//(quads, quads);
  Set *bcleft;	//(quads);
  Set *bcright;	//(quads);
  Set *bcup;		//(quads);
  Set *bcbottom;	//(quads);

  // input/ouput parameters of the model
  simit::Tensor<double,2> dt;
  simit::Tensor<double,2> cfl;
  simit::Tensor<int,2> 	coupling_direction;
  simit::Tensor<int,2> 	solver_type;
  simit::Tensor<int,2> 	solver_itermax;
  simit::Tensor<double,2> solver_tolerance;
  simit::Tensor<int,4> 	bc_types;

  // Simit functions
  Function solve_thermal;
  Function compute_dt;
  Function flux_interface;
  Function temperature_interface;
  void bindSimitFunc(Function *simFunc);

  // Set boundary conditions for coupling applications
  void setBC_qw(Set *bcIn);
  void setBC_Tw(Set *bcIn);
  bool compareTw(Set *bcIn, double tolerance);
};

#endif /* APPS_THERMAL_THERMAL_H_ */
