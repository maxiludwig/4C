// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_inpar_contact.hpp"

#include "4C_inpar_structure.hpp"
#include "4C_utils_parameter_list.hpp"

FOUR_C_NAMESPACE_OPEN



void Inpar::CONTACT::set_valid_parameters(std::map<std::string, Core::IO::InputSpec>& list)
{
  using Teuchos::tuple;

  /* parameters for structural meshtying and contact */
  Core::Utils::SectionSpecs scontact{"CONTACT DYNAMIC"};

  Core::Utils::int_parameter(
      "LINEAR_SOLVER", -1, "number of linear solver used for meshtying and contact", scontact);

  Core::Utils::bool_parameter("RESTART_WITH_CONTACT", false,
      "Must be chosen if a non-contact simulation is to be restarted with contact", scontact);

  Core::Utils::string_to_integral_parameter<Inpar::CONTACT::AdhesionType>("ADHESION", "None",
      "Type of adhesion law", tuple<std::string>("None", "none", "bounded", "b"),
      tuple<Inpar::CONTACT::AdhesionType>(
          adhesion_none, adhesion_none, adhesion_bound, adhesion_bound),
      scontact);

  Core::Utils::string_to_integral_parameter<Inpar::CONTACT::FrictionType>("FRICTION", "None",
      "Type of friction law", tuple<std::string>("None", "Stick", "Tresca", "Coulomb"),
      tuple<Inpar::CONTACT::FrictionType>(
          friction_none, friction_stick, friction_tresca, friction_coulomb),
      scontact);

  Core::Utils::bool_parameter("FRLESS_FIRST", false,
      "If chosen the first time step of a newly in contact slave node is regarded as frictionless",
      scontact);

  Core::Utils::bool_parameter("GP_SLIP_INCR", false,
      "If chosen the slip increment is computed gp-wise which results to a non-objective quantity, "
      "but this would be consistent to wear and tsi calculations.",
      scontact);

  Core::Utils::string_to_integral_parameter<Inpar::CONTACT::SolvingStrategy>("STRATEGY",
      "LagrangianMultipliers", "Type of employed solving strategy",
      tuple<std::string>("LagrangianMultipliers", "lagrange", "Lagrange", "penalty", "Penalty",
          "Uzawa", "Nitsche", "Ehl", "MultiScale"),
      tuple<Inpar::CONTACT::SolvingStrategy>(solution_lagmult, solution_lagmult, solution_lagmult,
          solution_penalty, solution_penalty, solution_uzawa, solution_nitsche, solution_ehl,
          solution_multiscale),
      scontact);

  Core::Utils::string_to_integral_parameter<Inpar::CONTACT::SystemType>("SYSTEM", "Condensed",
      "Type of linear system setup / solution",
      tuple<std::string>("Condensed", "condensed", "cond", "Condensedlagmult", "condensedlagmult",
          "condlm", "SaddlePoint", "Saddlepoint", "saddlepoint", "sp", "none"),
      tuple<Inpar::CONTACT::SystemType>(system_condensed, system_condensed, system_condensed,
          system_condensed_lagmult, system_condensed_lagmult, system_condensed_lagmult,
          system_saddlepoint, system_saddlepoint, system_saddlepoint, system_saddlepoint,
          system_none),
      scontact);

  Core::Utils::double_parameter("PENALTYPARAM", 0.0,
      "Penalty parameter for penalty / Uzawa augmented solution strategy", scontact);
  Core::Utils::double_parameter("PENALTYPARAMTAN", 0.0,
      "Tangential penalty parameter for penalty / Uzawa augmented solution strategy", scontact);
  Core::Utils::int_parameter(
      "UZAWAMAXSTEPS", 10, "Maximum no. of Uzawa steps for Uzawa solution strategy", scontact);
  Core::Utils::double_parameter("UZAWACONSTRTOL", 1.0e-8,
      "Tolerance of constraint norm for Uzawa solution strategy", scontact);

  Core::Utils::bool_parameter(
      "SEMI_SMOOTH_NEWTON", true, "If chosen semi-smooth Newton concept is applied", scontact);

  Core::Utils::double_parameter(
      "SEMI_SMOOTH_CN", 1.0, "Weighting factor cn for semi-smooth PDASS", scontact);
  Core::Utils::double_parameter(
      "SEMI_SMOOTH_CT", 1.0, "Weighting factor ct for semi-smooth PDASS", scontact);

  Core::Utils::bool_parameter("CONTACTFORCE_ENDTIME", false,
      "If chosen, the contact force is not evaluated at the generalized midpoint, but at the end "
      "of the time step",
      scontact);

  Core::Utils::bool_parameter(
      "VELOCITY_UPDATE", false, "If chosen, velocity update method is applied", scontact);

  Core::Utils::bool_parameter(
      "INITCONTACTBYGAP", false, "Initialize init contact by weighted gap vector", scontact);

  Core::Utils::double_parameter("INITCONTACTGAPVALUE", 0.0,
      "Value for initialization of init contact set with gap vector", scontact);

  // solver convergence test parameters for contact/meshtying in saddlepoint formulation
  Core::Utils::string_to_integral_parameter<Inpar::Solid::BinaryOp>("NORMCOMBI_RESFCONTCONSTR",
      "And", "binary operator to combine contact constraints and residual force values",
      tuple<std::string>("And", "Or"),
      tuple<Inpar::Solid::BinaryOp>(Inpar::Solid::bop_and, Inpar::Solid::bop_or), scontact);

  Core::Utils::string_to_integral_parameter<Inpar::Solid::BinaryOp>("NORMCOMBI_DISPLAGR", "And",
      "binary operator to combine displacement increments and Lagrange multiplier increment values",
      tuple<std::string>("And", "Or"),
      tuple<Inpar::Solid::BinaryOp>(Inpar::Solid::bop_and, Inpar::Solid::bop_or), scontact);

  Core::Utils::double_parameter("TOLCONTCONSTR", 1.0E-6,
      "tolerance in the contact constraint norm for the newton iteration (saddlepoint formulation "
      "only)",
      scontact);
  Core::Utils::double_parameter("TOLLAGR", 1.0E-6,
      "tolerance in the LM norm for the newton iteration (saddlepoint formulation only)", scontact);

  Core::Utils::string_to_integral_parameter<Inpar::CONTACT::ConstraintDirection>(
      "CONSTRAINT_DIRECTIONS", "ntt",
      "formulation of constraints in normal/tangential or xyz-direction",
      tuple<std::string>("ntt", "xyz"),
      tuple<Inpar::CONTACT::ConstraintDirection>(constr_ntt, constr_xyz), scontact);

  Core::Utils::bool_parameter("NONSMOOTH_GEOMETRIES", false,
      "If chosen the contact algorithm combines mortar and nts formulations. This is needed if "
      "contact between entities of different geometric dimension (such as contact between surfaces "
      "and lines, or lines and nodes) can occur",
      scontact);

  Core::Utils::bool_parameter("NONSMOOTH_CONTACT_SURFACE", false,
      "This flag is used to alter the criterion for the evaluation of the so-called qualified "
      "vectors in the case of a self contact scenario. This is needed as the standard criterion is "
      "only valid for smooth surfaces and thus has to be altered, if the surface that is defined "
      "to be a self contact surface is non-smooth!",
      scontact);

  Core::Utils::double_parameter("HYBRID_ANGLE_MIN", -1.0,
      "Non-smooth contact: angle between cpp normal and element normal: begin transition (Mortar)",
      scontact);
  Core::Utils::double_parameter("HYBRID_ANGLE_MAX", -1.0,
      "Non-smooth contact: angle between cpp normal and element normal: end transition (NTS)",
      scontact);

  Core::Utils::bool_parameter("CPP_NORMALS", false,
      "If chosen the nodal normal field is created as averaged CPP normal field.", scontact);

  Core::Utils::bool_parameter(
      "TIMING_DETAILS", false, "Enable and print detailed contact timings to screen.", scontact);

  // --------------------------------------------------------------------------
  Core::Utils::double_parameter(
      "NITSCHE_THETA", 0.0, "+1: symmetric, 0: non-symmetric, -1: skew-symmetric", scontact);
  Core::Utils::double_parameter("NITSCHE_THETA_2", 1.0,
      "+1: Chouly-type, 0: Burman penalty-free (only with theta=-1)", scontact);

  Core::Utils::string_to_integral_parameter<Inpar::CONTACT::NitscheWeighting>("NITSCHE_WEIGHTING",
      "harmonic", "how to weight consistency terms in Nitsche contact formulation",
      tuple<std::string>("slave", "master", "harmonic"),
      tuple<Inpar::CONTACT::NitscheWeighting>(NitWgt_slave, NitWgt_master, NitWgt_harmonic),
      scontact);

  Core::Utils::bool_parameter("NITSCHE_PENALTY_ADAPTIVE", true,
      "adapt penalty parameter after each converged time step", scontact);

  Core::Utils::bool_parameter("REGULARIZED_NORMAL_CONTACT", false,
      "add a regularized normal contact formulation", scontact);
  Core::Utils::double_parameter(
      "REGULARIZATION_THICKNESS", -1., "maximum contact penetration", scontact);
  Core::Utils::double_parameter("REGULARIZATION_STIFFNESS", -1.,
      "initial contact stiffness (i.e. initial \"penalty parameter\")", scontact);

  scontact.move_into_collection(list);
}

FOUR_C_NAMESPACE_CLOSE
