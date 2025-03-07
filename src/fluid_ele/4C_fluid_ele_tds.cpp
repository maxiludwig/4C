// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_fluid_ele_tds.hpp"

#include "4C_comm_pack_helpers.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN


FLD::TDSEleDataType FLD::TDSEleDataType::instance_;


/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 |                                                            gjb 12/12 |
 *----------------------------------------------------------------------*/
void FLD::TDSEleData::pack(Core::Communication::PackBuffer& data) const
{
  // pack type of this instance of ParObject
  int type = unique_par_object_id();
  add_to_pack(data, type);

  // history variables
  add_to_pack(data, saccn_.numRows());
  add_to_pack(data, saccn_.numCols());

  int size = saccn_.numRows() * saccn_.numCols() * sizeof(double);

  add_to_pack(data, saccn_.values(), size);
  add_to_pack(data, svelnp_.values(), size);
  add_to_pack(data, sveln_.values(), size);

  return;
}

/*----------------------------------------------------------------------*
 |  Unpack data                                                (public) |
 |                                                            gjb 12/12 |
 *----------------------------------------------------------------------*/
void FLD::TDSEleData::unpack(Core::Communication::UnpackBuffer& buffer)
{
  Core::Communication::extract_and_assert_id(buffer, unique_par_object_id());

  // history variables (subgrid-scale velocities, accelerations and pressure)
  {
    int firstdim;
    int secondim;

    extract_from_pack(buffer, firstdim);
    extract_from_pack(buffer, secondim);


    saccn_.shape(firstdim, secondim);
    svelnp_.shape(firstdim, secondim);
    sveln_.shape(firstdim, secondim);


    int size = firstdim * secondim * sizeof(double);

    extract_from_pack(buffer, saccn_.values(), size);
    extract_from_pack(buffer, svelnp_.values(), size);
    extract_from_pack(buffer, sveln_.values(), size);
  }


  return;
}



/*----------------------------------------------------------------------*
 |  activate time dependent subgrid scales (public)           gjb 12/12 |
 *----------------------------------------------------------------------*/
void FLD::TDSEleData::activate_tds(
    int nquad, int nsd, double** saccn, double** sveln, double** svelnp)
{
  if (saccn_.numRows() != nsd || saccn_.numCols() != nquad)
  {
    saccn_.shape(nsd, nquad);
    sveln_.shape(nsd, nquad);
    svelnp_.shape(nsd, nquad);
  }
  // for what's this exactly?
  if (saccn != nullptr) *saccn = saccn_.values();
  if (sveln != nullptr) *sveln = sveln_.values();
  if (svelnp != nullptr) *svelnp = svelnp_.values();
}



/*----------------------------------------------------------------------*
 |  activate time dependent subgrid-scales (public)           gjb 12/12 |
 *----------------------------------------------------------------------*/
void FLD::TDSEleData::update_svelnp_in_one_direction(const double fac1, const double fac2,
    const double fac3, const double resM, const double alphaF, const int dim, const int iquad,
    double& svelaf)
{
  /*
      ~n+1           ~n           ~ n            n+1
      u    =  fac1 * u  + fac2 * acc  -fac3 * res
       (i)

  */

  svelnp_(dim, iquad) = fac1 * sveln_(dim, iquad) + fac2 * saccn_(dim, iquad) - fac3 * resM;

  /* compute the intermediate value of subscale velocity

              ~n+af            ~n+1                   ~n
              u     = alphaF * u     + (1.0-alphaF) * u
               (i)              (i)

  */
  svelaf = alphaF * svelnp_(dim, iquad) + (1.0 - alphaF) * sveln_(dim, iquad);

  return;
}

/*----------------------------------------------------------------------*
 |  activate time-dependent subgrid scales (public)           gjb 12/12 |
 *----------------------------------------------------------------------*/
void FLD::TDSEleData::update_svelnp_in_one_direction(const double fac1, const double fac2,
    const double fac3, const double resM, const double alphaF, const int dim, const int iquad,
    double& svelnp, double& svelaf)
{
  update_svelnp_in_one_direction(fac1, fac2, fac3, resM, alphaF, dim, iquad, svelaf);

  svelnp = svelnp_(dim, iquad);

  return;
}

/*----------------------------------------------------------------------*
 |  update time dependent subgrid scales (public)             gjb 12/12 |
 *----------------------------------------------------------------------*/
void FLD::TDSEleData::update(const double dt, const double gamma)
{
  // the old subgrid-scale acceleration for the next timestep is calculated
  // on the fly, not stored on the element
  /*
                   ~n+1   ~n
           ~ n+1     u    - u     ~ n   / 1.0-gamma \
          acc    =   --------- - acc * |  ---------  |
                     gamma*dt           \   gamma   /

           ~ n       ~ n+1   / 1.0-gamma \
          acc    =    acc * |  ---------  |
   */

  // variable in space dimensions
  const int nsd = saccn_.numRows();  // does this always hold?
  for (int rr = 0; rr < nsd; ++rr)
  {
    for (int mm = 0; mm < svelnp_.numCols(); ++mm)
    {
      saccn_(rr, mm) = (svelnp_(rr, mm) - sveln_(rr, mm)) / (gamma * dt) -
                       saccn_(rr, mm) * (1.0 - gamma) / gamma;
    }
  }

  // most recent subgrid-scale velocity becomes the old subscale velocity
  // for the next timestep
  //
  //  ~n   ~n+1
  //  u <- u
  //
  // variable in space dimensions
  for (int rr = 0; rr < nsd; ++rr)
  {
    for (int mm = 0; mm < svelnp_.numCols(); ++mm)
    {
      sveln_(rr, mm) = svelnp_(rr, mm);
    }
  }

  return;
}

FOUR_C_NAMESPACE_CLOSE
