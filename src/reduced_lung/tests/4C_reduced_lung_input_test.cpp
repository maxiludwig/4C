// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "4C_reduced_lung_input.hpp"

#include "4C_io_input_parameter_container.hpp"
#include "4C_io_input_spec.hpp"
#include "4C_io_yaml.hpp"
#include "4C_unittest_utils_assertions_test.hpp"

#include <string>

namespace
{
  using namespace FourC;

  //! A complete `reduced_dimensional_lung` section carrying one pressure boundary condition, so
  //! that a test can make a single value invalid and match the rest.
  std::string input_with_pressure_condition(const std::string& id, const std::string& function_id)
  {
    return R"(
reduced_dimensional_lung:
  air_properties:
    density: 1.176e-06
    dynamic_viscosity: 1.79105e-05
  dynamics:
    linear_solver: 1
    number_of_steps: 1
    restart_every: 1
    results_every: 1
    time_increment: 1
  geometry:
    file: "mesh.vtu"
  lung_tree:
    airways:
      element_blocks: [1]
      radius:
        constant: 1.0
      flow_model:
        resistance_type:
          constant: Linear
        include_inertia:
          constant: false
      wall_model_type:
        constant: Rigid
    terminal_units:
      element_blocks: [2]
      v0:
        constant: 1.0
      rheological_model:
        rheological_model_type:
          constant: KelvinVoigt
        kelvin_voigt:
          viscosity_kelvin_voigt_eta:
            constant: 0.0
      elasticity_model:
        elasticity_model_type:
          constant: Linear
        linear:
          elasticity_e:
            constant: 1.0
  boundary_conditions:
    pressure:
      - id: )" +
           id +
           R"(
        function_id: )" +
           function_id + "\n";
  }

  Core::IO::InputParameterContainer match_input(const std::string& yaml_string)
  {
    Core::IO::InputParameterContainer container;
    ryml::Tree tree = Core::IO::init_yaml_tree_with_exceptions();
    ryml::NodeRef root = tree.rootref();
    ryml::parse_in_arena(ryml::to_csubstr(yaml_string), root);

    Core::IO::ConstYamlNodeRef node(root, "");
    ReducedLung::valid_parameters().match(node, container);
    return container;
  }

  //! The valid input from above with an `initial_volume` block added to the terminal units.
  std::string input_with_initial_volume(const std::string& initial_volume_block)
  {
    std::string input = input_with_pressure_condition("1", "1");
    const std::string v0_block = "      v0:\n        constant: 1.0\n";
    input.insert(input.find(v0_block) + v0_block.size(), initial_volume_block);
    return input;
  }

  double parsed_initial_volume(const std::string& yaml_string)
  {
    const auto container = match_input(yaml_string);
    return container.get<ReducedLung::ReducedLungParameters>("reduced_dimensional_lung")
        .lung_tree.terminal_units.initial_volume.at(0, "initial_volume");
  }

  //! The valid input from above with a `time_switched` definition with @p phases_block added.
  std::string input_with_time_switched(const std::string& phases_block)
  {
    return input_with_pressure_condition("1", "1") +
           "    time_switched:\n      - id: 2\n        phases:\n" + phases_block;
  }

  ReducedLung::ReducedLungParameters::BoundaryConditions parsed_boundary_conditions(
      const std::string& yaml_string)
  {
    const auto container = match_input(yaml_string);
    return container.get<ReducedLung::ReducedLungParameters>("reduced_dimensional_lung")
        .boundary_conditions;
  }

  TEST(ReducedLungInputTest, AcceptsPositiveDefinitionAndFunctionIds)
  {
    EXPECT_NO_THROW(match_input(input_with_pressure_condition("1", "1")));
  }

  //! The id 0 marks nodes without a boundary condition and can therefore not be defined.
  TEST(ReducedLungInputTest, RejectsNonPositiveDefinitionId)
  {
    FOUR_C_EXPECT_THROW_WITH_MESSAGE(match_input(input_with_pressure_condition("0", "1")),
        Core::Exception, "'id' does not pass validation");
  }

  TEST(ReducedLungInputTest, RejectsNonPositiveFunctionId)
  {
    FOUR_C_EXPECT_THROW_WITH_MESSAGE(match_input(input_with_pressure_condition("1", "-1")),
        Core::Exception, "'function_id' does not pass validation");
  }

  //! Without `initial_volume` the default 0 is stored, i.e. the units start at v0.
  TEST(ReducedLungInputTest, InitialVolumeDefaultsToZero)
  {
    EXPECT_DOUBLE_EQ(parsed_initial_volume(input_with_pressure_condition("1", "1")), 0.0);
  }

  TEST(ReducedLungInputTest, ReadsInitialVolume)
  {
    EXPECT_DOUBLE_EQ(parsed_initial_volume(input_with_initial_volume(
                         "      initial_volume:\n        constant: 2.5\n")),
        2.5);
  }

  TEST(ReducedLungInputTest, TimeSwitchedDefaultsToEmpty)
  {
    EXPECT_TRUE(
        parsed_boundary_conditions(input_with_pressure_condition("1", "1")).time_switched.empty());
  }

  TEST(ReducedLungInputTest, ReadsTimeSwitchedPhases)
  {
    using Variable =
        ReducedLung::ReducedLungParameters::BoundaryConditions::TimeSwitchedDefinition::Variable;
    const auto boundary_conditions =
        parsed_boundary_conditions(input_with_time_switched(R"(          - variable: Flow
            function_id: 3
            end_time: 2.0
          - variable: Pressure
            function_id: 4
            end_time: 7.5
          - variable: Flow
            function_id: 5
            end_time: 20
)"));

    ASSERT_EQ(boundary_conditions.time_switched.size(), 1u);
    const auto& definition = boundary_conditions.time_switched.front();
    EXPECT_EQ(definition.id, 2);
    ASSERT_EQ(definition.phases.size(), 3u);
    EXPECT_EQ(definition.phases[0].variable, Variable::Flow);
    EXPECT_EQ(definition.phases[0].function_id, 3);
    EXPECT_DOUBLE_EQ(definition.phases[0].end_time, 2.0);
    EXPECT_EQ(definition.phases[1].variable, Variable::Pressure);
    EXPECT_EQ(definition.phases[1].function_id, 4);
    EXPECT_DOUBLE_EQ(definition.phases[1].end_time, 7.5);
    EXPECT_EQ(definition.phases[2].variable, Variable::Flow);
    EXPECT_EQ(definition.phases[2].function_id, 5);
    EXPECT_DOUBLE_EQ(definition.phases[2].end_time, 20.0);
  }

  TEST(ReducedLungInputTest, RejectsUnknownTimeSwitchedVariable)
  {
    EXPECT_ANY_THROW(match_input(input_with_time_switched(R"(          - variable: Resistance
            function_id: 3
            end_time: 2.0
)")));
  }

  TEST(ReducedLungInputTest, RejectsNonPositiveTimeSwitchedEndTime)
  {
    FOUR_C_EXPECT_THROW_WITH_MESSAGE(
        match_input(input_with_time_switched(R"(          - variable: Flow
            function_id: 3
            end_time: 0.0
)")),
        Core::Exception, "'end_time' does not pass validation");
  }
}  // namespace
