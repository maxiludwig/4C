// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef FOUR_C_BEAMINTERACTION_LINK_PINJOINTED_HPP
#define FOUR_C_BEAMINTERACTION_LINK_PINJOINTED_HPP

#include "4C_config.hpp"

#include "4C_beaminteraction_link.hpp"
#include "4C_comm_parobject.hpp"
#include "4C_comm_parobjectfactory.hpp"
#include "4C_inpar_beaminteraction.hpp"
#include "4C_linalg_fixedsizematrix.hpp"

FOUR_C_NAMESPACE_OPEN


// forward declarations
namespace Core::Communication
{
  class PackBuffer;
}
namespace Discret
{
  namespace Elements
  {
    class Beam3Base;
    class Beam3r;
  }  // namespace Elements
}  // namespace Discret

namespace Core::Elements
{
  class Element;
}

namespace Core::LinAlg
{
  class SerialDenseVector;
  class SerialDenseMatrix;
}  // namespace Core::LinAlg

namespace BeamInteraction
{
  class BeamLinkPinJointedType : public Core::Communication::ParObjectType
  {
   public:
    std::string name() const override { return "BeamLinkPinJointedType"; };

    static BeamLinkPinJointedType& instance() { return instance_; };

   private:
    static BeamLinkPinJointedType instance_;
  };


  /*!
   \brief element for interaction of two 3D beam elements via a mechanical linkage
   */
  class BeamLinkPinJointed : public BeamLink
  {
   public:
    //! @name Constructors and destructors and related methods

    //! Constructor
    BeamLinkPinJointed();

    /*!
    \brief Copy Constructor

    Makes a deep copy of a Element

    */
    BeamLinkPinJointed(const BeamLinkPinJointed& old);

    //! Initialization
    void init(int id, const std::vector<std::pair<int, int>>& eleids,
        const std::vector<Core::LinAlg::Matrix<3, 1>>& initpos,
        const std::vector<Core::LinAlg::Matrix<3, 3>>& inittriad,
        Inpar::BeamInteraction::CrosslinkerType linkertype, double timelinkwasset) override;

    //! Setup
    void setup(const int matnum) override;

    /*!
    \brief Return unique ParObject id

    Every class implementing ParObject needs a unique id defined at the
    top of parobject.H
    */
    int unique_par_object_id() const override = 0;

    /*!
    \brief Pack this class so it can be communicated

    \ref pack and \ref unpack are used to communicate this element

    */
    void pack(Core::Communication::PackBuffer& data) const override;

    /*!
    \brief Unpack data from a char vector into this class

    \ref pack and \ref unpack are used to communicate this element

    */
    void unpack(Core::Communication::UnpackBuffer& buffer) override;

    //@}

    //! @name Access methods

    //! get force in first or second binding spot
    void get_binding_spot_force(
        int bspotid, Core::LinAlg::SerialDenseVector& bspotforce) const override
    {
      FOUR_C_THROW(" needs to be implemented in derived classes.");
    }

    // get current length of linker
    virtual double get_current_linker_length() const = 0;

    //@}

    //! @name Public evaluation methods

    /*!
    \brief Evaluate forces
    */
    bool evaluate_force(Core::LinAlg::SerialDenseVector& forcevec1,
        Core::LinAlg::SerialDenseVector& forcevec2) override = 0;

    /*!
    \brief Evaluate stiffness contribution
    */
    bool evaluate_stiff(Core::LinAlg::SerialDenseMatrix& stiffmat11,
        Core::LinAlg::SerialDenseMatrix& stiffmat12, Core::LinAlg::SerialDenseMatrix& stiffmat21,
        Core::LinAlg::SerialDenseMatrix& stiffmat22) override = 0;

    /*!
    \brief Evaluate forces and stiffness contribution
    */
    bool evaluate_force_stiff(Core::LinAlg::SerialDenseVector& forcevec1,
        Core::LinAlg::SerialDenseVector& forcevec2, Core::LinAlg::SerialDenseMatrix& stiffmat11,
        Core::LinAlg::SerialDenseMatrix& stiffmat12, Core::LinAlg::SerialDenseMatrix& stiffmat21,
        Core::LinAlg::SerialDenseMatrix& stiffmat22) override = 0;

    /*
    \brief Update position and triad of both connection sites (a.k.a. binding spots)
    */
    void reset_state(std::vector<Core::LinAlg::Matrix<3, 1>>& bspotpos,
        std::vector<Core::LinAlg::Matrix<3, 3>>& bspottriad) override;

    //! return appropriate instance of the desired class (acts as a simple factory)
    static std::shared_ptr<BeamLinkPinJointed> create(Inpar::BeamInteraction::JointType type);

    void print(std::ostream& out) const;
    //@}

   private:
    //@}
  };

}  // namespace BeamInteraction

FOUR_C_NAMESPACE_CLOSE

#endif
