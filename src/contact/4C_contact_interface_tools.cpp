// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_contact_defines.hpp"
#include "4C_contact_friction_node.hpp"
#include "4C_contact_integrator.hpp"
#include "4C_contact_interface.hpp"
#include "4C_contact_selfcontact_binarytree.hpp"
#include "4C_global_data.hpp"
#include "4C_inpar_contact.hpp"
#include "4C_io_control.hpp"
#include "4C_io_gmsh.hpp"
#include "4C_linalg_utils_densematrix_communication.hpp"
#include "4C_linalg_utils_densematrix_multiply.hpp"
#include "4C_mortar_defines.hpp"
#include "4C_mortar_dofset.hpp"
#include "4C_mortar_element.hpp"
#include "4C_mortar_integrator.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 |  Visualize contact stuff with gmsh                         popp 08/08|
 *----------------------------------------------------------------------*/
void CONTACT::Interface::visualize_gmsh(
    const int step, const int iter, const std::string& file_name_only_prefix) const
{
  //**********************************************************************
  // GMSH output of all interface elements
  //**********************************************************************
  // construct unique filename for gmsh output
  // basic information
  std::ostringstream filename;
  filename << "o/gmsh_output/" << file_name_only_prefix << "_co_id";
  if (id_ < 10)
    filename << 0;
  else if (id_ > 99)
    FOUR_C_THROW("Gmsh output implemented for a maximum of 99 iterations");
  filename << id_;

  // construct unique filename for gmsh output
  // first index = time step index
  filename << "_step";
  if (step < 10)
    filename << 0 << 0 << 0 << 0;
  else if (step < 100)
    filename << 0 << 0 << 0;
  else if (step < 1000)
    filename << 0 << 0;
  else if (step < 10000)
    filename << 0;
  else if (step > 99999)
    FOUR_C_THROW("Gmsh output implemented for a maximum of 99.999 time steps");
  filename << step;

  // construct unique filename for gmsh output
  // second index = Newton iteration index
  filename << "_iter";
  if (iter >= 0)
  {
    if (iter < 10)
      filename << 0;
    else if (iter > 99)
      FOUR_C_THROW("Gmsh output implemented for a maximum of 99 iterations");
    filename << iter;
  }
  else
    filename << "XX";

  // create three files (slave, master and whole interface)
  std::ostringstream filenameslave;
  std::ostringstream filenamemaster;
  filenameslave << filename.str();
  filenamemaster << filename.str();
  filename << "_if.pos";
  filenameslave << "_sl.pos";
  filenamemaster << "_ma.pos";

  // do output to file in c-style
  FILE* fp = nullptr;
  FILE* fps = nullptr;
  FILE* fpm = nullptr;

  //**********************************************************************
  // Start GMSH output
  //**********************************************************************
  for (int proc = 0; proc < Core::Communication::num_mpi_ranks(get_comm()); ++proc)
  {
    if (proc == Core::Communication::my_mpi_rank(get_comm()))
    {
      // open files (overwrite if proc==0, else append)
      if (proc == 0)
      {
        fp = fopen(filename.str().c_str(), "w");
        fps = fopen(filenameslave.str().c_str(), "w");
        fpm = fopen(filenamemaster.str().c_str(), "w");
      }
      else
      {
        fp = fopen(filename.str().c_str(), "a");
        fps = fopen(filenameslave.str().c_str(), "a");
        fpm = fopen(filenamemaster.str().c_str(), "a");
      }

      // write output to temporary std::ostringstream
      std::ostringstream gmshfilecontent;
      std::ostringstream gmshfilecontentslave;
      std::ostringstream gmshfilecontentmaster;
      if (proc == 0)
      {
        gmshfilecontent << "View \" Co-Id " << id_ << " Step " << step << " Iter " << iter
                        << " Iface\" {" << std::endl;
        gmshfilecontentslave << "View \" Co-Id " << id_ << " Step " << step << " Iter " << iter
                             << " Slave\" {" << std::endl;
        gmshfilecontentmaster << "View \" Co-Id " << id_ << " Step " << step << " Iter " << iter
                              << " Master\" {" << std::endl;
      }

      //******************************************************************
      // plot elements
      //******************************************************************
      for (int i = 0; i < idiscret_->num_my_row_elements(); ++i)
      {
        Mortar::Element* element = dynamic_cast<Mortar::Element*>(idiscret_->l_row_element(i));
        int nnodes = element->num_node();
        Core::LinAlg::SerialDenseMatrix coord(3, nnodes);
        element->get_nodal_coords(coord);
        double color = (double)element->owner();

        // local center
        double xi[2] = {0.0, 0.0};

        // 2D linear case (2noded line elements)
        if (element->shape() == Core::FE::CellType::line2)
        {
          if (element->is_slave())
          {
            gmshfilecontent << "SL(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "};" << std::endl;
            gmshfilecontentslave << "SL(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                 << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                 << "," << coord(2, 1) << ")";
            gmshfilecontentslave << "{" << std::scientific << color << "," << color << "};"
                                 << std::endl;
          }
          else
          {
            gmshfilecontent << "SL(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "};" << std::endl;
            gmshfilecontentmaster << "SL(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                  << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                  << "," << coord(2, 1) << ")";
            gmshfilecontentmaster << "{" << std::scientific << color << "," << color << "};"
                                  << std::endl;
          }
        }

        // 2D quadratic case (3noded line elements)
        if (element->shape() == Core::FE::CellType::line3)
        {
          if (element->is_slave())
          {
            gmshfilecontent << "SL2(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontentslave << "SL2(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                 << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                 << "," << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2)
                                 << "," << coord(2, 2) << ")";
            gmshfilecontentslave << "{" << std::scientific << color << "," << color << "," << color
                                 << "};" << std::endl;
          }
          else
          {
            gmshfilecontent << "SL2(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontentmaster << "SL2(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                  << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                  << "," << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2)
                                  << "," << coord(2, 2) << ")";
            gmshfilecontentmaster << "{" << std::scientific << color << "," << color << "," << color
                                  << "};" << std::endl;
          }
        }

        // 3D linear case (3noded triangular elements)
        if (element->shape() == Core::FE::CellType::tri3)
        {
          if (element->is_slave())
          {
            gmshfilecontent << "ST(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontentslave << "ST(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                 << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                 << "," << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2)
                                 << "," << coord(2, 2) << ")";
            gmshfilecontentslave << "{" << std::scientific << color << "," << color << "," << color
                                 << "};" << std::endl;
          }
          else
          {
            gmshfilecontent << "ST(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontentmaster << "ST(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                  << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                  << "," << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2)
                                  << "," << coord(2, 2) << ")";
            gmshfilecontentmaster << "{" << std::scientific << color << "," << color << "," << color
                                  << "};" << std::endl;
          }
          xi[0] = 1.0 / 3;
          xi[1] = 1.0 / 3;
        }

        // 3D bilinear case (4noded quadrilateral elements)
        if (element->shape() == Core::FE::CellType::quad4)
        {
          if (element->is_slave())
          {
            gmshfilecontent << "SQ(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3) << ","
                            << coord(2, 3) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "," << color << "};" << std::endl;
            gmshfilecontentslave << "SQ(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                 << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                 << "," << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2)
                                 << "," << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3)
                                 << "," << coord(2, 3) << ")";
            gmshfilecontentslave << "{" << std::scientific << color << "," << color << "," << color
                                 << "," << color << "};" << std::endl;
          }
          else
          {
            gmshfilecontent << "SQ(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3) << ","
                            << coord(2, 3) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "," << color << "};" << std::endl;
            gmshfilecontentmaster << "SQ(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                  << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                  << "," << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2)
                                  << "," << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3)
                                  << "," << coord(2, 3) << ")";
            gmshfilecontentmaster << "{" << std::scientific << color << "," << color << "," << color
                                  << "," << color << "};" << std::endl;
          }
        }

        // 3D quadratic case (6noded triangular elements)
        if (element->shape() == Core::FE::CellType::tri6)
        {
          if (element->is_slave())
          {
            gmshfilecontent << "ST2(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3) << ","
                            << coord(2, 3) << "," << coord(0, 4) << "," << coord(1, 4) << ","
                            << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5) << ","
                            << coord(2, 5) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "," << color << "," << color << "," << color << "};" << std::endl;
            gmshfilecontentslave << "ST2(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                 << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                 << "," << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2)
                                 << "," << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3)
                                 << "," << coord(2, 3) << "," << coord(0, 4) << "," << coord(1, 4)
                                 << "," << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5)
                                 << "," << coord(2, 5) << ")";
            gmshfilecontentslave << "{" << std::scientific << color << "," << color << "," << color
                                 << "," << color << "," << color << "," << color << "};"
                                 << std::endl;
          }
          else
          {
            gmshfilecontent << "ST2(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3) << ","
                            << coord(2, 3) << "," << coord(0, 4) << "," << coord(1, 4) << ","
                            << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5) << ","
                            << coord(2, 5) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "," << color << "," << color << "," << color << "};" << std::endl;
            gmshfilecontentmaster << "ST2(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                  << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                  << "," << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2)
                                  << "," << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3)
                                  << "," << coord(2, 3) << "," << coord(0, 4) << "," << coord(1, 4)
                                  << "," << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5)
                                  << "," << coord(2, 5) << ")";
            gmshfilecontentmaster << "{" << std::scientific << color << "," << color << "," << color
                                  << "," << color << "," << color << "," << color << "};"
                                  << std::endl;
          }
          xi[0] = 1.0 / 3;
          xi[1] = 1.0 / 3;
        }

        // 3D serendipity case (8noded quadrilateral elements)
        if (element->shape() == Core::FE::CellType::quad8)
        {
          if (element->is_slave())
          {
            gmshfilecontent << "ST(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 4) << "," << coord(1, 4) << ","
                            << coord(2, 4) << "," << coord(0, 7) << "," << coord(1, 7) << ","
                            << coord(2, 7) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontent << "ST(" << std::scientific << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 5) << "," << coord(1, 5) << ","
                            << coord(2, 5) << "," << coord(0, 4) << "," << coord(1, 4) << ","
                            << coord(2, 4) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontent << "ST(" << std::scientific << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << "," << coord(0, 6) << "," << coord(1, 6) << ","
                            << coord(2, 6) << "," << coord(0, 5) << "," << coord(1, 5) << ","
                            << coord(2, 5) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontent << "ST(" << std::scientific << coord(0, 3) << "," << coord(1, 3) << ","
                            << coord(2, 3) << "," << coord(0, 7) << "," << coord(1, 7) << ","
                            << coord(2, 7) << "," << coord(0, 6) << "," << coord(1, 6) << ","
                            << coord(2, 6) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontent << "SQ(" << std::scientific << coord(0, 4) << "," << coord(1, 4) << ","
                            << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5) << ","
                            << coord(2, 5) << "," << coord(0, 6) << "," << coord(1, 6) << ","
                            << coord(2, 6) << "," << coord(0, 7) << "," << coord(1, 7) << ","
                            << coord(2, 7) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "," << color << "};" << std::endl;
            gmshfilecontentslave << "ST(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                 << "," << coord(2, 0) << "," << coord(0, 4) << "," << coord(1, 4)
                                 << "," << coord(2, 4) << "," << coord(0, 7) << "," << coord(1, 7)
                                 << "," << coord(2, 7) << ")";
            gmshfilecontentslave << "{" << std::scientific << color << "," << color << "," << color
                                 << "};" << std::endl;
            gmshfilecontentslave << "ST(" << std::scientific << coord(0, 1) << "," << coord(1, 1)
                                 << "," << coord(2, 1) << "," << coord(0, 5) << "," << coord(1, 5)
                                 << "," << coord(2, 5) << "," << coord(0, 4) << "," << coord(1, 4)
                                 << "," << coord(2, 4) << ")";
            gmshfilecontentslave << "{" << std::scientific << color << "," << color << "," << color
                                 << "};" << std::endl;
            gmshfilecontentslave << "ST(" << std::scientific << coord(0, 2) << "," << coord(1, 2)
                                 << "," << coord(2, 2) << "," << coord(0, 6) << "," << coord(1, 6)
                                 << "," << coord(2, 6) << "," << coord(0, 5) << "," << coord(1, 5)
                                 << "," << coord(2, 5) << ")";
            gmshfilecontentslave << "{" << std::scientific << color << "," << color << "," << color
                                 << "};" << std::endl;
            gmshfilecontentslave << "ST(" << std::scientific << coord(0, 3) << "," << coord(1, 3)
                                 << "," << coord(2, 3) << "," << coord(0, 7) << "," << coord(1, 7)
                                 << "," << coord(2, 7) << "," << coord(0, 6) << "," << coord(1, 6)
                                 << "," << coord(2, 6) << ")";
            gmshfilecontentslave << "{" << std::scientific << color << "," << color << "," << color
                                 << "};" << std::endl;
            gmshfilecontentslave << "SQ(" << std::scientific << coord(0, 4) << "," << coord(1, 4)
                                 << "," << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5)
                                 << "," << coord(2, 5) << "," << coord(0, 6) << "," << coord(1, 6)
                                 << "," << coord(2, 6) << "," << coord(0, 7) << "," << coord(1, 7)
                                 << "," << coord(2, 7) << ")";
            gmshfilecontentslave << "{" << std::scientific << color << "," << color << "," << color
                                 << "," << color << "};" << std::endl;
          }
          else
          {
            gmshfilecontent << "ST(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 4) << "," << coord(1, 4) << ","
                            << coord(2, 4) << "," << coord(0, 7) << "," << coord(1, 7) << ","
                            << coord(2, 7) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontent << "ST(" << std::scientific << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 5) << "," << coord(1, 5) << ","
                            << coord(2, 5) << "," << coord(0, 4) << "," << coord(1, 4) << ","
                            << coord(2, 4) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontent << "ST(" << std::scientific << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << "," << coord(0, 6) << "," << coord(1, 6) << ","
                            << coord(2, 6) << "," << coord(0, 5) << "," << coord(1, 5) << ","
                            << coord(2, 5) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontent << "ST(" << std::scientific << coord(0, 3) << "," << coord(1, 3) << ","
                            << coord(2, 3) << "," << coord(0, 7) << "," << coord(1, 7) << ","
                            << coord(2, 7) << "," << coord(0, 6) << "," << coord(1, 6) << ","
                            << coord(2, 6) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "};" << std::endl;
            gmshfilecontent << "SQ(" << std::scientific << coord(0, 4) << "," << coord(1, 4) << ","
                            << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5) << ","
                            << coord(2, 5) << "," << coord(0, 6) << "," << coord(1, 6) << ","
                            << coord(2, 6) << "," << coord(0, 7) << "," << coord(1, 7) << ","
                            << coord(2, 7) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "," << color << "};" << std::endl;
            gmshfilecontentmaster << "ST(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                  << "," << coord(2, 0) << "," << coord(0, 4) << "," << coord(1, 4)
                                  << "," << coord(2, 4) << "," << coord(0, 7) << "," << coord(1, 7)
                                  << "," << coord(2, 7) << ")";
            gmshfilecontentmaster << "{" << std::scientific << color << "," << color << "," << color
                                  << "};" << std::endl;
            gmshfilecontentmaster << "ST(" << std::scientific << coord(0, 1) << "," << coord(1, 1)
                                  << "," << coord(2, 1) << "," << coord(0, 5) << "," << coord(1, 5)
                                  << "," << coord(2, 5) << "," << coord(0, 4) << "," << coord(1, 4)
                                  << "," << coord(2, 4) << ")";
            gmshfilecontentmaster << "{" << std::scientific << color << "," << color << "," << color
                                  << "};" << std::endl;
            gmshfilecontentmaster << "ST(" << std::scientific << coord(0, 2) << "," << coord(1, 2)
                                  << "," << coord(2, 2) << "," << coord(0, 6) << "," << coord(1, 6)
                                  << "," << coord(2, 6) << "," << coord(0, 5) << "," << coord(1, 5)
                                  << "," << coord(2, 5) << ")";
            gmshfilecontentmaster << "{" << std::scientific << color << "," << color << "," << color
                                  << "};" << std::endl;
            gmshfilecontentmaster << "ST(" << std::scientific << coord(0, 3) << "," << coord(1, 3)
                                  << "," << coord(2, 3) << "," << coord(0, 7) << "," << coord(1, 7)
                                  << "," << coord(2, 7) << "," << coord(0, 6) << "," << coord(1, 6)
                                  << "," << coord(2, 6) << ")";
            gmshfilecontentmaster << "{" << std::scientific << color << "," << color << "," << color
                                  << "};" << std::endl;
            gmshfilecontentmaster << "SQ(" << std::scientific << coord(0, 4) << "," << coord(1, 4)
                                  << "," << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5)
                                  << "," << coord(2, 5) << "," << coord(0, 6) << "," << coord(1, 6)
                                  << "," << coord(2, 6) << "," << coord(0, 7) << "," << coord(1, 7)
                                  << "," << coord(2, 7) << ")";
            gmshfilecontentmaster << "{" << std::scientific << color << "," << color << "," << color
                                  << "," << color << "};" << std::endl;
          }
        }

        // 3D biquadratic case (9noded quadrilateral elements)
        if (element->shape() == Core::FE::CellType::quad9)
        {
          if (element->is_slave())
          {
            gmshfilecontent << "SQ2(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3) << ","
                            << coord(2, 3) << "," << coord(0, 4) << "," << coord(1, 4) << ","
                            << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5) << ","
                            << coord(2, 5) << "," << coord(0, 6) << "," << coord(1, 6) << ","
                            << coord(2, 6) << "," << coord(0, 7) << "," << coord(1, 7) << ","
                            << coord(2, 7) << "," << coord(0, 8) << "," << coord(1, 8) << ","
                            << coord(2, 8) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "," << color << "," << color << "," << color << "," << color << ","
                            << color << "," << color << "};" << std::endl;
            gmshfilecontentslave << "SQ2(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                 << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                 << "," << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2)
                                 << "," << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3)
                                 << "," << coord(2, 3) << "," << coord(0, 4) << "," << coord(1, 4)
                                 << "," << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5)
                                 << "," << coord(2, 5) << "," << coord(0, 6) << "," << coord(1, 6)
                                 << "," << coord(2, 6) << "," << coord(0, 7) << "," << coord(1, 7)
                                 << "," << coord(2, 7) << "," << coord(0, 8) << "," << coord(1, 8)
                                 << "," << coord(2, 8) << ")";
            gmshfilecontentslave << "{" << std::scientific << color << "," << color << "," << color
                                 << "," << color << "," << color << "," << color << "," << color
                                 << "," << color << "," << color << "};" << std::endl;
          }
          else
          {
            gmshfilecontent << "SQ2(" << std::scientific << coord(0, 0) << "," << coord(1, 0) << ","
                            << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1) << ","
                            << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2) << ","
                            << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3) << ","
                            << coord(2, 3) << "," << coord(0, 4) << "," << coord(1, 4) << ","
                            << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5) << ","
                            << coord(2, 5) << "," << coord(0, 6) << "," << coord(1, 6) << ","
                            << coord(2, 6) << "," << coord(0, 7) << "," << coord(1, 7) << ","
                            << coord(2, 7) << "," << coord(0, 8) << "," << coord(1, 8) << ","
                            << coord(2, 8) << ")";
            gmshfilecontent << "{" << std::scientific << color << "," << color << "," << color
                            << "," << color << "," << color << "," << color << "," << color << ","
                            << color << "," << color << "};" << std::endl;
            gmshfilecontentmaster << "SQ2(" << std::scientific << coord(0, 0) << "," << coord(1, 0)
                                  << "," << coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)
                                  << "," << coord(2, 1) << "," << coord(0, 2) << "," << coord(1, 2)
                                  << "," << coord(2, 2) << "," << coord(0, 3) << "," << coord(1, 3)
                                  << "," << coord(2, 3) << "," << coord(0, 4) << "," << coord(1, 4)
                                  << "," << coord(2, 4) << "," << coord(0, 5) << "," << coord(1, 5)
                                  << "," << coord(2, 5) << "," << coord(0, 6) << "," << coord(1, 6)
                                  << "," << coord(2, 6) << "," << coord(0, 7) << "," << coord(1, 7)
                                  << "," << coord(2, 7) << "," << coord(0, 8) << "," << coord(1, 8)
                                  << "," << coord(2, 8) << ")";
            gmshfilecontentmaster << "{" << std::scientific << color << "," << color << "," << color
                                  << "," << color << "," << color << "," << color << "," << color
                                  << "," << color << "," << color << "};" << std::endl;
          }
        }

        // plot element number in element center
        double elec[3];
        element->local_to_global(xi, elec, 0);

        if (element->is_slave())
        {
          gmshfilecontent << "T3(" << std::scientific << elec[0] << "," << elec[1] << "," << elec[2]
                          << "," << 17 << ")";
          gmshfilecontent << "{\""
                          << "S" << element->id() << "\"};" << std::endl;
          gmshfilecontentslave << "T3(" << std::scientific << elec[0] << "," << elec[1] << ","
                               << elec[2] << "," << 17 << ")";
          gmshfilecontentslave << "{\""
                               << "S" << element->id() << "\"};" << std::endl;
        }
        else
        {
          gmshfilecontent << "T3(" << std::scientific << elec[0] << "," << elec[1] << "," << elec[2]
                          << "," << 17 << ")";
          gmshfilecontent << "{\""
                          << "M" << element->id() << "\"};" << std::endl;
          gmshfilecontentmaster << "T3(" << std::scientific << elec[0] << "," << elec[1] << ","
                                << elec[2] << "," << 17 << ")";
          gmshfilecontentmaster << "{\""
                                << "M" << element->id() << "\"};" << std::endl;
        }

        // plot node numbers at the nodes
        for (int j = 0; j < nnodes; ++j)
        {
          if (element->is_slave())
          {
            gmshfilecontent << "T3(" << std::scientific << coord(0, j) << "," << coord(1, j) << ","
                            << coord(2, j) << "," << 17 << ")";
            gmshfilecontent << "{\""
                            << "SN" << element->node_ids()[j] << "\"};" << std::endl;
            gmshfilecontentslave << "T3(" << std::scientific << coord(0, j) << "," << coord(1, j)
                                 << "," << coord(2, j) << "," << 17 << ")";
            gmshfilecontentslave << "{\""
                                 << "SN" << element->node_ids()[j] << "\"};" << std::endl;
          }
          else
          {
            gmshfilecontent << "T3(" << std::scientific << coord(0, j) << "," << coord(1, j) << ","
                            << coord(2, j) << "," << 17 << ")";
            gmshfilecontent << "{\""
                            << "MN" << element->node_ids()[j] << "\"};" << std::endl;
            gmshfilecontentmaster << "T3(" << std::scientific << coord(0, j) << "," << coord(1, j)
                                  << "," << coord(2, j) << "," << 17 << ")";
            gmshfilecontentmaster << "{\""
                                  << "MN" << element->node_ids()[j] << "\"};" << std::endl;
          }
        }
      }

      //******************************************************************
      // plot normal vector, tangent vectors and contact status
      //******************************************************************
      for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
      {
        int gid = snoderowmap_->GID(i);
        Core::Nodes::Node* node = idiscret_->g_node(gid);
        if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
        Node* cnode = dynamic_cast<Node*>(node);
        if (!cnode) FOUR_C_THROW("Static Cast to Node* failed");

        double nc[3];
        double nn[3];
        double nt1[3];
        double nt2[3];

        for (int j = 0; j < 3; ++j)
        {
          nc[j] = cnode->xspatial()[j];
          nn[j] = cnode->mo_data().n()[j];
          nt1[j] = cnode->data().txi()[j];
          nt2[j] = cnode->data().teta()[j];
        }

        //******************************************************************
        // plot normal and tangent vectors
        //******************************************************************
        gmshfilecontentslave << "VP(" << std::scientific << nc[0] << "," << nc[1] << "," << nc[2]
                             << ")";
        gmshfilecontentslave << "{" << std::scientific << nn[0] << "," << nn[1] << "," << nn[2]
                             << "};" << std::endl;

        if (friction_)
        {
          gmshfilecontentslave << "VP(" << std::scientific << nc[0] << "," << nc[1] << "," << nc[2]
                               << ")";
          gmshfilecontentslave << "{" << std::scientific << nt1[0] << "," << nt1[1] << "," << nt1[2]
                               << "};" << std::endl;
          gmshfilecontentslave << "VP(" << std::scientific << nc[0] << "," << nc[1] << "," << nc[2]
                               << ")";
          gmshfilecontentslave << "{" << std::scientific << nt2[0] << "," << nt2[1] << "," << nt2[2]
                               << "};" << std::endl;
        }

        //******************************************************************
        // plot contact status of slave nodes (inactive, active, stick, slip)
        //******************************************************************
        // frictionless contact, active node = {A}
        if (!friction_ && cnode->active())
        {
          gmshfilecontentslave << "T3(" << std::scientific << nc[0] << "," << nc[1] << "," << nc[2]
                               << "," << 17 << ")";
          gmshfilecontentslave << "{\""
                               << "A"
                               << "\"};" << std::endl;
        }

        // frictionless contact, inactive node = { }
        else if (!friction_ && !cnode->active())
        {
          // do nothing
        }

        // frictional contact, slip node = {G}
        else if (friction_ && cnode->active())
        {
          if (dynamic_cast<FriNode*>(cnode)->fri_data().slip())
          {
            gmshfilecontentslave << "T3(" << std::scientific << nc[0] << "," << nc[1] << ","
                                 << nc[2] << "," << 17 << ")";
            gmshfilecontentslave << "{\""
                                 << "G"
                                 << "\"};" << std::endl;
          }
          else
          {
            gmshfilecontentslave << "T3(" << std::scientific << nc[0] << "," << nc[1] << ","
                                 << nc[2] << "," << 17 << ")";
            gmshfilecontentslave << "{\""
                                 << "H"
                                 << "\"};" << std::endl;
          }
        }
      }

      // end GMSH output section in all files
      if (proc == Core::Communication::num_mpi_ranks(get_comm()) - 1)
      {
        gmshfilecontent << "};" << std::endl;
        gmshfilecontentslave << "};" << std::endl;
        gmshfilecontentmaster << "};" << std::endl;
      }

      // move everything to gmsh post-processing files and close them
      fputs(gmshfilecontent.str().c_str(), fp);
      fputs(gmshfilecontentslave.str().c_str(), fps);
      fputs(gmshfilecontentmaster.str().c_str(), fpm);
      fclose(fp);
      fclose(fps);
      fclose(fpm);
    }
    Core::Communication::barrier(get_comm());
  }


  //**********************************************************************
  // GMSH output of all treenodes (DOPs) on all layers
  //**********************************************************************
#ifdef MORTARGMSHTN
  // get max. number of layers for every proc.
  // (master elements are equal on each proc)
  int lnslayers = binarytree_->Streenodesmap().size();
  int gnmlayers = binarytree_->Mtreenodesmap().size();
  int gnslayers = 0;
  Core::Communication::max_all(&lnslayers, &gnslayers, 1, Comm());

  // create files for visualization of slave dops for every layer
  std::ostringstream filenametn;
  filenametn << "o/gmsh_output/" << file_name_only_prefix << "_";

  if (step < 10)
    filenametn << 0 << 0 << 0 << 0;
  else if (step < 100)
    filenametn << 0 << 0 << 0;
  else if (step < 1000)
    filenametn << 0 << 0;
  else if (step < 10000)
    filenametn << 0;
  else if (step > 99999)
    FOUR_C_THROW("Gmsh output implemented for a maximum of 99.999 time steps");
  filenametn << step;

  // construct unique filename for gmsh output
  // second index = Newton iteration index
  if (iter >= 0)
  {
    filenametn << "_";
    if (iter < 10)
      filenametn << 0;
    else if (iter > 99)
      FOUR_C_THROW("Gmsh output implemented for a maximum of 99 iterations");
    filenametn << iter;
  }

  if (Core::Communication::my_mpi_rank(Comm()) == 0)
  {
    for (int i = 0; i < gnslayers; i++)
    {
      std::ostringstream currentfilename;
      currentfilename << filenametn.str().c_str() << "_s_tnlayer_" << i << ".pos";
      // std::cout << std::endl << Core::Communication::my_mpi_rank(Comm())<< "filename: " <<
      // currentfilename.str().c_str();
      fp = fopen(currentfilename.str().c_str(), "w");
      std::ostringstream gmshfile;
      gmshfile << "View \" Step " << step << " Iter " << iter << " stl " << i << " \" {"
               << std::endl;
      fprintf(fp, gmshfile.str().c_str());
      fclose(fp);
    }
  }

  Core::Communication::barrier(Comm());

  // for every proc, one after another, put data of slabs into files
  for (int i = 0; i < Core::Communication::num_mpi_ranks(Comm()); i++)
  {
    if ((i == Core::Communication::my_mpi_rank(Comm())) && (binarytree_->Sroot()->Type() != 4))
    {
      // print full tree with treenodesmap
      for (int j = 0; j < (int)binarytree_->Streenodesmap().size(); j++)
      {
        for (int k = 0; k < (int)binarytree_->Streenodesmap()[j].size(); k++)
        {
          // if proc !=0 and first treenode to plot->create new sheet in gmsh
          if (i != 0 && k == 0)
          {
            // create new sheet "Treenode" in gmsh
            std::ostringstream currentfilename;
            currentfilename << filenametn.str().c_str() << "_s_tnlayer_" << j << ".pos";
            fp = fopen(currentfilename.str().c_str(), "a");
            std::ostringstream gmshfile;
            gmshfile << "};" << std::endl << "View \" Treenode \" { " << std::endl;
            fprintf(fp, gmshfile.str().c_str());
            fclose(fp);
          }
          // std::cout << std::endl << "plot streenode level: " << j << "treenode: " << k;
          std::ostringstream currentfilename;
          currentfilename << filenametn.str().c_str() << "_s_tnlayer_" << j << ".pos";
          binarytree_->Streenodesmap()[j][k]->PrintDopsForGmsh(currentfilename.str().c_str());

          // if there is another treenode to plot
          if (k < ((int)binarytree_->Streenodesmap()[j].size() - 1))
          {
            // create new sheet "Treenode" in gmsh
            std::ostringstream currentfilename;
            currentfilename << filenametn.str().c_str() << "_s_tnlayer_" << j << ".pos";
            fp = fopen(currentfilename.str().c_str(), "a");
            std::ostringstream gmshfile;
            gmshfile << "};" << std::endl << "View \" Treenode \" { " << std::endl;
            fprintf(fp, gmshfile.str().c_str());
            fclose(fp);
          }
        }
      }
    }

    Core::Communication::barrier(Comm());
  }

  Core::Communication::barrier(Comm());
  // close all slave-gmsh files
  if (Core::Communication::my_mpi_rank(Comm()) == 0)
  {
    for (int i = 0; i < gnslayers; i++)
    {
      std::ostringstream currentfilename;
      currentfilename << filenametn.str().c_str() << "_s_tnlayer_" << i << ".pos";
      // std::cout << std::endl << Core::Communication::my_mpi_rank(Comm())<< "current filename: "
      // << currentfilename.str().c_str();
      fp = fopen(currentfilename.str().c_str(), "a");
      std::ostringstream gmshfilecontent;
      gmshfilecontent << "};";
      fprintf(fp, gmshfilecontent.str().c_str());
      fclose(fp);
    }
  }
  Core::Communication::barrier(Comm());

  // create master slabs
  if (Core::Communication::my_mpi_rank(Comm()) == 0)
  {
    for (int i = 0; i < gnmlayers; i++)
    {
      std::ostringstream currentfilename;
      currentfilename << filenametn.str().c_str() << "_m_tnlayer_" << i << ".pos";
      // std::cout << std::endl << Core::Communication::my_mpi_rank(Comm())<< "filename: " <<
      // currentfilename.str().c_str();
      fp = fopen(currentfilename.str().c_str(), "w");
      std::ostringstream gmshfile;
      gmshfile << "View \" Step " << step << " Iter " << iter << " mtl " << i << " \" {"
               << std::endl;
      fprintf(fp, gmshfile.str().c_str());
      fclose(fp);
    }

    // print full tree with treenodesmap
    for (int j = 0; j < (int)binarytree_->Mtreenodesmap().size(); j++)
    {
      for (int k = 0; k < (int)binarytree_->Mtreenodesmap()[j].size(); k++)
      {
        std::ostringstream currentfilename;
        currentfilename << filenametn.str().c_str() << "_m_tnlayer_" << j << ".pos";
        binarytree_->Mtreenodesmap()[j][k]->PrintDopsForGmsh(currentfilename.str().c_str());

        // if there is another treenode to plot
        if (k < ((int)binarytree_->Mtreenodesmap()[j].size() - 1))
        {
          // create new sheet "Treenode" in gmsh
          std::ostringstream currentfilename;
          currentfilename << filenametn.str().c_str() << "_m_tnlayer_" << j << ".pos";
          fp = fopen(currentfilename.str().c_str(), "a");
          std::ostringstream gmshfile;
          gmshfile << "};" << std::endl << "View \" Treenode \" { " << std::endl;
          fprintf(fp, gmshfile.str().c_str());
          fclose(fp);
        }
      }
    }

    // close all master files
    for (int i = 0; i < gnmlayers; i++)
    {
      std::ostringstream currentfilename;
      currentfilename << filenametn.str().c_str() << "_m_tnlayer_" << i << ".pos";
      fp = fopen(currentfilename.str().c_str(), "a");
      std::ostringstream gmshfilecontent;
      gmshfilecontent << std::endl << "};";
      fprintf(fp, gmshfilecontent.str().c_str());
      fclose(fp);
    }
  }
#endif


  //**********************************************************************
  // GMSH output of all active treenodes (DOPs) on leaf level
  //**********************************************************************
#ifdef MORTARGMSHCTN
  std::ostringstream filenamectn;
  filenamectn << "o/gmsh_output/" << file_name_only_prefix << "_";
  if (step < 10)
    filenamectn << 0 << 0 << 0 << 0;
  else if (step < 100)
    filenamectn << 0 << 0 << 0;
  else if (step < 1000)
    filenamectn << 0 << 0;
  else if (step < 10000)
    filenamectn << 0;
  else if (step > 99999)
    FOUR_C_THROW("Gmsh output implemented for a maximum of 99.999 time steps");
  filenamectn << step;

  // construct unique filename for gmsh output
  // second index = Newton iteration index
  if (iter >= 0)
  {
    filenamectn << "_";
    if (iter < 10)
      filenamectn << 0;
    else if (iter > 99)
      FOUR_C_THROW("Gmsh output implemented for a maximum of 99 iterations");
    filenamectn << iter;
  }

  int lcontactmapsize = (int)(binarytree_->coupling_map()[0].size());
  int gcontactmapsize;

  Core::Communication::max_all(&lcontactmapsize, &gcontactmapsize, 1, Comm());

  if (gcontactmapsize > 0)
  {
    // open/create new file
    if (Core::Communication::my_mpi_rank(Comm()) == 0)
    {
      std::ostringstream currentfilename;
      currentfilename << filenamectn.str().c_str() << "_ct.pos";
      // std::cout << std::endl << Core::Communication::my_mpi_rank(Comm())<< "filename: " <<
      // currentfilename.str().c_str();
      fp = fopen(currentfilename.str().c_str(), "w");
      std::ostringstream gmshfile;
      gmshfile << "View \" Step " << step << " Iter " << iter << " contacttn  \" {" << std::endl;
      fprintf(fp, gmshfile.str().c_str());
      fclose(fp);
    }

    // every proc should plot its contacting treenodes!
    for (int i = 0; i < Core::Communication::num_mpi_ranks(Comm()); i++)
    {
      if (Core::Communication::my_mpi_rank(Comm()) == i)
      {
        if ((int)(binarytree_->coupling_map()[0]).size() !=
            (int)(binarytree_->coupling_map()[1]).size())
          FOUR_C_THROW("Binarytree coupling_map does not have right size!");

        for (int j = 0; j < (int)((binarytree_->coupling_map()[0]).size()); j++)
        {
          std::ostringstream currentfilename;
          std::ostringstream gmshfile;
          std::ostringstream newgmshfile;

          // create new sheet for slave
          if (Core::Communication::my_mpi_rank(Comm()) == 0 && j == 0)
          {
            currentfilename << filenamectn.str().c_str() << "_ct.pos";
            fp = fopen(currentfilename.str().c_str(), "w");
            gmshfile << "View \" Step " << step << " Iter " << iter << " CS  \" {" << std::endl;
            fprintf(fp, gmshfile.str().c_str());
            fclose(fp);
            (binarytree_->coupling_map()[0][j])->PrintDopsForGmsh(currentfilename.str().c_str());
          }
          else
          {
            currentfilename << filenamectn.str().c_str() << "_ct.pos";
            fp = fopen(currentfilename.str().c_str(), "a");
            gmshfile << "};" << std::endl
                     << "View \" Step " << step << " Iter " << iter << " CS  \" {" << std::endl;
            fprintf(fp, gmshfile.str().c_str());
            fclose(fp);
            (binarytree_->coupling_map()[0][j])->PrintDopsForGmsh(currentfilename.str().c_str());
          }

          // create new sheet for master
          fp = fopen(currentfilename.str().c_str(), "a");
          newgmshfile << "};" << std::endl
                      << "View \" Step " << step << " Iter " << iter << " CM  \" {" << std::endl;
          fprintf(fp, newgmshfile.str().c_str());
          fclose(fp);
          (binarytree_->coupling_map()[1][j])->PrintDopsForGmsh(currentfilename.str().c_str());
        }
      }
      Core::Communication::barrier(Comm());
    }

    // close file
    if (Core::Communication::my_mpi_rank(Comm()) == 0)
    {
      std::ostringstream currentfilename;
      currentfilename << filenamectn.str().c_str() << "_ct.pos";
      // std::cout << std::endl << Core::Communication::my_mpi_rank(Comm())<< "filename: " <<
      // currentfilename.str().c_str();
      fp = fopen(currentfilename.str().c_str(), "a");
      std::ostringstream gmshfile;
      gmshfile << "};";
      fprintf(fp, gmshfile.str().c_str());
      fclose(fp);
    }
  }
#endif  // MORTARGMSHCTN

  return;
}

/*----------------------------------------------------------------------*
 | Finite difference check for normal/tangent deriv.          popp 05/08|
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_normal_deriv()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for normals / tangents
  std::vector<double> refnx(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> refny(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> refnz(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newnx(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newny(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newnz(int(snodecolmapbound_->NumMyElements()));

  std::vector<double> reftxix(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> reftxiy(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> reftxiz(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtxix(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtxiy(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtxiz(int(snodecolmapbound_->NumMyElements()));

  std::vector<double> reftetax(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> reftetay(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> reftetaz(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtetax(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtetay(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtetaz(int(snodecolmapbound_->NumMyElements()));

  // problem dimension (2D or 3D)
  int dim = n_dim();

  // store all nodal normals / derivatives (reference)
  for (int j = 0; j < snodecolmapbound_->NumMyElements(); ++j)
  {
    int jgid = snodecolmapbound_->GID(j);
    Core::Nodes::Node* jnode = idiscret_->g_node(jgid);
    if (!jnode) FOUR_C_THROW("Cannot find node with gid %", jgid);
    Node* jcnode = dynamic_cast<Node*>(jnode);

    // store reference normals / tangents
    refnx[j] = jcnode->mo_data().n()[0];
    refny[j] = jcnode->mo_data().n()[1];
    refnz[j] = jcnode->mo_data().n()[2];
    reftxix[j] = jcnode->data().txi()[0];
    reftxiy[j] = jcnode->data().txi()[1];
    reftxiz[j] = jcnode->data().txi()[2];
    reftetax[j] = jcnode->data().teta()[0];
    reftetay[j] = jcnode->data().teta()[1];
    reftetaz[j] = jcnode->data().teta()[2];
  }

  // global loop to apply FD scheme to all slave dofs (=dim*nodes)
  for (int i = 0; i < dim * snodefullmap->NumMyElements(); ++i)
  {
    // store warnings for this finite difference
    int w = 0;

    // reset normal etc.
    initialize();

    // compute element areas
    set_element_areas();

    // now finally get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(i / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    int sdof = snode->dofs()[i % dim];
    std::cout << "\nDERIVATIVE FOR S-NODE # " << gid << " DOF: " << sdof << std::endl;


    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (i % dim == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (i % dim == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // compute finite difference derivative
    for (int k = 0; k < snodecolmapbound_->NumMyElements(); ++k)
    {
      int kgid = snodecolmapbound_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      // build NEW averaged normal at each slave node
      kcnode->build_averaged_normal();

      newnx[k] = kcnode->mo_data().n()[0];
      newny[k] = kcnode->mo_data().n()[1];
      newnz[k] = kcnode->mo_data().n()[2];
      newtxix[k] = kcnode->data().txi()[0];
      newtxiy[k] = kcnode->data().txi()[1];
      newtxiz[k] = kcnode->data().txi()[2];
      newtetax[k] = kcnode->data().teta()[0];
      newtetay[k] = kcnode->data().teta()[1];
      newtetaz[k] = kcnode->data().teta()[2];

      // get reference normal / tangent
      std::array<double, 3> refn = {0.0, 0.0, 0.0};
      std::array<double, 3> reftxi = {0.0, 0.0, 0.0};
      std::array<double, 3> refteta = {0.0, 0.0, 0.0};
      refn[0] = refnx[k];
      refn[1] = refny[k];
      refn[2] = refnz[k];
      reftxi[0] = reftxix[k];
      reftxi[1] = reftxiy[k];
      reftxi[2] = reftxiz[k];
      refteta[0] = reftetax[k];
      refteta[1] = reftetay[k];
      refteta[2] = reftetaz[k];

      // get modified normal / tangent
      std::array<double, 3> newn = {0.0, 0.0, 0.0};
      std::array<double, 3> newtxi = {0.0, 0.0, 0.0};
      std::array<double, 3> newteta = {0.0, 0.0, 0.0};
      newn[0] = newnx[k];
      newn[1] = newny[k];
      newn[2] = newnz[k];
      newtxi[0] = newtxix[k];
      newtxi[1] = newtxiy[k];
      newtxi[2] = newtxiz[k];
      newteta[0] = newtetax[k];
      newteta[1] = newtetay[k];
      newteta[2] = newtetaz[k];

      // print results (derivatives) to screen
      if (abs(newn[0] - refn[0]) > 1e-12 || abs(newn[1] - refn[1]) > 1e-12 ||
          abs(newn[2] - refn[2]) > 1e-12)
      {
        for (int d = 0; d < dim; ++d)
        {
          double finit = (newn[d] - refn[d]) / delta;
          double analy = (kcnode->data().get_deriv_n()[d])[snode->dofs()[i % dim]];
          double dev = finit - analy;

          // kgid: id of currently tested slave node
          // snode->Dofs()[fd%dim]: currently modified slave dof
          std::cout << "NORMAL(" << kgid << "," << d << "," << snode->dofs()[i % dim]
                    << ") : fd=" << finit << " derivn=" << analy << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }

      if (abs(newtxi[0] - reftxi[0]) > 1e-12 || abs(newtxi[1] - reftxi[1]) > 1e-12 ||
          abs(newtxi[2] - reftxi[2]) > 1e-12)
      {
        for (int d = 0; d < dim; ++d)
        {
          double finit = (newtxi[d] - reftxi[d]) / delta;
          double analy = (kcnode->data().get_deriv_txi()[d])[snode->dofs()[i % dim]];
          double dev = finit - analy;

          // kgid: id of currently tested slave node
          // snode->Dofs()[fd%dim]: currently modified slave dof
          std::cout << "TANGENT_XI(" << kgid << "," << d << "," << snode->dofs()[i % dim]
                    << ") : fd=" << finit << " derivn=" << analy << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }

      if (abs(newteta[0] - refteta[0]) > 1e-12 || abs(newteta[1] - refteta[1]) > 1e-12 ||
          abs(newteta[2] - refteta[2]) > 1e-12)
      {
        for (int d = 0; d < dim; ++d)
        {
          double finit = (newteta[d] - refteta[d]) / delta;
          double analy = (kcnode->data().get_deriv_teta()[d])[snode->dofs()[i % dim]];
          double dev = finit - analy;

          // kgid: id of currently tested slave node
          // snode->Dofs()[fd%dim]: currently modified slave dof
          std::cout << "TANGENT_ETA(" << kgid << "," << d << "," << snode->dofs()[i % dim]
                    << ") : fd=" << finit << " derivn=" << analy << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }
    }

    // undo finite difference modification
    if (i % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (i % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // back to normal...
  // reset normal etc.
  initialize();

  // compute element areas
  set_element_areas();

  // contents of evaluate()
  evaluate();

  return;
}


/*----------------------------------------------------------------------*
 | Finite difference check for normal/tangent deriv.         farah 01/16|
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_normal_cpp_deriv()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for normals / tangents
  std::vector<double> refnx(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> refny(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> refnz(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newnx(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newny(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newnz(int(snodecolmapbound_->NumMyElements()));

  std::vector<double> reftxix(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> reftxiy(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> reftxiz(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtxix(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtxiy(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtxiz(int(snodecolmapbound_->NumMyElements()));

  std::vector<double> reftetax(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> reftetay(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> reftetaz(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtetax(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtetay(int(snodecolmapbound_->NumMyElements()));
  std::vector<double> newtetaz(int(snodecolmapbound_->NumMyElements()));

  // problem dimension (2D or 3D)
  int dim = n_dim();

  // store all nodal normals / derivatives (reference)
  for (int j = 0; j < snodecolmapbound_->NumMyElements(); ++j)
  {
    int jgid = snodecolmapbound_->GID(j);
    Core::Nodes::Node* jnode = idiscret_->g_node(jgid);
    if (!jnode) FOUR_C_THROW("Cannot find node with gid %", jgid);
    Node* jcnode = dynamic_cast<Node*>(jnode);

    if (!jcnode->is_on_edge()) continue;

    // store reference normals / tangents
    refnx[j] = jcnode->mo_data().n()[0];
    refny[j] = jcnode->mo_data().n()[1];
    refnz[j] = jcnode->mo_data().n()[2];
    reftxix[j] = jcnode->data().txi()[0];
    reftxiy[j] = jcnode->data().txi()[1];
    reftxiz[j] = jcnode->data().txi()[2];
    reftetax[j] = jcnode->data().teta()[0];
    reftetay[j] = jcnode->data().teta()[1];
    reftetaz[j] = jcnode->data().teta()[2];
  }

  // global loop to apply FD scheme to all slave dofs (=dim*nodes)
  for (int i = 0; i < dim * snodefullmap->NumMyElements(); ++i)
  {
    // store warnings for this finite difference
    int w = 0;

    // reset normal etc.
    initialize();

    // compute element areas
    set_element_areas();

    evaluate_search_binarytree();

    // now finally get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(i / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    int sdof = snode->dofs()[i % dim];
    std::cout << "\nDERIVATIVE FOR S-NODE # " << gid << " DOF: " << sdof << std::endl;


    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (i % dim == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (i % dim == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    evaluate_cpp_normals();

    // compute finite difference derivative
    for (int k = 0; k < snodecolmapbound_->NumMyElements(); ++k)
    {
      int kgid = snodecolmapbound_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if (!kcnode->is_on_edge()) continue;

      // build NEW averaged normal at each slave node
      //      kcnode->BuildAveragedNormal();

      newnx[k] = kcnode->mo_data().n()[0];
      newny[k] = kcnode->mo_data().n()[1];
      newnz[k] = kcnode->mo_data().n()[2];
      newtxix[k] = kcnode->data().txi()[0];
      newtxiy[k] = kcnode->data().txi()[1];
      newtxiz[k] = kcnode->data().txi()[2];
      newtetax[k] = kcnode->data().teta()[0];
      newtetay[k] = kcnode->data().teta()[1];
      newtetaz[k] = kcnode->data().teta()[2];

      // get reference normal / tangent
      std::array<double, 3> refn = {0.0, 0.0, 0.0};
      std::array<double, 3> reftxi = {0.0, 0.0, 0.0};
      std::array<double, 3> refteta = {0.0, 0.0, 0.0};
      refn[0] = refnx[k];
      refn[1] = refny[k];
      refn[2] = refnz[k];
      reftxi[0] = reftxix[k];
      reftxi[1] = reftxiy[k];
      reftxi[2] = reftxiz[k];
      refteta[0] = reftetax[k];
      refteta[1] = reftetay[k];
      refteta[2] = reftetaz[k];

      // get modified normal / tangent
      std::array<double, 3> newn = {0.0, 0.0, 0.0};
      std::array<double, 3> newtxi = {0.0, 0.0, 0.0};
      std::array<double, 3> newteta = {0.0, 0.0, 0.0};
      newn[0] = newnx[k];
      newn[1] = newny[k];
      newn[2] = newnz[k];
      newtxi[0] = newtxix[k];
      newtxi[1] = newtxiy[k];
      newtxi[2] = newtxiz[k];
      newteta[0] = newtetax[k];
      newteta[1] = newtetay[k];
      newteta[2] = newtetaz[k];

      // print results (derivatives) to screen
      if (abs(newn[0] - refn[0]) > 1e-12 || abs(newn[1] - refn[1]) > 1e-12 ||
          abs(newn[2] - refn[2]) > 1e-12)
      {
        for (int d = 0; d < dim; ++d)
        {
          double finit = (newn[d] - refn[d]) / delta;
          double analy = (kcnode->data().get_deriv_n()[d])[snode->dofs()[i % dim]];
          double dev = finit - analy;

          if (abs(finit) < 1e-12) continue;
          // kgid: id of currently tested slave node
          // snode->Dofs()[fd%dim]: currently modified slave dof

          if (abs(analy) > 1e-12)
            std::cout << "NORMAL(" << kgid << "," << d << "," << snode->dofs()[i % dim]
                      << ") : fd=" << finit << " derivn=" << analy << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }

      if (abs(newtxi[0] - reftxi[0]) > 1e-12 || abs(newtxi[1] - reftxi[1]) > 1e-12 ||
          abs(newtxi[2] - reftxi[2]) > 1e-12)
      {
        for (int d = 0; d < dim; ++d)
        {
          double finit = (newtxi[d] - reftxi[d]) / delta;
          double analy = (kcnode->data().get_deriv_txi()[d])[snode->dofs()[i % dim]];
          double dev = finit - analy;

          if (abs(finit) < 1e-12) continue;

          // kgid: id of currently tested slave node
          // snode->Dofs()[fd%dim]: currently modified slave dof
          std::cout << "TANGENT_XI(" << kgid << "," << d << "," << snode->dofs()[i % dim]
                    << ") : fd=" << finit << " derivn=" << analy << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }

      // if (abs(newteta[0]-refteta[0])>1e-12 || abs(newteta[1]-refteta[1])>1e-12 ||
      // abs(newteta[2]-refteta[2]) > 1e-12)
      {
        for (int d = 0; d < dim; ++d)
        {
          double finit = (newteta[d] - refteta[d]) / delta;
          double analy = (kcnode->data().get_deriv_teta()[d])[snode->dofs()[i % dim]];
          double dev = finit - analy;

          if (abs(finit) < 1e-12) continue;

          // kgid: id of currently tested slave node
          // snode->Dofs()[fd%dim]: currently modified slave dof
          std::cout << "TANGENT_ETA(" << kgid << "," << d << "," << snode->dofs()[i % dim]
                    << ") : fd=" << finit << " derivn=" << analy << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }
    }

    // undo finite difference modification
    if (i % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (i % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }



  // global loop to apply FD scheme to all slave dofs (=dim*nodes)
  for (int i = 0; i < dim * mnodefullmap->NumMyElements(); ++i)
  {
    // store warnings for this finite difference
    int w = 0;

    // reset normal etc.
    initialize();

    // compute element areas
    set_element_areas();

    evaluate_search_binarytree();

    // now finally get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(i / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    int mdof = mnode->dofs()[i % dim];
    std::cout << "\nDERIVATIVE FOR M-NODE # " << gid << " DOF: " << mdof << std::endl;


    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (i % dim == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (i % dim == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    evaluate_cpp_normals();

    // compute finite difference derivative
    for (int k = 0; k < snodecolmapbound_->NumMyElements(); ++k)
    {
      int kgid = snodecolmapbound_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if (!kcnode->is_on_edge()) continue;

      // build NEW averaged normal at each slave node
      //      kcnode->BuildAveragedNormal();

      newnx[k] = kcnode->mo_data().n()[0];
      newny[k] = kcnode->mo_data().n()[1];
      newnz[k] = kcnode->mo_data().n()[2];
      newtxix[k] = kcnode->data().txi()[0];
      newtxiy[k] = kcnode->data().txi()[1];
      newtxiz[k] = kcnode->data().txi()[2];
      newtetax[k] = kcnode->data().teta()[0];
      newtetay[k] = kcnode->data().teta()[1];
      newtetaz[k] = kcnode->data().teta()[2];

      // get reference normal / tangent
      std::array<double, 3> refn = {0.0, 0.0, 0.0};
      std::array<double, 3> reftxi = {0.0, 0.0, 0.0};
      std::array<double, 3> refteta = {0.0, 0.0, 0.0};
      refn[0] = refnx[k];
      refn[1] = refny[k];
      refn[2] = refnz[k];
      reftxi[0] = reftxix[k];
      reftxi[1] = reftxiy[k];
      reftxi[2] = reftxiz[k];
      refteta[0] = reftetax[k];
      refteta[1] = reftetay[k];
      refteta[2] = reftetaz[k];

      // get modified normal / tangent
      std::array<double, 3> newn = {0.0, 0.0, 0.0};
      std::array<double, 3> newtxi = {0.0, 0.0, 0.0};
      std::array<double, 3> newteta = {0.0, 0.0, 0.0};
      newn[0] = newnx[k];
      newn[1] = newny[k];
      newn[2] = newnz[k];
      newtxi[0] = newtxix[k];
      newtxi[1] = newtxiy[k];
      newtxi[2] = newtxiz[k];
      newteta[0] = newtetax[k];
      newteta[1] = newtetay[k];
      newteta[2] = newtetaz[k];

      // print results (derivatives) to screen
      if (abs(newn[0] - refn[0]) > 1e-12 || abs(newn[1] - refn[1]) > 1e-12 ||
          abs(newn[2] - refn[2]) > 1e-12)
      {
        for (int d = 0; d < dim; ++d)
        {
          double finit = (newn[d] - refn[d]) / delta;
          double analy = (kcnode->data().get_deriv_n()[d])[mnode->dofs()[i % dim]];
          double dev = finit - analy;

          if (abs(finit) < 1e-12) continue;
          // kgid: id of currently tested slave node
          // snode->Dofs()[fd%dim]: currently modified slave dof

          if (abs(analy) > 1e-12)
            std::cout << "NORMAL(" << kgid << "," << d << "," << mnode->dofs()[i % dim]
                      << ") : fd=" << finit << " derivn=" << analy << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }

      if (abs(newtxi[0] - reftxi[0]) > 1e-12 || abs(newtxi[1] - reftxi[1]) > 1e-12 ||
          abs(newtxi[2] - reftxi[2]) > 1e-12)
      {
        for (int d = 0; d < dim; ++d)
        {
          double finit = (newtxi[d] - reftxi[d]) / delta;
          double analy = (kcnode->data().get_deriv_txi()[d])[mnode->dofs()[i % dim]];
          double dev = finit - analy;

          if (abs(finit) < 1e-12) continue;
          // kgid: id of currently tested slave node
          // snode->Dofs()[fd%dim]: currently modified slave dof
          std::cout << "TANGENT_XI(" << kgid << "," << d << "," << mnode->dofs()[i % dim]
                    << ") : fd=" << finit << " derivn=" << analy << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }

      // if (abs(newteta[0]-refteta[0])>1e-12 || abs(newteta[1]-refteta[1])>1e-12 ||
      // abs(newteta[2]-refteta[2]) > 1e-12)
      {
        for (int d = 0; d < dim; ++d)
        {
          double finit = (newteta[d] - refteta[d]) / delta;
          double analy = (kcnode->data().get_deriv_teta()[d])[mnode->dofs()[i % dim]];
          double dev = finit - analy;

          if (abs(finit) < 1e-12) continue;
          // kgid: id of currently tested slave node
          // snode->Dofs()[fd%dim]: currently modified slave dof
          std::cout << "TANGENT_ETA(" << kgid << "," << d << "," << mnode->dofs()[i % dim]
                    << ") : fd=" << finit << " derivn=" << analy << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }
    }

    // undo finite difference modification
    if (i % dim == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (i % dim == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // back to normal...
  // reset normal etc.
  initialize();

  // compute element areas
  set_element_areas();

  // contents of evaluate()
  evaluate();

  return;
}


/*----------------------------------------------------------------------*
 | Finite difference check for D-Mortar derivatives           popp 05/08|
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_mortar_d_deriv()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for D-Matrix entries
  std::map<int, double> refD;  // stores dof-wise the entries of D
  std::map<int, double> newD;

  std::map<int, std::map<int, std::map<int, double>>>
      refDerivD;  // stores old derivm for every node

  // problem dimension (2D or 3D)
  int dim = n_dim();

  // print reference to screen (D-derivative-maps) and store them for later comparison
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    Node* cnode = dynamic_cast<Node*>(node);

    typedef Core::Gen::Pairedvector<int, double>::const_iterator _CI;

    if ((int)(cnode->mo_data().get_d().size()) == 0) continue;

    for (_CI it = cnode->mo_data().get_d().begin(); it != cnode->mo_data().get_d().end(); ++it)
      refD[it->first] = it->second;


    refDerivD[gid] = cnode->data().get_deriv_d();
  }

  // global loop to apply FD scheme to all SLAVE dofs (=dim*nodes)
  for (int fd = 0; fd < dim * snodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    int sdof = snode->dofs()[fd % dim];

    std::cout << "\nDERIVATIVE FOR S-NODE # " << gid << " DOF: " << sdof << std::endl;

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if ((int)(kcnode->mo_data().get_d().size()) == 0) continue;

      typedef std::map<int, double>::const_iterator CI;
      typedef Core::Gen::Pairedvector<int, double>::const_iterator _CI;

      for (_CI it = kcnode->mo_data().get_d().begin(); it != kcnode->mo_data().get_d().end(); ++it)
        newD[it->first] = it->second;

      // print results (derivatives) to screen
      for (CI p = newD.begin(); p != newD.end(); ++p)
      {
        if (abs(newD[p->first] - refD[p->first]) > 1e-12)
        {
          double finit = (newD[p->first] - refD[p->first]) / delta;
          double analy = ((refDerivD[kgid])[p->first])[sdof];
          double dev = finit - analy;

          // kgid: currently tested dof of slave node kgid
          // (p->first)/Dim(): paired master
          // sdof: currently modified slave dof
          std::cout << "(" << (p->first) << "," << sdof << ") : fd=" << finit << " derivd=" << analy
                    << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // global loop to apply FD scheme to all MASTER dofs (=dim*nodes)
  for (int fd = 0; fd < dim * mnodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    int mdof = mnode->dofs()[fd % dim];

    std::cout << "\nDERIVATIVE FOR M-NODE # " << gid << " DOF: " << mdof << std::endl;

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if ((int)(kcnode->mo_data().get_d().size()) == 0) continue;

      typedef std::map<int, double>::const_iterator CI;
      typedef Core::Gen::Pairedvector<int, double>::const_iterator _CI;

      for (_CI it = kcnode->mo_data().get_d().begin(); it != kcnode->mo_data().get_d().end(); ++it)
        newD[it->first] = it->second;

      // print results (derivatives) to screen
      for (CI p = newD.begin(); p != newD.end(); ++p)
      {
        if (abs(newD[p->first] - refD[p->first]) > 1e-12)
        {
          double finit = (newD[p->first] - refD[p->first]) / delta;
          double analy = ((refDerivD[kgid])[p->first])[mdof];
          double dev = finit - analy;

          // kgid: currently tested dof of slave node kgid
          // (p->first)/Dim(): paired master
          // sdof: currently modified slave dof
          std::cout << "(" << (p->first) << "," << mdof << ") : fd=" << finit << " derivd=" << analy
                    << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // back to normal...

  // Initialize
  initialize();

  // compute element areas
  set_element_areas();

  // *******************************************************************
  // contents of evaluate()
  // *******************************************************************
  evaluate();

  return;
}

/*----------------------------------------------------------------------*
 | Finite difference check for M-Mortar derivatives           popp 05/08|
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_mortar_m_deriv()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for M-Matrix entries
  std::map<int, double> refM;  // stores dof-wise the entries of M
  std::map<int, double> newM;

  std::map<int, std::map<int, std::map<int, double>>>
      refDerivM;  // stores old derivm for every node

  // problem dimension (2D or 3D)
  int dim = n_dim();

  // print reference to screen (M-derivative-maps) and store them for later comparison
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    Node* cnode = dynamic_cast<Node*>(node);

    // typedef std::map<int,std::map<int,double> >::const_iterator CIM;
    // typedef std::map<int,double>::const_iterator CI;

    if ((int)(cnode->mo_data().get_m().size()) == 0) continue;

    refM = cnode->mo_data().get_m();

    refDerivM[gid] = cnode->data().get_deriv_m();
  }

  // global loop to apply FD scheme to all slave dofs (=dim*nodes)
  for (int fd = 0; fd < dim * snodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    int sdof = snode->dofs()[fd % dim];

    std::cout << "\nDERIVATIVE FOR S-NODE # " << gid << " DOF: " << sdof << std::endl;

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if ((int)(kcnode->mo_data().get_m().size()) == 0) continue;

      typedef std::map<int, double>::const_iterator CI;

      // store M-values into refM
      newM = kcnode->mo_data().get_m();

      // print results (derivatives) to screen
      for (CI p = newM.begin(); p != newM.end(); ++p)
      {
        if (abs(newM[p->first] - refM[p->first]) > 1e-12)
        {
          double finit = (newM[p->first] - refM[p->first]) / delta;
          double analy = ((refDerivM[kgid])[(p->first)])[sdof];
          double dev = finit - analy;

          // kgid: currently tested dof of slave node kgid
          // (p->first)/Dim(): paired master
          // sdof: currently modified slave dof
          std::cout << "(" << (p->first) << "," << sdof << ") : fd=" << finit << " derivm=" << analy
                    << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // global loop to apply FD scheme to all MASTER dofs (=dim*nodes)
  for (int fd = 0; fd < dim * mnodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    int mdof = mnode->dofs()[fd % dim];

    std::cout << "\nDEVIATION FOR M-NODE # " << gid << " DOF: " << mdof << std::endl;

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if ((int)(kcnode->mo_data().get_m().size()) == 0) continue;

      typedef std::map<int, double>::const_iterator CI;

      // store M-values into refM
      newM = kcnode->mo_data().get_m();

      // print results (derivatives) to screen
      for (CI p = newM.begin(); p != newM.end(); ++p)
      {
        if (abs(newM[p->first] - refM[p->first]) > 1e-12)
        {
          double finit = (newM[p->first] - refM[p->first]) / delta;
          double analy = ((refDerivM[kgid])[p->first])[mdof];
          double dev = finit - analy;

          // dof: currently tested dof of slave node kgid
          // (p->first)/Dim(): paired master
          // mdof: currently modified master dof
          std::cout << "(" << (p->first) << "," << mdof << ") : fd=" << finit << " derivm=" << analy
                    << " DEVIATION " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " ***** WARNING ***** ";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " ***** warning ***** ";
            w++;
          }

          std::cout << std::endl;
        }
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // back to normal...

  // Initialize
  initialize();

  // compute element areas
  set_element_areas();

  // *******************************************************************
  // contents of evaluate()
  // *******************************************************************
  evaluate();

  return;
}

/*----------------------------------------------------------------------*
 | Finite difference check for obj.-variant splip           farah 08/13 |
 | derivatives -- TXI                                                   |
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_slip_incr_deriv_txi()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for gap values
  int nrow = snoderowmap_->NumMyElements();
  std::vector<double> refU(nrow);
  std::vector<double> newU(nrow);

  // problem dimension (2D or 3D)
  int dim = n_dim();

  // store reference
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    FriNode* cnode = dynamic_cast<FriNode*>(node);

    refU[i] = cnode->fri_data().jump_var()[0];  // txi value
  }

  // global loop to apply FD scheme to all slave dofs (=dim*nodes)
  for (int fd = 0; fd < dim * snodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    int sdof = snode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR S-NODE # " << gid << " DOF: " << sdof << std::endl;

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      FriNode* kcnode = dynamic_cast<FriNode*>(knode);

      // store gap-values into newG
      newU[k] = kcnode->fri_data().jump_var()[0];

      if (abs(newU[k] - refU[k]) > 1e-12 && newU[k] != 1.0e12 && refU[k] != 1.0e12)
      {
        double finit = (newU[k] - refU[k]) / delta;
        double analy = kcnode->fri_data().get_deriv_var_jump()[0][snode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // snode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << snode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivu -- 1=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          // FOUR_C_THROW("WARNING --- LIN");
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }
    // undo finite difference modification
    if (fd % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // global loop to apply FD scheme to all master dofs (=dim*nodes)
  for (int fd = 0; fd < dim * mnodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    // loop over all nodes to reset normals, closestnode and Mortar maps
    // (use fully overlapping column map)
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find master node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    int mdof = mnode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR M-NODE # " << gid << " DOF: " << mdof << std::endl;

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      FriNode* kcnode = dynamic_cast<FriNode*>(knode);

      // store gap-values into newG
      newU[k] = kcnode->fri_data().jump_var()[0];

      if (abs(newU[k] - refU[k]) > 1e-12 && newU[k] != 1.0e12 && refU[k] != 1.0e12)
      {
        double finit = (newU[k] - refU[k]) / delta;
        double analy = kcnode->fri_data().get_deriv_var_jump()[0][mnode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // mnode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << mnode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivu -- 1=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          // FOUR_C_THROW("WARNING --- LIN");
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // back to normal...
  initialize();
  evaluate();

  return;
}

/*----------------------------------------------------------------------*
 | Finite difference check for obj.-variant splip           farah 08/13 |
 | derivatives -- TXI                                                        |
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_slip_incr_deriv_teta()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for gap values
  int nrow = snoderowmap_->NumMyElements();
  std::vector<double> refU(nrow);
  std::vector<double> newU(nrow);

  // problem dimension (2D or 3D)
  int dim = n_dim();

  // store reference
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    FriNode* cnode = dynamic_cast<FriNode*>(node);

    // store gap-values into refG
    refU[i] = cnode->fri_data().jump_var()[1];  // txi value
  }

  // global loop to apply FD scheme to all slave dofs (=dim*nodes)
  for (int fd = 0; fd < dim * snodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    int sdof = snode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR S-NODE # " << gid << " DOF: " << sdof << std::endl;

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      FriNode* kcnode = dynamic_cast<FriNode*>(knode);

      // store gap-values into newG
      newU[k] = kcnode->fri_data().jump_var()[1];

      if (abs(newU[k] - refU[k]) > 1e-12 && newU[k] != 1.0e12 && refU[k] != 1.0e12)
      {
        double finit = (newU[k] - refU[k]) / delta;
        double analy = kcnode->fri_data().get_deriv_var_jump()[1][snode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // snode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << snode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivu -- 2=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          // FOUR_C_THROW("WARNING --- LIN");
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }
    // undo finite difference modification
    if (fd % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // global loop to apply FD scheme to all master dofs (=dim*nodes)
  for (int fd = 0; fd < dim * mnodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    // loop over all nodes to reset normals, closestnode and Mortar maps
    // (use fully overlapping column map)
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find master node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    int mdof = mnode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR M-NODE # " << gid << " DOF: " << mdof << std::endl;

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      FriNode* kcnode = dynamic_cast<FriNode*>(knode);

      // store gap-values into newG
      newU[k] = kcnode->fri_data().jump_var()[1];

      if (abs(newU[k] - refU[k]) > 1e-12 && newU[k] != 1.0e12 && refU[k] != 1.0e12)
      {
        double finit = (newU[k] - refU[k]) / delta;
        double analy = kcnode->fri_data().get_deriv_var_jump()[1][mnode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // mnode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << mnode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivu -- 2=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          // FOUR_C_THROW("WARNING --- LIN");
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // back to normal...
  initialize();
  evaluate();

  return;
}


/*----------------------------------------------------------------------*
 | Finite difference check for alpha derivatives             farah 05/16|
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_alpha_deriv()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for gap values
  int nrow = snoderowmap_->NumMyElements();
  std::vector<double> refG(nrow);
  std::vector<double> newG(nrow);
  std::vector<double> refa(nrow);
  std::vector<double> newa(nrow);

  // problem dimension (2D or 3D)
  int dim = n_dim();

  // store reference
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    Node* cnode = dynamic_cast<Node*>(node);

    //    if (cnode->Active())
    //    {
    //      // check two versions of weighted gap
    //      double defgap = 0.0;
    //      double wii = (cnode->MoData().GetD()[0])[cnode->Dofs()[0]];
    //
    //      for (int j=0;j<dim;++j)
    //        defgap-= (cnode->MoData().n()[j])*wii*(cnode->xspatial()[j]);
    //
    //      std::vector<std::map<int,double> > mmap = cnode->MoData().GetM();
    //      std::map<int,double>::iterator mcurr;
    //
    //      for (int m=0;m<mnodefullmap->NumMyElements();++m)
    //      {
    //        int gid = mnodefullmap->GID(m);
    //        Core::Nodes::Node* mnode = idiscret_->gNode(gid);
    //        if (!mnode) FOUR_C_THROW("Cannot find node with gid %",gid);
    //        Node* cmnode = dynamic_cast<Node*>(mnode);
    //        const int* mdofs = cmnode->Dofs();
    //        bool hasentry = false;
    //
    //        // look for this master node in M-map of the active slave node
    //        for (mcurr=mmap[0].begin();mcurr!=mmap[0].end();++mcurr)
    //          if ((mcurr->first)==mdofs[0])
    //          {
    //            hasentry=true;
    //            break;
    //          }
    //
    //        double mik = (mmap[0])[mdofs[0]];
    //        double* mxi = cmnode->xspatial();
    //
    //        // get out of here, if master node not adjacent or coupling very weak
    //        if (!hasentry || abs(mik)<1.0e-12) continue;
    //
    //        for (int j=0;j<dim;++j)
    //          defgap+= (cnode->MoData().n()[j]) * mik * mxi[j];
    //      }
    //
    //      //std::cout << "SNode: " << cnode->Id() << " IntGap: " << cnode->Data().Getg() << "
    //      DefGap: " << defgap << endl;
    //      //cnode->Data().Getg = defgap;
    //    }

    // store gap-values into refG
    refG[i] = cnode->data().getg();
    refa[i] = cnode->data().get_alpha_n();
  }

  // global loop to apply FD scheme to all slave dofs (=dim*nodes)
  for (int fd = 0; fd < dim * snodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    int sdof = snode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR S-NODE # " << gid << " DOF: " << sdof << std::endl;

    // apply finite difference scheme
    /*if (Core::Communication::my_mpi_rank(Comm())==snode->Owner())
    {
      std::cout << "\nBuilding FD for Slave Node: " << snode->Id() << " Dof(l): " << fd%dim
           << " Dof(g): " << snode->Dofs()[fd%dim] << std::endl;
    }*/

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if (kcnode->data().get_alpha_n() < 0.0) continue;

      //      if (kcnode->Active())
      //      {
      //        // check two versions of weighted gap
      //        double defgap = 0.0;
      //        double wii = (kcnode->MoData().GetD()[0])[kcnode->Dofs()[0]];
      //
      //        for (int j=0;j<dim;++j)
      //          defgap-= (kcnode->MoData().n()[j])*wii*(kcnode->xspatial()[j]);
      //
      //        std::vector<std::map<int,double> > mmap = kcnode->MoData().GetM();
      //        std::map<int,double>::iterator mcurr;
      //
      //        for (int m=0;m<mnodefullmap->NumMyElements();++m)
      //        {
      //          int gid = mnodefullmap->GID(m);
      //          Core::Nodes::Node* mnode = idiscret_->gNode(gid);
      //          if (!mnode) FOUR_C_THROW("Cannot find node with gid %",gid);
      //          Node* cmnode = dynamic_cast<Node*>(mnode);
      //          const int* mdofs = cmnode->Dofs();
      //          bool hasentry = false;
      //
      //          // look for this master node in M-map of the active slave node
      //          for (mcurr=mmap[0].begin();mcurr!=mmap[0].end();++mcurr)
      //            if ((mcurr->first)==mdofs[0])
      //            {
      //              hasentry=true;
      //              break;
      //            }
      //
      //          double mik = (mmap[0])[mdofs[0]];
      //          double* mxi = cmnode->xspatial();
      //
      //          // get out of here, if master node not adjacent or coupling very weak
      //          if (!hasentry || abs(mik)<1.0e-12) continue;
      //
      //          for (int j=0;j<dim;++j)
      //            defgap+= (kcnode->MoData().n()[j]) * mik * mxi[j];
      //        }
      //
      //        //std::cout << "SNode: " << kcnode->Id() << " IntGap: " << kcnode->Data().Getg <<
      //        " DefGap: " << defgap << endl;
      //        //kcnode->Data().Getg = defgap;
      //      }

      // store gap-values into newG
      newG[k] = kcnode->data().getg();
      newa[k] = kcnode->data().get_alpha_n();


      if (abs(newa[k] - refa[k]) > 1e-12 && newa[k] != 1.0e12 && refa[k] != 1.0e12)
      {
        double finit = (newa[k] - refa[k]) / delta;
        double analy = kcnode->data().get_alpha()[snode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // snode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << snode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivg=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }
    // undo finite difference modification
    if (fd % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // global loop to apply FD scheme to all master dofs (=dim*nodes)
  for (int fd = 0; fd < dim * mnodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    // loop over all nodes to reset normals, closestnode and Mortar maps
    // (use fully overlapping column map)
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find master node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    int mdof = mnode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR M-NODE # " << gid << " DOF: " << mdof << std::endl;

    // apply finite difference scheme
    /*if (Core::Communication::my_mpi_rank(Comm())==mnode->Owner())
    {
      std::cout << "\nBuilding FD for Master Node: " << mnode->Id() << " Dof(l): " << fd%dim
           << " Dof(g): " << mnode->Dofs()[fd%dim] << std::endl;
    }*/

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if (kcnode->data().get_alpha_n() < 0.0) continue;

      if (kcnode->active())
      {
        // check two versions of weighted gap
        std::map<int, double>& mmap = kcnode->mo_data().get_m();
        std::map<int, double>::const_iterator mcurr;

        for (int m = 0; m < mnodefullmap->NumMyElements(); ++m)
        {
          int gid = mnodefullmap->GID(m);
          Core::Nodes::Node* mnode = idiscret_->g_node(gid);
          if (!mnode) FOUR_C_THROW("Cannot find node with gid %", gid);
          Node* cmnode = dynamic_cast<Node*>(mnode);
          bool hasentry = false;

          // look for this master node in M-map of the active slave node
          for (mcurr = mmap.begin(); mcurr != mmap.end(); ++mcurr)
            if ((mcurr->first) == cmnode->id())
            {
              hasentry = true;
              break;
            }

          double mik = mmap[cmnode->id()];

          // get out of here, if master node not adjacent or coupling very weak
          if (!hasentry || abs(mik) < 1.0e-12) continue;
        }
      }

      // store gap-values into newG
      newG[k] = kcnode->data().getg();
      newa[k] = kcnode->data().get_alpha_n();

      if (abs(newa[k] - refa[k]) > 1e-12 && newa[k] != 1.0e12 && refa[k] != 1.0e12)
      {
        double finit = (newa[k] - refa[k]) / delta;
        double analy = kcnode->data().get_alpha()[mnode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // mnode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << mnode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivg=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // back to normal...
  initialize();
  evaluate();

  return;
}


/*----------------------------------------------------------------------*
 | Finite difference check for normal gap derivatives        Farah 06/16|
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_gap_deriv_ltl()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for gap values
  int nrow = snoderowmap_->NumMyElements();
  std::vector<double> refG0(nrow);
  std::vector<double> newG0(nrow);
  std::vector<double> refG1(nrow);
  std::vector<double> newG1(nrow);
  std::vector<double> refG2(nrow);
  std::vector<double> newG2(nrow);
  // problem dimension (2D or 3D)
  int dim = n_dim();

  // store reference
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    Node* cnode = dynamic_cast<Node*>(node);

    // store gap-values into refG
    refG0[i] = cnode->data().getgltl()[0];
    refG1[i] = cnode->data().getgltl()[1];
    refG2[i] = cnode->data().getgltl()[2];
  }

  // global loop to apply FD scheme to all slave dofs (=dim*nodes)
  for (int fd = 0; fd < dim * snodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    int sdof = snode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR S-NODE # " << gid << " DOF: " << sdof << std::endl;

    // apply finite difference scheme
    /*if (Core::Communication::my_mpi_rank(Comm())==snode->Owner())
    {
      std::cout << "\nBuilding FD for Slave Node: " << snode->Id() << " Dof(l): " << fd%dim
           << " Dof(g): " << snode->Dofs()[fd%dim] << std::endl;
    }*/

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      // store gap-values into newG
      newG0[k] = kcnode->data().getgltl()[0];
      newG1[k] = kcnode->data().getgltl()[1];
      newG2[k] = kcnode->data().getgltl()[2];


      if (abs(newG0[k] - refG0[k]) > 1e-12 && newG0[k] != 1.0e12 && refG0[k] != 1.0e12)
      {
        double finit = (newG0[k] - refG0[k]) / delta;
        double analy = kcnode->data().get_deriv_gltl()[0][snode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // snode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << snode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivg=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
      if (abs(newG1[k] - refG1[k]) > 1e-12 && newG1[k] != 1.0e12 && refG1[k] != 1.0e12)
      {
        double finit = (newG1[k] - refG1[k]) / delta;
        double analy = kcnode->data().get_deriv_gltl()[1][snode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // snode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << snode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivg=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
      if (abs(newG2[k] - refG2[k]) > 1e-12 && newG2[k] != 1.0e12 && refG2[k] != 1.0e12)
      {
        double finit = (newG2[k] - refG2[k]) / delta;
        double analy = kcnode->data().get_deriv_gltl()[2][snode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // snode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << snode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivg=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }
    // undo finite difference modification
    if (fd % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // global loop to apply FD scheme to all master dofs (=dim*nodes)
  for (int fd = 0; fd < dim * mnodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    // loop over all nodes to reset normals, closestnode and Mortar maps
    // (use fully overlapping column map)
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find master node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    int mdof = mnode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR M-NODE # " << gid << " DOF: " << mdof << std::endl;

    // apply finite difference scheme
    /*if (Core::Communication::my_mpi_rank(Comm())==mnode->Owner())
    {
      std::cout << "\nBuilding FD for Master Node: " << mnode->Id() << " Dof(l): " << fd%dim
           << " Dof(g): " << mnode->Dofs()[fd%dim] << std::endl;
    }*/

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if (kcnode->active())
      {
        // check two versions of weighted gap

        std::map<int, double>& mmap = kcnode->mo_data().get_m();
        std::map<int, double>::const_iterator mcurr;

        for (int m = 0; m < mnodefullmap->NumMyElements(); ++m)
        {
          int gid = mnodefullmap->GID(m);
          Core::Nodes::Node* mnode = idiscret_->g_node(gid);
          if (!mnode) FOUR_C_THROW("Cannot find node with gid %", gid);
          Node* cmnode = dynamic_cast<Node*>(mnode);
          bool hasentry = false;

          // look for this master node in M-map of the active slave node
          for (mcurr = mmap.begin(); mcurr != mmap.end(); ++mcurr)
            if ((mcurr->first) == cmnode->id())
            {
              hasentry = true;
              break;
            }

          double mik = mmap[cmnode->id()];

          // get out of here, if master node not adjacent or coupling very weak
          if (!hasentry || abs(mik) < 1.0e-12) continue;
        }
      }

      // store gap-values into newG
      newG0[k] = kcnode->data().getgltl()[0];
      newG1[k] = kcnode->data().getgltl()[1];
      newG2[k] = kcnode->data().getgltl()[2];

      if (abs(newG0[k] - refG0[k]) > 1e-12 && newG0[k] != 1.0e12 && refG0[k] != 1.0e12)
      {
        double finit = (newG0[k] - refG0[k]) / delta;
        double analy = kcnode->data().get_deriv_gltl()[0][mnode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // mnode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << mnode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivg=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
      if (abs(newG1[k] - refG1[k]) > 1e-12 && newG1[k] != 1.0e12 && refG1[k] != 1.0e12)
      {
        double finit = (newG1[k] - refG1[k]) / delta;
        double analy = kcnode->data().get_deriv_gltl()[1][mnode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // mnode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << mnode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivg=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
      if (abs(newG2[k] - refG2[k]) > 1e-12 && newG2[k] != 1.0e12 && refG2[k] != 1.0e12)
      {
        double finit = (newG2[k] - refG2[k]) / delta;
        double analy = kcnode->data().get_deriv_gltl()[2][mnode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // mnode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << mnode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivg=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // back to normal...
  initialize();
  evaluate();

  return;
}


/*----------------------------------------------------------------------*
 | Finite difference check for normal gap derivatives        Farah 06/16|
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_jump_deriv_ltl()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for gap values
  int nrow = snoderowmap_->NumMyElements();
  std::vector<double> refG0(nrow);
  std::vector<double> newG0(nrow);
  std::vector<double> refG1(nrow);
  std::vector<double> newG1(nrow);
  std::vector<double> refG2(nrow);
  std::vector<double> newG2(nrow);
  // problem dimension (2D or 3D)
  int dim = n_dim();

  // store reference
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    Node* cnode = dynamic_cast<Node*>(node);

    // store gap-values into refG
    refG0[i] = cnode->data().getjumpltl()[0];
    refG1[i] = cnode->data().getjumpltl()[1];
    refG2[i] = cnode->data().getjumpltl()[2];
  }

  // global loop to apply FD scheme to all slave dofs (=dim*nodes)
  for (int fd = 0; fd < dim * snodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    int sdof = snode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR S-NODE # " << gid << " DOF: " << sdof << std::endl;

    // apply finite difference scheme
    /*if (Core::Communication::my_mpi_rank(Comm())==snode->Owner())
    {
      std::cout << "\nBuilding FD for Slave Node: " << snode->Id() << " Dof(l): " << fd%dim
           << " Dof(g): " << snode->Dofs()[fd%dim] << std::endl;
    }*/

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      // store gap-values into newG
      newG0[k] = kcnode->data().getjumpltl()[0];
      newG1[k] = kcnode->data().getjumpltl()[1];
      newG2[k] = kcnode->data().getjumpltl()[2];


      if (abs(newG0[k] - refG0[k]) > 1e-12 && newG0[k] != 1.0e12 && refG0[k] != 1.0e12)
      {
        double finit = (newG0[k] - refG0[k]) / delta;
        double analy = kcnode->data().get_deriv_jumpltl()[0][snode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // snode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << snode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivj=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
      if (abs(newG1[k] - refG1[k]) > 1e-12 && newG1[k] != 1.0e12 && refG1[k] != 1.0e12)
      {
        double finit = (newG1[k] - refG1[k]) / delta;
        double analy = kcnode->data().get_deriv_jumpltl()[1][snode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // snode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << snode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivj=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
      if (abs(newG2[k] - refG2[k]) > 1e-12 && newG2[k] != 1.0e12 && refG2[k] != 1.0e12)
      {
        double finit = (newG2[k] - refG2[k]) / delta;
        double analy = kcnode->data().get_deriv_jumpltl()[2][snode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // snode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << snode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivj=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }
    // undo finite difference modification
    if (fd % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // global loop to apply FD scheme to all master dofs (=dim*nodes)
  for (int fd = 0; fd < dim * mnodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    // loop over all nodes to reset normals, closestnode and Mortar maps
    // (use fully overlapping column map)
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find master node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    int mdof = mnode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR M-NODE # " << gid << " DOF: " << mdof << std::endl;

    // apply finite difference scheme
    /*if (Core::Communication::my_mpi_rank(Comm())==mnode->Owner())
    {
      std::cout << "\nBuilding FD for Master Node: " << mnode->Id() << " Dof(l): " << fd%dim
           << " Dof(g): " << mnode->Dofs()[fd%dim] << std::endl;
    }*/

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if (kcnode->active())
      {
        // check two versions of weighted gap
        std::map<int, double>& mmap = kcnode->mo_data().get_m();
        std::map<int, double>::const_iterator mcurr;

        for (int m = 0; m < mnodefullmap->NumMyElements(); ++m)
        {
          int gid = mnodefullmap->GID(m);
          Core::Nodes::Node* mnode = idiscret_->g_node(gid);
          if (!mnode) FOUR_C_THROW("Cannot find node with gid %", gid);
          Node* cmnode = dynamic_cast<Node*>(mnode);
          bool hasentry = false;

          // look for this master node in M-map of the active slave node
          for (mcurr = mmap.begin(); mcurr != mmap.end(); ++mcurr)
            if ((mcurr->first) == cmnode->id())
            {
              hasentry = true;
              break;
            }

          double mik = mmap[cmnode->id()];

          // get out of here, if master node not adjacent or coupling very weak
          if (!hasentry || abs(mik) < 1.0e-12) continue;
        }
      }

      // store gap-values into newG
      newG0[k] = kcnode->data().getjumpltl()[0];
      newG1[k] = kcnode->data().getjumpltl()[1];
      newG2[k] = kcnode->data().getjumpltl()[2];

      if (abs(newG0[k] - refG0[k]) > 1e-12 && newG0[k] != 1.0e12 && refG0[k] != 1.0e12)
      {
        double finit = (newG0[k] - refG0[k]) / delta;
        double analy = kcnode->data().get_deriv_jumpltl()[0][mnode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // mnode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << mnode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivj=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
      if (abs(newG1[k] - refG1[k]) > 1e-12 && newG1[k] != 1.0e12 && refG1[k] != 1.0e12)
      {
        double finit = (newG1[k] - refG1[k]) / delta;
        double analy = kcnode->data().get_deriv_jumpltl()[1][mnode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // mnode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << mnode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivj=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
      if (abs(newG2[k] - refG2[k]) > 1e-12 && newG2[k] != 1.0e12 && refG2[k] != 1.0e12)
      {
        double finit = (newG2[k] - refG2[k]) / delta;
        double analy = kcnode->data().get_deriv_jumpltl()[2][mnode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // mnode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << mnode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivj=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // back to normal...
  initialize();
  evaluate();

  return;
}


/*----------------------------------------------------------------------*
 | Finite difference check for normal gap derivatives         popp 06/08|
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_gap_deriv()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for gap values
  int nrow = snoderowmap_->NumMyElements();
  std::vector<double> refG(nrow);
  std::vector<double> newG(nrow);

  // problem dimension (2D or 3D)
  int dim = n_dim();

  // store reference
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    Node* cnode = dynamic_cast<Node*>(node);

    if (!cnode->is_on_edge()) continue;

    //    if (cnode->Active())
    //    {
    //      // check two versions of weighted gap
    //      double defgap = 0.0;
    //      double wii = (cnode->MoData().GetD()[0])[cnode->Dofs()[0]];
    //
    //      for (int j=0;j<dim;++j)
    //        defgap-= (cnode->MoData().n()[j])*wii*(cnode->xspatial()[j]);
    //
    //      std::vector<std::map<int,double> > mmap = cnode->MoData().GetM();
    //      std::map<int,double>::iterator mcurr;
    //
    //      for (int m=0;m<mnodefullmap->NumMyElements();++m)
    //      {
    //        int gid = mnodefullmap->GID(m);
    //        Core::Nodes::Node* mnode = idiscret_->gNode(gid);
    //        if (!mnode) FOUR_C_THROW("Cannot find node with gid %",gid);
    //        Node* cmnode = dynamic_cast<Node*>(mnode);
    //        const int* mdofs = cmnode->Dofs();
    //        bool hasentry = false;
    //
    //        // look for this master node in M-map of the active slave node
    //        for (mcurr=mmap[0].begin();mcurr!=mmap[0].end();++mcurr)
    //          if ((mcurr->first)==mdofs[0])
    //          {
    //            hasentry=true;
    //            break;
    //          }
    //
    //        double mik = (mmap[0])[mdofs[0]];
    //        double* mxi = cmnode->xspatial();
    //
    //        // get out of here, if master node not adjacent or coupling very weak
    //        if (!hasentry || abs(mik)<1.0e-12) continue;
    //
    //        for (int j=0;j<dim;++j)
    //          defgap+= (cnode->MoData().n()[j]) * mik * mxi[j];
    //      }
    //
    //      //std::cout << "SNode: " << cnode->Id() << " IntGap: " << cnode->Data().Getg() << "
    //      DefGap: " << defgap << endl;
    //      //cnode->Data().Getg = defgap;
    //    }

    // store gap-values into refG
    refG[i] = cnode->data().getg();
  }

  // global loop to apply FD scheme to all slave dofs (=dim*nodes)
  for (int fd = 0; fd < dim * snodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    int sdof = snode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR S-NODE # " << gid << " DOF: " << sdof << std::endl;

    // apply finite difference scheme
    /*if (Core::Communication::my_mpi_rank(Comm())==snode->Owner())
    {
      std::cout << "\nBuilding FD for Slave Node: " << snode->Id() << " Dof(l): " << fd%dim
           << " Dof(g): " << snode->Dofs()[fd%dim] << std::endl;
    }*/

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if (!kcnode->is_on_edge()) continue;

      //      if (kcnode->Active())
      //      {
      //        // check two versions of weighted gap
      //        double defgap = 0.0;
      //        double wii = (kcnode->MoData().GetD()[0])[kcnode->Dofs()[0]];
      //
      //        for (int j=0;j<dim;++j)
      //          defgap-= (kcnode->MoData().n()[j])*wii*(kcnode->xspatial()[j]);
      //
      //        std::vector<std::map<int,double> > mmap = kcnode->MoData().GetM();
      //        std::map<int,double>::iterator mcurr;
      //
      //        for (int m=0;m<mnodefullmap->NumMyElements();++m)
      //        {
      //          int gid = mnodefullmap->GID(m);
      //          Core::Nodes::Node* mnode = idiscret_->gNode(gid);
      //          if (!mnode) FOUR_C_THROW("Cannot find node with gid %",gid);
      //          Node* cmnode = dynamic_cast<Node*>(mnode);
      //          const int* mdofs = cmnode->Dofs();
      //          bool hasentry = false;
      //
      //          // look for this master node in M-map of the active slave node
      //          for (mcurr=mmap[0].begin();mcurr!=mmap[0].end();++mcurr)
      //            if ((mcurr->first)==mdofs[0])
      //            {
      //              hasentry=true;
      //              break;
      //            }
      //
      //          double mik = (mmap[0])[mdofs[0]];
      //          double* mxi = cmnode->xspatial();
      //
      //          // get out of here, if master node not adjacent or coupling very weak
      //          if (!hasentry || abs(mik)<1.0e-12) continue;
      //
      //          for (int j=0;j<dim;++j)
      //            defgap+= (kcnode->MoData().n()[j]) * mik * mxi[j];
      //        }
      //
      //        //std::cout << "SNode: " << kcnode->Id() << " IntGap: " << kcnode->Data().Getg <<
      //        " DefGap: " << defgap << endl;
      //        //kcnode->Data().Getg = defgap;
      //      }

      // store gap-values into newG
      newG[k] = kcnode->data().getg();


      if (abs(newG[k] - refG[k]) > 1e-12 && newG[k] != 1.0e12 && refG[k] != 1.0e12)
      {
        double finit = (newG[k] - refG[k]) / delta;
        double analy = kcnode->data().get_deriv_g()[snode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // snode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << snode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivg=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }
    // undo finite difference modification
    if (fd % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // global loop to apply FD scheme to all master dofs (=dim*nodes)
  for (int fd = 0; fd < dim * mnodefullmap->NumMyElements(); ++fd)
  {
    // store warnings for this finite difference
    int w = 0;

    // Initialize
    // loop over all nodes to reset normals, closestnode and Mortar maps
    // (use fully overlapping column map)
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / dim);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find master node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    int mdof = mnode->dofs()[fd % dim];
    std::cout << "\nDERIVATIVE FOR M-NODE # " << gid << " DOF: " << mdof << std::endl;

    // apply finite difference scheme
    /*if (Core::Communication::my_mpi_rank(Comm())==mnode->Owner())
    {
      std::cout << "\nBuilding FD for Master Node: " << mnode->Id() << " Dof(l): " << fd%dim
           << " Dof(g): " << mnode->Dofs()[fd%dim] << std::endl;
    }*/

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      if (!kcnode->is_on_edge()) continue;

      if (kcnode->active())
      {
        // check two versions of weighted gap
        std::map<int, double>& mmap = kcnode->mo_data().get_m();
        std::map<int, double>::const_iterator mcurr;

        for (int m = 0; m < mnodefullmap->NumMyElements(); ++m)
        {
          int gid = mnodefullmap->GID(m);
          Core::Nodes::Node* mnode = idiscret_->g_node(gid);
          if (!mnode) FOUR_C_THROW("Cannot find node with gid %", gid);
          Node* cmnode = dynamic_cast<Node*>(mnode);
          bool hasentry = false;

          // look for this master node in M-map of the active slave node
          for (mcurr = mmap.begin(); mcurr != mmap.end(); ++mcurr)
            if ((mcurr->first) == cmnode->id())
            {
              hasentry = true;
              break;
            }

          double mik = mmap[cmnode->id()];
          // get out of here, if master node not adjacent or coupling very weak
          if (!hasentry || abs(mik) < 1.0e-12) continue;
        }
      }

      // store gap-values into newG
      newG[k] = kcnode->data().getg();

      if (abs(newG[k] - refG[k]) > 1e-12 && newG[k] != 1.0e12 && refG[k] != 1.0e12)
      {
        double finit = (newG[k] - refG[k]) / delta;
        double analy = kcnode->data().get_deriv_g()[mnode->dofs()[fd % dim]];
        double dev = finit - analy;

        // kgid: id of currently tested slave node
        // mnode->Dofs()[fd%dim]: currently modified slave dof
        std::cout << "(" << kgid << "," << mnode->dofs()[fd % dim] << ") : fd=" << finit
                  << " derivg=" << analy << " DEVIATION " << dev;

        if (abs(dev) > 1e-4)
        {
          std::cout << " ***** WARNING ***** ";
          w++;
        }
        else if (abs(dev) > 1e-5)
        {
          std::cout << " ***** warning ***** ";
          w++;
        }

        std::cout << std::endl;
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }

    std::cout << " ******************** GENERATED " << w << " WARNINGS ***************** "
              << std::endl;
  }

  // back to normal...
  initialize();
  evaluate();

  return;
}



/*----------------------------------------------------------------------*
 | Finite difference check for tang. LM derivatives           popp 06/08|
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_tang_lm_deriv()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for tangential LM values
  int nrow = snoderowmap_->NumMyElements();
  std::vector<double> refTLMxi(nrow);
  std::vector<double> newTLMxi(nrow);
  std::vector<double> refTLMeta(nrow);
  std::vector<double> newTLMeta(nrow);

  // store reference
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    Node* cnode = dynamic_cast<Node*>(node);

    double valxi = 0.0;
    double valeta = 0.0;
    for (int dim = 0; dim < 3; ++dim)
    {
      valxi += (cnode->data().txi()[dim]) * (cnode->mo_data().lm()[dim]);
      valeta += (cnode->data().teta()[dim]) * (cnode->mo_data().lm()[dim]);
    }

    // store gap-values into refTLM
    refTLMxi[i] = valxi;
    refTLMeta[i] = valeta;
  }

  // global loop to apply FD scheme to all slave dofs (=3*nodes)
  for (int fd = 0; fd < 3 * snodefullmap->NumMyElements(); ++fd)
  {
    // Initialize
    // loop over all nodes to reset normals, closestnode and Mortar maps
    // (use fully overlapping column map)
    for (int i = 0; i < idiscret_->num_my_col_nodes(); ++i)
    {
      CONTACT::Node* node = dynamic_cast<CONTACT::Node*>(idiscret_->l_col_node(i));

      // reset nodal normal vector
      for (int j = 0; j < 3; ++j)
      {
        node->mo_data().n()[j] = 0.0;
        node->data().txi()[j] = 0.0;
        node->data().teta()[j] = 0.0;
      }

      // reset derivative maps of normal vector
      for (int j = 0; j < (int)((node->data().get_deriv_n()).size()); ++j)
        (node->data().get_deriv_n())[j].clear();
      (node->data().get_deriv_n()).resize(0, 0);

      // reset derivative maps of tangent vectors
      for (int j = 0; j < (int)((node->data().get_deriv_txi()).size()); ++j)
        (node->data().get_deriv_txi())[j].clear();
      (node->data().get_deriv_txi()).resize(0, 0);
      for (int j = 0; j < (int)((node->data().get_deriv_teta()).size()); ++j)
        (node->data().get_deriv_teta())[j].clear();
      (node->data().get_deriv_teta()).resize(0, 0);

      // reset nodal Mortar maps
      node->mo_data().get_d().clear();
      node->mo_data().get_m().clear();
      node->mo_data().get_mmod().clear();

      // reset derivative map of Mortar matrices
      (node->data().get_deriv_d()).clear();
      (node->data().get_deriv_m()).clear();

      // reset nodal weighted gap
      node->data().getg() = 1.0e12;
      (node->data().get_deriv_g()).clear();

      // reset feasible projection and segmentation status
      node->has_proj() = false;
      node->has_segment() = false;
    }

    // loop over all elements to reset candidates / search lists
    // (use standard slave column map)
    for (int i = 0; i < slave_col_elements()->NumMyElements(); ++i)
    {
      int gid = slave_col_elements()->GID(i);
      Core::Elements::Element* ele = discret().g_element(gid);
      if (!ele) FOUR_C_THROW("Cannot find ele with gid %i", gid);
      Mortar::Element* mele = dynamic_cast<Mortar::Element*>(ele);

      mele->mo_data().search_elements().resize(0);
    }

    // reset matrix containing interface contact segments (gmsh)
    // CSegs().Shape(0,0);

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / 3);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    // apply finite difference scheme
    if (Core::Communication::my_mpi_rank(get_comm()) == snode->owner())
    {
      std::cout << "\nBuilding FD for Slave Node: " << snode->id() << " Dof(l): " << fd % 3
                << " Dof(g): " << snode->dofs()[fd % 3] << std::endl;
    }

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % 3 == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (fd % 3 == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    // loop over proc's slave nodes of the interface
    // use standard column map to include processor's ghosted nodes
    // use boundary map to include slave side boundary nodes
    for (int i = 0; i < snodecolmapbound_->NumMyElements(); ++i)
    {
      int gid1 = snodecolmapbound_->GID(i);
      Core::Nodes::Node* node = idiscret_->g_node(gid1);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", gid1);
      Node* cnode = dynamic_cast<Node*>(node);

      // build averaged normal at each slave node
      cnode->build_averaged_normal();
    }

    // contact search algorithm
    evaluate_search_binarytree();

    // loop over proc's slave elements of the interface for integration
    // use standard column map to include processor's ghosted elements
    for (int i = 0; i < selecolmap_->NumMyElements(); ++i)
    {
      int gid1 = selecolmap_->GID(i);
      Core::Elements::Element* ele1 = idiscret_->g_element(gid1);
      if (!ele1) FOUR_C_THROW("Cannot find slave element with gid %", gid1);
      Mortar::Element* selement = dynamic_cast<Mortar::Element*>(ele1);

      // empty vector of master element pointers
      std::vector<Mortar::Element*> melements;

      // loop over the candidate master elements of sele_
      // use slave element's candidate list SearchElements !!!
      for (int j = 0; j < selement->mo_data().num_search_elements(); ++j)
      {
        int gid2 = selement->mo_data().search_elements()[j];
        Core::Elements::Element* ele2 = idiscret_->g_element(gid2);
        if (!ele2) FOUR_C_THROW("Cannot find master element with gid %", gid2);
        Mortar::Element* melement = dynamic_cast<Mortar::Element*>(ele2);
        melements.push_back(melement);
      }

      //********************************************************************
      // 1) perform coupling (projection + overlap detection for sl/m pair)
      // 2) integrate Mortar matrix M and weighted gap g
      // 3) compute directional derivative of M and g and store into nodes
      //********************************************************************
      mortar_coupling(selement, melements, nullptr);
    }
    // *******************************************************************

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      double valxi = 0.0;
      double valeta = 0.0;
      for (int dim = 0; dim < 3; ++dim)
      {
        valxi += (kcnode->data().txi()[dim]) * (kcnode->mo_data().lm()[dim]);
        valeta += (kcnode->data().teta()[dim]) * (kcnode->mo_data().lm()[dim]);
      }

      // store gap-values into newTLM
      newTLMxi[k] = valxi;
      newTLMeta[k] = valeta;

      // print results (derivatives) to screen
      if (abs(newTLMxi[k] - refTLMxi[k]) > 1e-12)
      {
        std::cout << "Xi-TLM-FD-derivative for node S" << kcnode->id() << std::endl;
        // std::cout << "Ref-Xi-TLM: " << refTLMxi[k] << std::endl;
        // std::cout << "New-Xi-TLM: " << newTLMxi[k] << std::endl;
        std::cout << "Deriv: " << snode->dofs()[fd % 3] << " "
                  << (newTLMxi[k] - refTLMxi[k]) / delta << std::endl;
      }
      // print results (derivatives) to screen
      if (abs(newTLMeta[k] - refTLMeta[k]) > 1e-12)
      {
        std::cout << "Eta-TLM-FD-derivative for node S" << kcnode->id() << std::endl;
        // std::cout << "Ref-TLM: " << refTLMeta[k] << std::endl;
        // std::cout << "New-TLM: " << newTLMeta[k] << std::endl;
        std::cout << "Deriv: " << snode->dofs()[fd % 3] << " "
                  << (newTLMeta[k] - refTLMeta[k]) / delta << std::endl;
      }
    }

    // undo finite difference modification
    if (fd % 3 == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % 3 == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }
  }

  // global loop to apply FD scheme to all master dofs (=3*nodes)
  for (int fd = 0; fd < 3 * mnodefullmap->NumMyElements(); ++fd)
  {
    // Initialize
    // loop over all nodes to reset normals, closestnode and Mortar maps
    // (use fully overlapping column map)
    for (int i = 0; i < idiscret_->num_my_col_nodes(); ++i)
    {
      CONTACT::Node* node = dynamic_cast<CONTACT::Node*>(idiscret_->l_col_node(i));

      // reset nodal normal vector
      for (int j = 0; j < 3; ++j)
      {
        node->mo_data().n()[j] = 0.0;
        node->data().txi()[j] = 0.0;
        node->data().teta()[j] = 0.0;
      }

      // reset derivative maps of normal vector
      for (int j = 0; j < (int)((node->data().get_deriv_n()).size()); ++j)
        (node->data().get_deriv_n())[j].clear();
      (node->data().get_deriv_n()).resize(0, 0);

      // reset derivative maps of tangent vectors
      for (int j = 0; j < (int)((node->data().get_deriv_txi()).size()); ++j)
        (node->data().get_deriv_txi())[j].clear();
      (node->data().get_deriv_txi()).resize(0, 0);
      for (int j = 0; j < (int)((node->data().get_deriv_teta()).size()); ++j)
        (node->data().get_deriv_teta())[j].clear();
      (node->data().get_deriv_teta()).resize(0, 0);

      // reset nodal Mortar maps
      node->mo_data().get_d().clear();
      node->mo_data().get_m().clear();
      node->mo_data().get_mmod().clear();

      // reset derivative map of Mortar matrices
      (node->data().get_deriv_d()).clear();
      (node->data().get_deriv_m()).clear();

      // reset nodal weighted gap
      node->data().getg() = 1.0e12;
      (node->data().get_deriv_g()).clear();

      // reset feasible projection and segmentation status
      node->has_proj() = false;
      node->has_segment() = false;
    }

    // loop over all elements to reset candidates / search lists
    // (use standard slave column map)
    for (int i = 0; i < slave_col_elements()->NumMyElements(); ++i)
    {
      int gid = slave_col_elements()->GID(i);
      Core::Elements::Element* ele = discret().g_element(gid);
      if (!ele) FOUR_C_THROW("Cannot find ele with gid %i", gid);
      Mortar::Element* mele = dynamic_cast<Mortar::Element*>(ele);

      mele->mo_data().search_elements().resize(0);
    }

    // reset matrix containing interface contact segments (gmsh)
    // CSegs().Shape(0,0);

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / 3);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find master node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    // apply finite difference scheme
    if (Core::Communication::my_mpi_rank(get_comm()) == mnode->owner())
    {
      std::cout << "\nBuilding FD for Master Node: " << mnode->id() << " Dof(l): " << fd % 3
                << " Dof(g): " << mnode->dofs()[fd % 3] << std::endl;
    }

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % 3 == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (fd % 3 == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    // loop over proc's slave nodes of the interface
    // use standard column map to include processor's ghosted nodes
    // use boundary map to include slave side boundary nodes
    for (int i = 0; i < snodecolmapbound_->NumMyElements(); ++i)
    {
      int gid1 = snodecolmapbound_->GID(i);
      Core::Nodes::Node* node = idiscret_->g_node(gid1);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", gid1);
      Node* cnode = dynamic_cast<Node*>(node);

      // build averaged normal at each slave node
      cnode->build_averaged_normal();
    }

    // contact search algorithm
    evaluate_search_binarytree();

    // loop over proc's slave elements of the interface for integration
    // use standard column map to include processor's ghosted elements
    for (int i = 0; i < selecolmap_->NumMyElements(); ++i)
    {
      int gid1 = selecolmap_->GID(i);
      Core::Elements::Element* ele1 = idiscret_->g_element(gid1);
      if (!ele1) FOUR_C_THROW("Cannot find slave element with gid %", gid1);
      Mortar::Element* selement = dynamic_cast<Mortar::Element*>(ele1);

      // empty vector of master element pointers
      std::vector<Mortar::Element*> melements;

      // loop over the candidate master elements of sele_
      // use slave element's candidate list SearchElements !!!
      for (int j = 0; j < selement->mo_data().num_search_elements(); ++j)
      {
        int gid2 = selement->mo_data().search_elements()[j];
        Core::Elements::Element* ele2 = idiscret_->g_element(gid2);
        if (!ele2) FOUR_C_THROW("Cannot find master element with gid %", gid2);
        Mortar::Element* melement = dynamic_cast<Mortar::Element*>(ele2);
        melements.push_back(melement);
      }

      //********************************************************************
      // 1) perform coupling (projection + overlap detection for sl/m pair)
      // 2) integrate Mortar matrix M and weighted gap g
      // 3) compute directional derivative of M and g and store into nodes
      //********************************************************************
      mortar_coupling(selement, melements, nullptr);
    }
    // *******************************************************************

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      double valxi = 0.0;
      double valeta = 0.0;
      for (int dim = 0; dim < 3; ++dim)
      {
        valxi += (kcnode->data().txi()[dim]) * (kcnode->mo_data().lm()[dim]);
        valeta += (kcnode->data().teta()[dim]) * (kcnode->mo_data().lm()[dim]);
      }

      // store gap-values into newTLM
      newTLMxi[k] = valxi;
      newTLMeta[k] = valeta;

      // print results (derivatives) to screen
      if (abs(newTLMxi[k] - refTLMxi[k]) > 1e-12)
      {
        std::cout << "Xi-TLM-FD-derivative for node S" << kcnode->id() << std::endl;
        // std::cout << "Ref-TLM: " << refTLMxi[k] << std::endl;
        // std::cout << "New-TLM: " << newTLMxi[k] << std::endl;
        std::cout << "Deriv: " << mnode->dofs()[fd % 3] << " "
                  << (newTLMxi[k] - refTLMxi[k]) / delta << std::endl;
      }
      // print results (derivatives) to screen
      if (abs(newTLMeta[k] - refTLMeta[k]) > 1e-12)
      {
        std::cout << "Eta-TLM-FD-derivative for node S" << kcnode->id() << std::endl;
        // std::cout << "Ref-TLM: " << refTLMeta[k] << std::endl;
        // std::cout << "New-TLM: " << newTLMeta[k] << std::endl;
        std::cout << "Deriv: " << mnode->dofs()[fd % 3] << " "
                  << (newTLMeta[k] - refTLMeta[k]) / delta << std::endl;
      }
    }

    // undo finite difference modification
    if (fd % 3 == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % 3 == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }
  }

  // back to normal...

  // Initialize
  // loop over all nodes to reset normals, closestnode and Mortar maps
  // (use fully overlapping column map)
  for (int i = 0; i < idiscret_->num_my_col_nodes(); ++i)
  {
    CONTACT::Node* node = dynamic_cast<CONTACT::Node*>(idiscret_->l_col_node(i));

    // reset nodal normal vector
    for (int j = 0; j < 3; ++j)
    {
      node->mo_data().n()[j] = 0.0;
      node->data().txi()[j] = 0.0;
      node->data().teta()[j] = 0.0;
    }

    // reset derivative maps of normal vector
    for (int j = 0; j < (int)((node->data().get_deriv_n()).size()); ++j)
      (node->data().get_deriv_n())[j].clear();
    (node->data().get_deriv_n()).resize(0, 0);

    // reset derivative maps of tangent vectors
    for (int j = 0; j < (int)((node->data().get_deriv_txi()).size()); ++j)
      (node->data().get_deriv_txi())[j].clear();
    (node->data().get_deriv_txi()).resize(0, 0);
    for (int j = 0; j < (int)((node->data().get_deriv_teta()).size()); ++j)
      (node->data().get_deriv_teta())[j].clear();
    (node->data().get_deriv_teta()).resize(0, 0);

    // reset nodal Mortar maps
    node->mo_data().get_d().clear();
    node->mo_data().get_m().clear();
    node->mo_data().get_mmod().clear();

    // reset derivative map of Mortar matrices
    (node->data().get_deriv_d()).clear();
    (node->data().get_deriv_m()).clear();

    // reset nodal weighted gap
    node->data().getg() = 1.0e12;
    (node->data().get_deriv_g()).clear();

    // reset feasible projection and segmentation status
    node->has_proj() = false;
    node->has_segment() = false;
  }

  // loop over all elements to reset candidates / search lists
  // (use standard slave column map)
  for (int i = 0; i < slave_col_elements()->NumMyElements(); ++i)
  {
    int gid = slave_col_elements()->GID(i);
    Core::Elements::Element* ele = discret().g_element(gid);
    if (!ele) FOUR_C_THROW("Cannot find ele with gid %i", gid);
    Mortar::Element* mele = dynamic_cast<Mortar::Element*>(ele);

    mele->mo_data().search_elements().resize(0);
  }

  // reset matrix containing interface contact segments (gmsh)
  // CSegs().Shape(0,0);

  // compute element areas
  set_element_areas();

  // *******************************************************************
  // contents of evaluate()
  // *******************************************************************
  // loop over proc's slave nodes of the interface
  // use standard column map to include processor's ghosted nodes
  // use boundary map to include slave side boundary nodes
  for (int i = 0; i < snodecolmapbound_->NumMyElements(); ++i)
  {
    int gid1 = snodecolmapbound_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid1);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid1);
    Node* cnode = dynamic_cast<Node*>(node);

    // build averaged normal at each slave node
    cnode->build_averaged_normal();
  }

  // contact search algorithm
  evaluate_search_binarytree();

  // loop over proc's slave elements of the interface for integration
  // use standard column map to include processor's ghosted elements
  for (int i = 0; i < selecolmap_->NumMyElements(); ++i)
  {
    int gid1 = selecolmap_->GID(i);
    Core::Elements::Element* ele1 = idiscret_->g_element(gid1);
    if (!ele1) FOUR_C_THROW("Cannot find slave element with gid %", gid1);
    Mortar::Element* selement = dynamic_cast<Mortar::Element*>(ele1);

    // empty vector of master element pointers
    std::vector<Mortar::Element*> melements;

    // loop over the candidate master elements of sele_
    // use slave element's candidate list SearchElements !!!
    for (int j = 0; j < selement->mo_data().num_search_elements(); ++j)
    {
      int gid2 = selement->mo_data().search_elements()[j];
      Core::Elements::Element* ele2 = idiscret_->g_element(gid2);
      if (!ele2) FOUR_C_THROW("Cannot find master element with gid %", gid2);
      Mortar::Element* melement = dynamic_cast<Mortar::Element*>(ele2);
      melements.push_back(melement);
    }

    //********************************************************************
    // 1) perform coupling (projection + overlap detection for sl/m pair)
    // 2) integrate Mortar matrix M and weighted gap g
    // 3) compute directional derivative of M and g and store into nodes
    //********************************************************************
    mortar_coupling(selement, melements, nullptr);
  }
  // *******************************************************************

  return;
}

/*----------------------------------------------------------------------*
 | Finite difference check of stick condition derivatives    farah 08/13|
 | Not for Wear Lin. or modifications concerning the compl.             |
 | fnc. !!! See flags CONSISTENTSTICK / CONSISTENTSLIP                  |
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_stick_deriv(
    Core::LinAlg::SparseMatrix& linstickLMglobal, Core::LinAlg::SparseMatrix& linstickDISglobal)
{
  // create stream
  std::ostringstream oss;

  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // create storage for values of complementary function C
  int nrow = snoderowmap_->NumMyElements();
  int dim = n_dim();
  std::vector<double> refCtxi(nrow);
  std::vector<double> refCteta(nrow);
  std::vector<double> newCtxi(nrow);
  std::vector<double> newCteta(nrow);

  // store reference
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    FriNode* cnode = dynamic_cast<FriNode*>(node);

    double jumptxi = 0;
    double jumpteta = 0;

    if (cnode->active() and !(cnode->fri_data().slip()))
    {
      // calculate value of C-function
      double D = cnode->mo_data().get_d()[cnode->id()];
      double Dold = cnode->fri_data().get_d_old()[cnode->id()];

      for (int dim = 0; dim < cnode->num_dof(); ++dim)
      {
        jumptxi -= (cnode->data().txi()[dim]) * (D - Dold) * (cnode->xspatial()[dim]);
        jumpteta -= (cnode->data().teta()[dim]) * (D - Dold) * (cnode->xspatial()[dim]);
      }

      std::map<int, double>& mmap = cnode->mo_data().get_m();
      std::map<int, double>& mmapold = cnode->fri_data().get_m_old();

      std::map<int, double>::const_iterator colcurr;
      std::set<int> mnodes;

      for (colcurr = mmap.begin(); colcurr != mmap.end(); colcurr++) mnodes.insert(colcurr->first);

      for (colcurr = mmapold.begin(); colcurr != mmapold.end(); colcurr++)
        mnodes.insert(colcurr->first);

      std::set<int>::iterator mcurr;

      // loop over all master nodes (find adjacent ones to this slip node)
      for (mcurr = mnodes.begin(); mcurr != mnodes.end(); mcurr++)
      {
        int gid = *mcurr;
        Core::Nodes::Node* mnode = idiscret_->g_node(gid);
        if (!mnode) FOUR_C_THROW("Cannot find node with gid %", gid);
        FriNode* cmnode = dynamic_cast<FriNode*>(mnode);

        double mik = mmap[cmnode->id()];
        double mikold = mmapold[cmnode->id()];

        std::map<int, double>::iterator mcurr;

        for (int dim = 0; dim < cnode->num_dof(); ++dim)
        {
          jumptxi += (cnode->data().txi()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
          jumpteta += (cnode->data().teta()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
        }
      }  //  loop over master nodes

      // gp-wise slip !!!!!!!
      if (interface_params().get<bool>("GP_SLIP_INCR"))
      {
        jumptxi = cnode->fri_data().jump_var()[0];
        jumpteta = 0.0;

        if (n_dim() == 3) jumpteta = cnode->fri_data().jump_var()[1];
      }
    }  // if cnode == Stick

    // store C in vector
    refCtxi[i] = jumptxi;
    refCteta[i] = jumpteta;
  }  // loop over procs slave nodes

  // **************************************************************************
  // global loop to apply FD scheme to all slave dofs (=3*nodes)
  // **************************************************************************
  for (int fd = 0; fd < dim * snodefullmap->NumMyElements(); ++fd)
  {
    // Initialize
    // loop over all nodes to reset normals, closestnode and Mortar maps
    // (use fully overlapping column map)
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / dim);
    int coldof = 0;
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    FriNode* snode = dynamic_cast<FriNode*>(node);

    // apply finite difference scheme
    if (Core::Communication::my_mpi_rank(get_comm()) == snode->owner())
    {
      // std::cout << "\nBuilding FD for Slave Node: " << snode->Id() << " Dof: " << fd%3
      //     << " Dof: " << snode->Dofs()[fd%3] << endl;
    }

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      snode->xspatial()[0] += delta;
      coldof = snode->dofs()[0];
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] += delta;
      coldof = snode->dofs()[1];
    }
    else
    {
      snode->xspatial()[2] += delta;
      coldof = snode->dofs()[2];
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", kgid);
      FriNode* kcnode = dynamic_cast<FriNode*>(knode);

      double jumptxi = 0;
      double jumpteta = 0;

      if (kcnode->active() and !(kcnode->fri_data().slip()))
      {
        // check two versions of weighted gap
        double D = kcnode->mo_data().get_d()[kcnode->id()];
        double Dold = kcnode->fri_data().get_d_old()[kcnode->id()];

        for (int dim = 0; dim < kcnode->num_dof(); ++dim)
        {
          jumptxi -= (kcnode->data().txi()[dim]) * (D - Dold) * (kcnode->xspatial()[dim]);
          jumpteta -= (kcnode->data().teta()[dim]) * (D - Dold) * (kcnode->xspatial()[dim]);
        }

        std::map<int, double> mmap = kcnode->mo_data().get_m();
        std::map<int, double> mmapold = kcnode->fri_data().get_m_old();

        std::map<int, double>::iterator colcurr;
        std::set<int> mnodes;

        for (colcurr = mmap.begin(); colcurr != mmap.end(); colcurr++)
          mnodes.insert(colcurr->first);

        for (colcurr = mmapold.begin(); colcurr != mmapold.end(); colcurr++)
          mnodes.insert(colcurr->first);

        std::set<int>::iterator mcurr;

        // loop over all master nodes (find adjacent ones to this stick node)
        for (mcurr = mnodes.begin(); mcurr != mnodes.end(); mcurr++)
        {
          int gid = *mcurr;
          Core::Nodes::Node* mnode = idiscret_->g_node(gid);
          if (!mnode) FOUR_C_THROW("Cannot find node with gid %", gid);
          FriNode* cmnode = dynamic_cast<FriNode*>(mnode);

          double mik = mmap[cmnode->id()];
          double mikold = mmapold[cmnode->id()];

          std::map<int, double>::iterator mcurr;

          for (int dim = 0; dim < kcnode->num_dof(); ++dim)
          {
            jumptxi += (kcnode->data().txi()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
            jumpteta += (kcnode->data().teta()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
          }
        }  //  loop over master nodes

        // gp-wise slip !!!!!!!
        if (interface_params().get<bool>("GP_SLIP_INCR"))
        {
          jumptxi = kcnode->fri_data().jump_var()[0];
          jumpteta = 0.0;

          if (n_dim() == 3) jumpteta = kcnode->fri_data().jump_var()[1];
        }
      }  // if cnode == Slip

      newCtxi[k] = jumptxi;
      newCteta[k] = jumpteta;

      // ************************************************************************
      // Extract linearizations from sparse matrix !!!
      // ************************************************************************

      // ********************************* TXI
      std::shared_ptr<Epetra_CrsMatrix> sparse_crs = linstickDISglobal.epetra_matrix();
      sparse_crs->FillComplete();
      double sparse_ij = 0.0;
      int sparsenumentries = 0;
      int sparselength = sparse_crs->NumGlobalEntries(kcnode->dofs()[1]);
      std::vector<double> sparsevalues(sparselength);
      std::vector<int> sparseindices(sparselength);
      // int sparseextractionstatus =
      sparse_crs->ExtractGlobalRowCopy(kcnode->dofs()[1], sparselength, sparsenumentries,
          sparsevalues.data(), sparseindices.data());

      for (int h = 0; h < sparselength; ++h)
      {
        if (sparseindices[h] == coldof)
        {
          sparse_ij = sparsevalues[h];
          break;
        }
        else
          sparse_ij = 0.0;
      }
      double analyt_txi = sparse_ij;

      // ********************************* TETA
      std::shared_ptr<Epetra_CrsMatrix> sparse_crs2 = linstickDISglobal.epetra_matrix();
      sparse_crs2->FillComplete();
      double sparse_2 = 0.0;
      int sparsenumentries2 = 0;
      int sparselength2 = sparse_crs2->NumGlobalEntries(kcnode->dofs()[2]);
      std::vector<double> sparsevalues2(sparselength2);
      std::vector<int> sparseindices2(sparselength2);
      // int sparseextractionstatus =
      sparse_crs->ExtractGlobalRowCopy(kcnode->dofs()[2], sparselength2, sparsenumentries2,
          sparsevalues2.data(), sparseindices2.data());

      for (int h = 0; h < sparselength2; ++h)
      {
        if (sparseindices2[h] == coldof)
        {
          sparse_2 = sparsevalues2[h];
          break;
        }
        else
          sparse_2 = 0.0;
      }
      double analyt_teta = sparse_2;

      // print results (derivatives) to screen
      if (abs(newCtxi[k] - refCtxi[k]) > 1e-12)
      {
        std::cout << "STICK DIS-Deriv_xi:  " << kcnode->id()
                  << "\t w.r.t Master: " << snode->dofs()[fd % dim]
                  << "\t FD= " << std::setprecision(4) << (newCtxi[k] - refCtxi[k]) / delta
                  << "\t analyt= " << std::setprecision(4) << analyt_txi
                  << "\t Error= " << analyt_txi - ((newCtxi[k] - refCtxi[k]) / delta);
        if (abs(analyt_txi - (newCtxi[k] - refCtxi[k]) / delta) > 1.0e-4)
          std::cout << "*** WARNING ***" << std::endl;
        else
          std::cout << " " << std::endl;
      }

      if (abs(newCteta[k] - refCteta[k]) > 1e-12)
      {
        std::cout << "STICK DIS-Deriv_eta: " << kcnode->id()
                  << "\t w.r.t Master: " << snode->dofs()[fd % dim]
                  << "\t FD= " << std::setprecision(4) << (newCteta[k] - refCteta[k]) / delta
                  << "\t analyt= " << std::setprecision(4) << analyt_teta
                  << "\t Error= " << analyt_teta - ((newCteta[k] - refCteta[k]) / delta);
        if (abs(analyt_teta - (newCteta[k] - refCteta[k]) / delta) > 1.0e-4)
          std::cout << "*** WARNING ***" << std::endl;
        else
          std::cout << " " << std::endl;
      }
    }
    // undo finite difference modification
    if (fd % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }
  }  // loop over procs slave nodes

  // **************************************************************************
  // global loop to apply FD scheme to all master dofs (=3*nodes)
  // **************************************************************************
  for (int fd = 0; fd < dim * mnodefullmap->NumMyElements(); ++fd)
  {
    // Initialize
    // loop over all nodes to reset normals, closestnode and Mortar maps
    // (use fully overlapping column map)
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / dim);
    int coldof = 0;
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find master node with gid %", gid);
    FriNode* mnode = dynamic_cast<FriNode*>(node);

    // apply finite difference scheme
    if (Core::Communication::my_mpi_rank(get_comm()) == mnode->owner())
    {
      // std::cout << "\nBuilding FD for Master Node: " << mnode->Id() << " Dof: " << fd%3
      //     << " Dof: " << mnode->Dofs()[fd%3] << endl;
    }

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] += delta;
      coldof = mnode->dofs()[0];
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] += delta;
      coldof = mnode->dofs()[1];
    }
    else
    {
      mnode->xspatial()[2] += delta;
      coldof = mnode->dofs()[2];
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      FriNode* kcnode = dynamic_cast<FriNode*>(knode);

      double jumptxi = 0;
      double jumpteta = 0;

      if (kcnode->active() and !(kcnode->fri_data().slip()))
      {
        // check two versions of weighted gap
        double D = kcnode->mo_data().get_d()[kcnode->id()];
        double Dold = kcnode->fri_data().get_d_old()[kcnode->id()];

        for (int dim = 0; dim < kcnode->num_dof(); ++dim)
        {
          jumptxi -= (kcnode->data().txi()[dim]) * (D - Dold) * (kcnode->xspatial()[dim]);
          jumpteta -= (kcnode->data().teta()[dim]) * (D - Dold) * (kcnode->xspatial()[dim]);
        }

        std::map<int, double> mmap = kcnode->mo_data().get_m();
        std::map<int, double> mmapold = kcnode->fri_data().get_m_old();

        std::map<int, double>::iterator colcurr;
        std::set<int> mnodes;

        for (colcurr = mmap.begin(); colcurr != mmap.end(); colcurr++)
          mnodes.insert(colcurr->first);

        for (colcurr = mmapold.begin(); colcurr != mmapold.end(); colcurr++)
          mnodes.insert(colcurr->first);

        std::set<int>::iterator mcurr;

        // loop over all master nodes (find adjacent ones to this stick node)
        for (mcurr = mnodes.begin(); mcurr != mnodes.end(); mcurr++)
        {
          int gid = *mcurr;
          Core::Nodes::Node* mnode = idiscret_->g_node(gid);
          if (!mnode) FOUR_C_THROW("Cannot find node with gid %", gid);
          FriNode* cmnode = dynamic_cast<FriNode*>(mnode);

          double mik = mmap[cmnode->id()];
          double mikold = mmapold[cmnode->id()];

          std::map<int, double>::iterator mcurr;

          for (int dim = 0; dim < kcnode->num_dof(); ++dim)
          {
            jumptxi += (kcnode->data().txi()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
            jumpteta += (kcnode->data().teta()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
          }
        }  //  loop over master nodes
        // gp-wise slip !!!!!!!

        if (interface_params().get<bool>("GP_SLIP_INCR"))
        {
          jumptxi = kcnode->fri_data().jump_var()[0];
          jumpteta = 0.0;

          if (n_dim() == 3) jumpteta = kcnode->fri_data().jump_var()[1];
        }
      }  // if cnode == Slip

      newCtxi[k] = jumptxi;
      newCteta[k] = jumpteta;


      // ************************************************************************
      // Extract linearizations from sparse matrix !!!
      // ************************************************************************

      // ********************************* TXI
      std::shared_ptr<Epetra_CrsMatrix> sparse_crs = linstickDISglobal.epetra_matrix();
      sparse_crs->FillComplete();
      double sparse_ij = 0.0;
      int sparsenumentries = 0;
      int sparselength = sparse_crs->NumGlobalEntries(kcnode->dofs()[1]);
      std::vector<double> sparsevalues(sparselength);
      std::vector<int> sparseindices(sparselength);
      // int sparseextractionstatus =
      sparse_crs->ExtractGlobalRowCopy(kcnode->dofs()[1], sparselength, sparsenumentries,
          sparsevalues.data(), sparseindices.data());

      for (int h = 0; h < sparselength; ++h)
      {
        if (sparseindices[h] == coldof)
        {
          sparse_ij = sparsevalues[h];
          break;
        }
        else
          sparse_ij = 0.0;
      }
      double analyt_txi = sparse_ij;

      // ********************************* TETA
      std::shared_ptr<Epetra_CrsMatrix> sparse_crs2 = linstickDISglobal.epetra_matrix();
      sparse_crs2->FillComplete();
      double sparse_2 = 0.0;
      int sparsenumentries2 = 0;
      int sparselength2 = sparse_crs2->NumGlobalEntries(kcnode->dofs()[2]);
      std::vector<double> sparsevalues2(sparselength2);
      std::vector<int> sparseindices2(sparselength2);
      // int sparseextractionstatus =
      sparse_crs->ExtractGlobalRowCopy(kcnode->dofs()[2], sparselength2, sparsenumentries2,
          sparsevalues2.data(), sparseindices2.data());

      for (int h = 0; h < sparselength2; ++h)
      {
        if (sparseindices2[h] == coldof)
        {
          sparse_2 = sparsevalues2[h];
          break;
        }
        else
          sparse_2 = 0.0;
      }
      double analyt_teta = sparse_2;

      // print results (derivatives) to screen
      if (abs(newCtxi[k] - refCtxi[k]) > 1e-12)
      {
        std::cout << "STICK DIS-Deriv_xi:  " << kcnode->id()
                  << "\t w.r.t Master: " << mnode->dofs()[fd % dim]
                  << "\t FD= " << std::setprecision(4) << (newCtxi[k] - refCtxi[k]) / delta
                  << "\t analyt= " << std::setprecision(5) << analyt_txi
                  << "\t Error= " << analyt_txi - ((newCtxi[k] - refCtxi[k]) / delta);
        if (abs(analyt_txi - (newCtxi[k] - refCtxi[k]) / delta) > 1.0e-4)
          std::cout << "*** WARNING ***" << std::endl;
        else
          std::cout << " " << std::endl;
      }

      if (abs(newCteta[k] - refCteta[k]) > 1e-12)
      {
        std::cout << "STICK DIS-Deriv_eta: " << kcnode->id()
                  << "\t w.r.t Master: " << mnode->dofs()[fd % dim]
                  << "\t FD= " << std::setprecision(4) << (newCteta[k] - refCteta[k]) / delta
                  << "\t analyt= " << std::setprecision(5) << analyt_teta
                  << "\t Error= " << analyt_teta - ((newCteta[k] - refCteta[k]) / delta);
        if (abs(analyt_teta - (newCteta[k] - refCteta[k]) / delta) > 1.0e-4)
          std::cout << "*** WARNING ***" << std::endl;
        else
          std::cout << " " << std::endl;
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }
  }

  // back to normal...
  initialize();
  evaluate();

  return;

}  // fd_check_stick_deriv

/*----------------------------------------------------------------------*
 | Finite difference check of slip condition derivatives     farah 08/13|
 | Not for Wear Lin. or modifications concerning the compl.             |
 | fnc. !!! See flags CONSISTENTSTICK / CONSISTENTSLIP                  |
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_slip_deriv(
    Core::LinAlg::SparseMatrix& linslipLMglobal, Core::LinAlg::SparseMatrix& linslipDISglobal)
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // information from interface contact parameter list
  auto ftype =
      Teuchos::getIntegralValue<Inpar::CONTACT::FrictionType>(interface_params(), "FRICTION");
  double frbound = interface_params().get<double>("FRBOUND");
  double frcoeff = interface_params().get<double>("FRCOEFF");
  double ct = interface_params().get<double>("SEMI_SMOOTH_CT");
  double cn = interface_params().get<double>("SEMI_SMOOTH_CN");

  // create storage for values of complementary function C
  int nrow = snoderowmap_->NumMyElements();
  std::vector<double> refCtxi(nrow);
  std::vector<double> refCteta(nrow);
  std::vector<double> newCtxi(nrow);
  std::vector<double> newCteta(nrow);

  int dim = n_dim();

  // store reference
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    FriNode* cnode = dynamic_cast<FriNode*>(node);

    double jumptxi = 0;
    double jumpteta = 0;
    double ztxi = 0;
    double zteta = 0;
    double znor = 0;
    double euclidean = 0;

    if (cnode->fri_data().slip())
    {
      // calculate value of C-function
      double D = cnode->mo_data().get_d()[cnode->id()];
      double Dold = cnode->fri_data().get_d_old()[cnode->id()];

      for (int dim = 0; dim < cnode->num_dof(); ++dim)
      {
        jumptxi -= (cnode->data().txi()[dim]) * (D - Dold) * (cnode->xspatial()[dim]);
        jumpteta -= (cnode->data().teta()[dim]) * (D - Dold) * (cnode->xspatial()[dim]);
        ztxi += (cnode->data().txi()[dim]) * (cnode->mo_data().lm()[dim]);
        zteta += (cnode->data().teta()[dim]) * (cnode->mo_data().lm()[dim]);
        znor += (cnode->mo_data().n()[dim]) * (cnode->mo_data().lm()[dim]);
      }

      std::map<int, double>& mmap = cnode->mo_data().get_m();
      std::map<int, double>& mmapold = cnode->fri_data().get_m_old();

      std::map<int, double>::const_iterator colcurr;
      std::set<int> mnodes;

      for (colcurr = mmap.begin(); colcurr != mmap.end(); colcurr++) mnodes.insert(colcurr->first);

      for (colcurr = mmapold.begin(); colcurr != mmapold.end(); colcurr++)
        mnodes.insert(colcurr->first);

      std::set<int>::iterator mcurr;

      // loop over all master nodes (find adjacent ones to this slip node)
      for (mcurr = mnodes.begin(); mcurr != mnodes.end(); mcurr++)
      {
        int gid = *mcurr;
        Core::Nodes::Node* mnode = idiscret_->g_node(gid);
        if (!mnode) FOUR_C_THROW("Cannot find node with gid %", gid);
        FriNode* cmnode = dynamic_cast<FriNode*>(mnode);

        double mik = mmap[cmnode->id()];
        double mikold = mmapold[cmnode->id()];

        std::map<int, double>::iterator mcurr;

        for (int dim = 0; dim < cnode->num_dof(); ++dim)
        {
          jumptxi += (cnode->data().txi()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
          jumpteta += (cnode->data().teta()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
        }
      }  //  loop over master nodes

      // gp-wise slip !!!!!!!
      if (interface_params().get<bool>("GP_SLIP_INCR"))
      {
        jumptxi = cnode->fri_data().jump_var()[0];
        jumpteta = 0.0;

        if (n_dim() == 3) jumpteta = cnode->fri_data().jump_var()[1];
      }

      // evaluate euclidean norm ||vec(zt)+ct*vec(jump)||
      std::vector<double> sum1(n_dim() - 1, 0);
      sum1[0] = ztxi + ct * jumptxi;
      if (n_dim() == 3) sum1[1] = zteta + ct * jumpteta;
      if (n_dim() == 2) euclidean = abs(sum1[0]);
      if (n_dim() == 3) euclidean = sqrt(sum1[0] * sum1[0] + sum1[1] * sum1[1]);
    }  // if cnode == Slip

    // store C in vector
    if (ftype == Inpar::CONTACT::friction_tresca)
    {
      refCtxi[i] = euclidean * ztxi - frbound * (ztxi + ct * jumptxi);
      refCteta[i] = euclidean * zteta - frbound * (zteta + ct * jumpteta);
    }
    else if (ftype == Inpar::CONTACT::friction_coulomb)
    {
      refCtxi[i] = euclidean * ztxi - (frcoeff * znor) * (ztxi + ct * jumptxi);
      refCteta[i] = euclidean * zteta - (frcoeff * znor) * (zteta + ct * jumpteta);
    }
    else
      FOUR_C_THROW("Friction law is neither Tresca nor Coulomb");

    refCtxi[i] =
        euclidean * ztxi - (frcoeff * (znor - cn * cnode->data().getg())) * (ztxi + ct * jumptxi);
    refCteta[i] = euclidean * zteta -
                  (frcoeff * (znor - cn * cnode->data().getg())) * (zteta + ct * jumpteta);

  }  // loop over procs slave nodes

  // **********************************************************************************
  // global loop to apply FD scheme for LM to all slave dofs (=3*nodes)
  // **********************************************************************************
  for (int fd = 0; fd < dim * snodefullmap->NumMyElements(); ++fd)
  {
    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / dim);
    int coldof = 0;
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    FriNode* snode = dynamic_cast<FriNode*>(node);

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      snode->mo_data().lm()[0] += delta;
      coldof = snode->dofs()[0];
    }
    else if (fd % dim == 1)
    {
      snode->mo_data().lm()[1] += delta;
      coldof = snode->dofs()[1];
    }
    else
    {
      snode->mo_data().lm()[2] += delta;
      coldof = snode->dofs()[2];
    }

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", kgid);
      FriNode* kcnode = dynamic_cast<FriNode*>(knode);

      double jumptxi = 0;
      double jumpteta = 0;
      double ztxi = 0;
      double zteta = 0;
      double znor = 0;
      double euclidean = 0;

      if (kcnode->fri_data().slip())
      {
        // check two versions of weighted gap
        double D = kcnode->mo_data().get_d()[kcnode->id()];
        double Dold = kcnode->fri_data().get_d_old()[kcnode->id()];
        for (int dim = 0; dim < kcnode->num_dof(); ++dim)
        {
          jumptxi -= (kcnode->data().txi()[dim]) * (D - Dold) * (kcnode->xspatial()[dim]);
          jumpteta -= (kcnode->data().teta()[dim]) * (D - Dold) * (kcnode->xspatial()[dim]);
          ztxi += (kcnode->data().txi()[dim]) * (kcnode->mo_data().lm()[dim]);
          zteta += (kcnode->data().teta()[dim]) * (kcnode->mo_data().lm()[dim]);
          znor += (kcnode->mo_data().n()[dim]) * (kcnode->mo_data().lm()[dim]);
        }

        std::map<int, double> mmap = kcnode->mo_data().get_m();
        std::map<int, double> mmapold = kcnode->fri_data().get_m_old();

        std::map<int, double>::iterator colcurr;
        std::set<int> mnodes;

        for (colcurr = mmap.begin(); colcurr != mmap.end(); colcurr++)
          mnodes.insert(colcurr->first);

        for (colcurr = mmapold.begin(); colcurr != mmapold.end(); colcurr++)
          mnodes.insert(colcurr->first);

        std::set<int>::iterator mcurr;

        // loop over all master nodes (find adjacent ones to this stick node)
        for (mcurr = mnodes.begin(); mcurr != mnodes.end(); mcurr++)
        {
          int gid = *mcurr;
          Core::Nodes::Node* mnode = idiscret_->g_node(gid);
          if (!mnode) FOUR_C_THROW("Cannot find node with gid %", gid);
          FriNode* cmnode = dynamic_cast<FriNode*>(mnode);
          double mik = mmap[cmnode->id()];
          double mikold = mmapold[cmnode->id()];

          std::map<int, double>::iterator mcurr;

          for (int dim = 0; dim < kcnode->num_dof(); ++dim)
          {
            jumptxi += (kcnode->data().txi()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
            jumpteta += (kcnode->data().teta()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
          }
        }  //  loop over master nodes

        // gp-wise slip !!!!!!!
        if (interface_params().get<bool>("GP_SLIP_INCR"))
        {
          jumptxi = kcnode->fri_data().jump_var()[0];
          jumpteta = 0.0;

          if (n_dim() == 3) jumpteta = kcnode->fri_data().jump_var()[1];
        }

        // evaluate euclidean norm ||vec(zt)+ct*vec(jump)||
        std::vector<double> sum1(n_dim() - 1, 0);
        sum1[0] = ztxi + ct * jumptxi;
        if (n_dim() == 3) sum1[1] = zteta + ct * jumpteta;
        if (n_dim() == 2) euclidean = abs(sum1[0]);
        if (n_dim() == 3) euclidean = sqrt(sum1[0] * sum1[0] + sum1[1] * sum1[1]);
      }  // if cnode == Slip

      // store C in vector
      if (ftype == Inpar::CONTACT::friction_tresca)
      {
        newCtxi[k] = euclidean * ztxi - frbound * (ztxi + ct * jumptxi);
        newCteta[k] = euclidean * zteta - frbound * (zteta + ct * jumpteta);
      }
      else if (ftype == Inpar::CONTACT::friction_coulomb)
      {
        newCtxi[k] = euclidean * ztxi - (frcoeff * znor) * (ztxi + ct * jumptxi);
        newCteta[k] = euclidean * zteta - (frcoeff * znor) * (zteta + ct * jumpteta);
      }
      else
        FOUR_C_THROW("Friction law is neither Tresca nor Coulomb");

      newCtxi[k] = euclidean * ztxi -
                   (frcoeff * (znor - cn * kcnode->data().getg())) * (ztxi + ct * jumptxi);
      newCteta[k] = euclidean * zteta -
                    (frcoeff * (znor - cn * kcnode->data().getg())) * (zteta + ct * jumpteta);

      // ************************************************************************
      // Extract linearizations from sparse matrix !!!
      // ************************************************************************

      // ********************************* TXI
      std::shared_ptr<Epetra_CrsMatrix> sparse_crs = linslipLMglobal.epetra_matrix();
      sparse_crs->FillComplete();
      double sparse_ij = 0.0;
      int sparsenumentries = 0;
      int sparselength = sparse_crs->NumGlobalEntries(kcnode->dofs()[1]);
      std::vector<double> sparsevalues(sparselength);
      std::vector<int> sparseindices(sparselength);
      // int sparseextractionstatus =
      sparse_crs->ExtractGlobalRowCopy(kcnode->dofs()[1], sparselength, sparsenumentries,
          sparsevalues.data(), sparseindices.data());

      for (int h = 0; h < sparselength; ++h)
      {
        if (sparseindices[h] == coldof)
        {
          sparse_ij = sparsevalues[h];
          break;
        }
        else
          sparse_ij = 0.0;
      }
      double analyt_txi = sparse_ij;

      // ********************************* TETA
      std::shared_ptr<Epetra_CrsMatrix> sparse_crs2 = linslipLMglobal.epetra_matrix();
      sparse_crs2->FillComplete();
      double sparse_2 = 0.0;
      int sparsenumentries2 = 0;
      int sparselength2 = sparse_crs2->NumGlobalEntries(kcnode->dofs()[2]);
      std::vector<double> sparsevalues2(sparselength2);
      std::vector<int> sparseindices2(sparselength2);
      // int sparseextractionstatus =
      sparse_crs->ExtractGlobalRowCopy(kcnode->dofs()[2], sparselength2, sparsenumentries2,
          sparsevalues2.data(), sparseindices2.data());

      for (int h = 0; h < sparselength2; ++h)
      {
        if (sparseindices2[h] == coldof)
        {
          sparse_2 = sparsevalues2[h];
          break;
        }
        else
          sparse_2 = 0.0;
      }
      double analyt_teta = sparse_2;

      // print results (derivatives) to screen
      if (abs(newCtxi[k] - refCtxi[k]) > 1e-12)
      {
        std::cout << "SLIP LM-Deriv_xi: " << kcnode->id() << "\t w.r.t: " << snode->dofs()[fd % dim]
                  << "\t FD= " << std::setprecision(4) << (newCtxi[k] - refCtxi[k]) / delta
                  << "\t analyt= " << std::setprecision(4) << analyt_txi
                  << "\t Error= " << analyt_txi - ((newCtxi[k] - refCtxi[k]) / delta);
        if (abs(analyt_txi - (newCtxi[k] - refCtxi[k]) / delta) > 1.0e-4)
          std::cout << "*** WARNING ***" << std::endl;
        else
          std::cout << " " << std::endl;
      }

      // print results (derivatives) to screen
      if (abs(newCteta[k] - refCteta[k]) > 1e-12)
      {
        std::cout << "SLIP LM-Deriv_eta: " << kcnode->id()
                  << "\t w.r.t: " << snode->dofs()[fd % dim] << "\t FD= " << std::setprecision(4)
                  << (newCteta[k] - refCteta[k]) / delta << "\t analyt= " << std::setprecision(4)
                  << analyt_teta
                  << "\t Error= " << analyt_teta - ((newCteta[k] - refCteta[k]) / delta);
        if (abs(analyt_teta - (newCteta[k] - refCteta[k]) / delta) > 1.0e-4)
          std::cout << "*** WARNING ***" << std::endl;
        else
          std::cout << " " << std::endl;
      }
    }
    // undo finite difference modification
    if (fd % dim == 0)
    {
      snode->mo_data().lm()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      snode->mo_data().lm()[1] -= delta;
    }
    else
    {
      snode->mo_data().lm()[2] -= delta;
    }
  }  // loop over procs slave nodes


  // ********************************************************************************************
  // global loop to apply FD scheme to all slave dofs (=3*nodes)
  // ********************************************************************************************
  for (int fd = 0; fd < dim * snodefullmap->NumMyElements(); ++fd)
  {
    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / dim);
    int coldof = 0;
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    FriNode* snode = dynamic_cast<FriNode*>(node);

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      snode->xspatial()[0] += delta;
      coldof = snode->dofs()[0];
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] += delta;
      coldof = snode->dofs()[1];
    }
    else
    {
      snode->xspatial()[2] += delta;
      coldof = snode->dofs()[2];
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", kgid);
      FriNode* kcnode = dynamic_cast<FriNode*>(knode);

      double jumptxi = 0;
      double jumpteta = 0;
      double ztxi = 0;
      double zteta = 0;
      double znor = 0;
      double euclidean = 0;

      if (kcnode->fri_data().slip())
      {
        // check two versions of weighted gap
        double D = kcnode->mo_data().get_d()[kcnode->id()];
        double Dold = kcnode->fri_data().get_d_old()[kcnode->id()];

        for (int dim = 0; dim < kcnode->num_dof(); ++dim)
        {
          jumptxi -= (kcnode->data().txi()[dim]) * (D - Dold) * (kcnode->xspatial()[dim]);
          jumpteta -= (kcnode->data().teta()[dim]) * (D - Dold) * (kcnode->xspatial()[dim]);
          ztxi += (kcnode->data().txi()[dim]) * (kcnode->mo_data().lm()[dim]);
          zteta += (kcnode->data().teta()[dim]) * (kcnode->mo_data().lm()[dim]);
          znor += (kcnode->mo_data().n()[dim]) * (kcnode->mo_data().lm()[dim]);
        }

        std::map<int, double> mmap = kcnode->mo_data().get_m();
        std::map<int, double> mmapold = kcnode->fri_data().get_m_old();

        std::map<int, double>::iterator colcurr;
        std::set<int> mnodes;

        for (colcurr = mmap.begin(); colcurr != mmap.end(); colcurr++)
          mnodes.insert(colcurr->first);

        for (colcurr = mmapold.begin(); colcurr != mmapold.end(); colcurr++)
          mnodes.insert(colcurr->first);

        std::set<int>::iterator mcurr;

        // loop over all master nodes (find adjacent ones to this stick node)
        for (mcurr = mnodes.begin(); mcurr != mnodes.end(); mcurr++)
        {
          int gid = *mcurr;
          Core::Nodes::Node* mnode = idiscret_->g_node(gid);
          if (!mnode) FOUR_C_THROW("Cannot find node with gid %", gid);
          FriNode* cmnode = dynamic_cast<FriNode*>(mnode);

          double mik = mmap[cmnode->id()];
          double mikold = mmapold[cmnode->id()];

          std::map<int, double>::iterator mcurr;

          for (int dim = 0; dim < kcnode->num_dof(); ++dim)
          {
            jumptxi += (kcnode->data().txi()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
            jumpteta += (kcnode->data().teta()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
          }
        }  //  loop over master nodes

        // gp-wise slip !!!!!!!
        if (interface_params().get<bool>("GP_SLIP_INCR"))
        {
          jumptxi = kcnode->fri_data().jump_var()[0];
          jumpteta = 0.0;

          if (n_dim() == 3) jumpteta = kcnode->fri_data().jump_var()[1];
        }

        // evaluate euclidean norm ||vec(zt)+ct*vec(jump)||
        std::vector<double> sum1(n_dim() - 1, 0);
        sum1[0] = ztxi + ct * jumptxi;
        if (n_dim() == 3) sum1[1] = zteta + ct * jumpteta;
        if (n_dim() == 2) euclidean = abs(sum1[0]);
        if (n_dim() == 3) euclidean = sqrt(sum1[0] * sum1[0] + sum1[1] * sum1[1]);

      }  // if cnode == Slip

      // store C in vector
      if (ftype == Inpar::CONTACT::friction_tresca)
      {
        newCtxi[k] = euclidean * ztxi - frbound * (ztxi + ct * jumptxi);
        newCteta[k] = euclidean * zteta - frbound * (zteta + ct * jumpteta);
      }
      else if (ftype == Inpar::CONTACT::friction_coulomb)
      {
        newCtxi[k] = euclidean * ztxi - (frcoeff * znor) * (ztxi + ct * jumptxi);
        newCteta[k] = euclidean * zteta - (frcoeff * znor) * (zteta + ct * jumpteta);
      }
      else
        FOUR_C_THROW("Friction law is neither Tresca nor Coulomb");

      newCtxi[k] = euclidean * ztxi -
                   (frcoeff * (znor - cn * kcnode->data().getg())) * (ztxi + ct * jumptxi);
      newCteta[k] = euclidean * zteta -
                    (frcoeff * (znor - cn * kcnode->data().getg())) * (zteta + ct * jumpteta);



      // ************************************************************************
      // Extract linearizations from sparse matrix !!!
      // ************************************************************************

      // ********************************* TXI
      std::shared_ptr<Epetra_CrsMatrix> sparse_crs = linslipDISglobal.epetra_matrix();
      sparse_crs->FillComplete();
      double sparse_ij = 0.0;
      int sparsenumentries = 0;
      int sparselength = sparse_crs->NumGlobalEntries(kcnode->dofs()[1]);
      std::vector<double> sparsevalues(sparselength);
      std::vector<int> sparseindices(sparselength);
      // int sparseextractionstatus =
      sparse_crs->ExtractGlobalRowCopy(kcnode->dofs()[1], sparselength, sparsenumentries,
          sparsevalues.data(), sparseindices.data());

      for (int h = 0; h < sparselength; ++h)
      {
        if (sparseindices[h] == coldof)
        {
          sparse_ij = sparsevalues[h];
          break;
        }
        else
          sparse_ij = 0.0;
      }
      double analyt_txi = sparse_ij;

      // ********************************* TETA
      std::shared_ptr<Epetra_CrsMatrix> sparse_crs2 = linslipDISglobal.epetra_matrix();
      sparse_crs2->FillComplete();
      double sparse_2 = 0.0;
      int sparsenumentries2 = 0;
      int sparselength2 = sparse_crs2->NumGlobalEntries(kcnode->dofs()[2]);
      std::vector<double> sparsevalues2(sparselength2);
      std::vector<int> sparseindices2(sparselength2);
      // int sparseextractionstatus =
      sparse_crs->ExtractGlobalRowCopy(kcnode->dofs()[2], sparselength2, sparsenumentries2,
          sparsevalues2.data(), sparseindices2.data());

      for (int h = 0; h < sparselength2; ++h)
      {
        if (sparseindices2[h] == coldof)
        {
          sparse_2 = sparsevalues2[h];
          break;
        }
        else
          sparse_2 = 0.0;
      }
      double analyt_teta = sparse_2;


      if (abs(newCtxi[k] - refCtxi[k]) > 1e-12)
      {
        std::cout << "SLIP DIS-Deriv_xi: " << kcnode->id()
                  << "\t w.r.t Slave: " << snode->dofs()[fd % dim]
                  << "\t FD= " << std::setprecision(4) << (newCtxi[k] - refCtxi[k]) / delta
                  << "\t analyt= " << std::setprecision(5) << analyt_txi
                  << "\t Error= " << analyt_txi - ((newCtxi[k] - refCtxi[k]) / delta);
        if (abs(analyt_txi - (newCtxi[k] - refCtxi[k]) / delta) > 1.0e-4)
          std::cout << "*** WARNING ***" << std::endl;
        else
          std::cout << " " << std::endl;
      }

      // print results (derivatives) to screen
      if (abs(newCteta[k] - refCteta[k]) > 1e-12)
      {
        std::cout << "SLIP DIS-Deriv_eta: " << kcnode->id()
                  << "\t w.r.t Slave: " << snode->dofs()[fd % dim]
                  << "\t FD= " << std::setprecision(4) << (newCteta[k] - refCteta[k]) / delta
                  << "\t analyt= " << std::setprecision(5) << analyt_teta
                  << "\t Error= " << analyt_teta - ((newCteta[k] - refCteta[k]) / delta);
        if (abs(analyt_teta - (newCteta[k] - refCteta[k]) / delta) > 1.0e-4)
          std::cout << "*** WARNING ***" << std::endl;
        else
          std::cout << " " << std::endl;
      }
    }
    // undo finite difference modification
    if (fd % dim == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }
  }  // loop over procs slave nodes

  // ********************************************************************************************
  // global loop to apply FD scheme to all master dofs (=3*nodes)
  // ********************************************************************************************
  for (int fd = 0; fd < dim * mnodefullmap->NumMyElements(); ++fd)
  {
    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / dim);
    int coldof = 0;
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find master node with gid %", gid);
    FriNode* mnode = dynamic_cast<FriNode*>(node);

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] += delta;
      coldof = mnode->dofs()[0];
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] += delta;
      coldof = mnode->dofs()[1];
    }
    else
    {
      mnode->xspatial()[2] += delta;
      coldof = mnode->dofs()[2];
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      FriNode* kcnode = dynamic_cast<FriNode*>(knode);

      double jumptxi = 0;
      double jumpteta = 0;
      double ztxi = 0;
      double zteta = 0;
      double znor = 0;
      double euclidean = 0;

      if (kcnode->fri_data().slip())
      {
        // check two versions of weighted gap
        double D = kcnode->mo_data().get_d()[kcnode->id()];
        double Dold = kcnode->fri_data().get_d_old()[kcnode->id()];

        for (int dim = 0; dim < kcnode->num_dof(); ++dim)
        {
          jumptxi -= (kcnode->data().txi()[dim]) * (D - Dold) * (kcnode->xspatial()[dim]);
          jumpteta -= (kcnode->data().teta()[dim]) * (D - Dold) * (kcnode->xspatial()[dim]);
          ztxi += (kcnode->data().txi()[dim]) * (kcnode->mo_data().lm()[dim]);
          zteta += (kcnode->data().teta()[dim]) * (kcnode->mo_data().lm()[dim]);
          znor += (kcnode->mo_data().n()[dim]) * (kcnode->mo_data().lm()[dim]);
        }

        std::map<int, double> mmap = kcnode->mo_data().get_m();
        std::map<int, double> mmapold = kcnode->fri_data().get_m_old();

        std::map<int, double>::iterator colcurr;
        std::set<int> mnodes;

        for (colcurr = mmap.begin(); colcurr != mmap.end(); colcurr++)
          mnodes.insert(colcurr->first);

        for (colcurr = mmapold.begin(); colcurr != mmapold.end(); colcurr++)
          mnodes.insert(colcurr->first);

        std::set<int>::iterator mcurr;

        // loop over all master nodes (find adjacent ones to this stick node)
        for (mcurr = mnodes.begin(); mcurr != mnodes.end(); mcurr++)
        {
          int gid = *mcurr;
          Core::Nodes::Node* mnode = idiscret_->g_node(gid);
          if (!mnode) FOUR_C_THROW("Cannot find node with gid %", gid);
          FriNode* cmnode = dynamic_cast<FriNode*>(mnode);

          double mik = mmap[cmnode->id()];
          double mikold = mmapold[cmnode->id()];

          std::map<int, double>::iterator mcurr;

          for (int dim = 0; dim < kcnode->num_dof(); ++dim)
          {
            jumptxi += (kcnode->data().txi()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
            jumpteta += (kcnode->data().teta()[dim]) * (mik - mikold) * (cmnode->xspatial()[dim]);
          }
        }  //  loop over master nodes

        // gp-wise slip !!!!!!!
        if (interface_params().get<bool>("GP_SLIP_INCR"))
        {
          jumptxi = kcnode->fri_data().jump_var()[0];
          jumpteta = 0.0;

          if (n_dim() == 3) jumpteta = kcnode->fri_data().jump_var()[1];
        }

        // evaluate euclidean norm ||vec(zt)+ct*vec(jump)||
        std::vector<double> sum1(n_dim() - 1, 0);
        sum1[0] = ztxi + ct * jumptxi;
        if (n_dim() == 3) sum1[1] = zteta + ct * jumpteta;
        if (n_dim() == 2) euclidean = abs(sum1[0]);
        if (n_dim() == 3) euclidean = sqrt(sum1[0] * sum1[0] + sum1[1] * sum1[1]);
      }  // if cnode == Slip

      // store C in vector
      if (ftype == Inpar::CONTACT::friction_tresca)
      {
        newCtxi[k] = euclidean * ztxi - frbound * (ztxi + ct * jumptxi);
        newCteta[k] = euclidean * zteta - frbound * (zteta + ct * jumpteta);
      }
      else if (ftype == Inpar::CONTACT::friction_coulomb)
      {
        newCtxi[k] = euclidean * ztxi - (frcoeff * znor) * (ztxi + ct * jumptxi);
        newCteta[k] = euclidean * zteta - (frcoeff * znor) * (zteta + ct * jumpteta);
      }
      else
        FOUR_C_THROW("Friction law is neither Tresca nor Coulomb");

      newCtxi[k] = euclidean * ztxi -
                   (frcoeff * (znor - cn * kcnode->data().getg())) * (ztxi + ct * jumptxi);
      newCteta[k] = euclidean * zteta -
                    (frcoeff * (znor - cn * kcnode->data().getg())) * (zteta + ct * jumpteta);



      // ************************************************************************
      // Extract linearizations from sparse matrix !!!
      // ************************************************************************

      // ********************************* TXI
      std::shared_ptr<Epetra_CrsMatrix> sparse_crs = linslipDISglobal.epetra_matrix();
      sparse_crs->FillComplete();
      double sparse_ij = 0.0;
      int sparsenumentries = 0;
      int sparselength = sparse_crs->NumGlobalEntries(kcnode->dofs()[1]);
      std::vector<double> sparsevalues(sparselength);
      std::vector<int> sparseindices(sparselength);
      // int sparseextractionstatus =
      sparse_crs->ExtractGlobalRowCopy(kcnode->dofs()[1], sparselength, sparsenumentries,
          sparsevalues.data(), sparseindices.data());

      for (int h = 0; h < sparselength; ++h)
      {
        if (sparseindices[h] == coldof)
        {
          sparse_ij = sparsevalues[h];
          break;
        }
        else
          sparse_ij = 0.0;
      }
      double analyt_txi = sparse_ij;

      // ********************************* TETA
      std::shared_ptr<Epetra_CrsMatrix> sparse_crs2 = linslipDISglobal.epetra_matrix();
      sparse_crs2->FillComplete();
      double sparse_2 = 0.0;
      int sparsenumentries2 = 0;
      int sparselength2 = sparse_crs2->NumGlobalEntries(kcnode->dofs()[2]);
      std::vector<double> sparsevalues2(sparselength2);
      std::vector<int> sparseindices2(sparselength2);
      // int sparseextractionstatus =
      sparse_crs->ExtractGlobalRowCopy(kcnode->dofs()[2], sparselength2, sparsenumentries2,
          sparsevalues2.data(), sparseindices2.data());

      for (int h = 0; h < sparselength2; ++h)
      {
        if (sparseindices2[h] == coldof)
        {
          sparse_2 = sparsevalues2[h];
          break;
        }
        else
          sparse_2 = 0.0;
      }
      double analyt_teta = sparse_2;

      // print results (derivatives) to screen
      if (abs(newCtxi[k] - refCtxi[k]) > 1e-12)
      {
        std::cout << "SLIP DIS-Deriv_xi: " << kcnode->id()
                  << "\t w.r.t Master: " << mnode->dofs()[fd % dim]
                  << "\t FD= " << std::setprecision(4) << (newCtxi[k] - refCtxi[k]) / delta
                  << "\t analyt= " << std::setprecision(5) << analyt_txi
                  << "\t Error= " << analyt_txi - ((newCtxi[k] - refCtxi[k]) / delta);
        if (abs(analyt_txi - (newCtxi[k] - refCtxi[k]) / delta) > 1.0e-4)
          std::cout << "*** WARNING ***" << std::endl;
        else
          std::cout << " " << std::endl;
      }

      if (abs(newCteta[k] - refCteta[k]) > 1e-12)
      {
        std::cout << "SLIP DIS-Deriv_eta: " << kcnode->id()
                  << "\t w.r.t Master: " << mnode->dofs()[fd % dim]
                  << "\t FD= " << std::setprecision(4) << (newCteta[k] - refCteta[k]) / delta
                  << "\t analyt= " << std::setprecision(5) << analyt_teta
                  << "\t Error= " << analyt_teta - ((newCteta[k] - refCteta[k]) / delta);
        if (abs(analyt_teta - (newCteta[k] - refCteta[k]) / delta) > 1.0e-4)
          std::cout << "*** WARNING ***" << std::endl;
        else
          std::cout << " " << std::endl;
      }
    }

    // undo finite difference modification
    if (fd % dim == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % dim == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }
  }

  // back to normal...
  initialize();
  evaluate();

  return;

}  // FDCheckSlipTrescaDeriv

/*----------------------------------------------------------------------*
 | Finite difference check of lagr. mult. derivatives        popp 06/09 |
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_penalty_trac_nor()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  std::cout << std::setprecision(14);

  // create storage for lm entries
  std::map<int, double> reflm;
  std::map<int, double> newlm;

  std::map<int, std::map<int, double>> deltastorage;

  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    Node* cnode = dynamic_cast<Node*>(node);

    int dim = cnode->num_dof();

    for (int d = 0; d < dim; d++)
    {
      int dof = cnode->dofs()[d];

      if ((int)(cnode->data().get_deriv_z()).size() != 0)
      {
        typedef std::map<int, double>::const_iterator CI;
        std::map<int, double>& derivzmap = cnode->data().get_deriv_z()[d];

        // print derivz-values to screen and store
        for (CI p = derivzmap.begin(); p != derivzmap.end(); ++p)
        {
          // std::cout << " (" << cnode->Dofs()[k] << ", " << p->first << ") : \t " << p->second <<
          // std::endl;
          (deltastorage[cnode->dofs()[d]])[p->first] = p->second;
        }
      }

      // store lm-values into refM
      reflm[dof] = cnode->mo_data().lm()[d];
    }
  }

  std::cout << "FINITE DIFFERENCE SOLUTION\n" << std::endl;

  int w = 0;

  // global loop to apply FD scheme to all SLAVE dofs (=3*nodes)
  for (int fd = 0; fd < 3 * snodefullmap->NumMyElements(); ++fd)
  {
    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / 3);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    int sdof = snode->dofs()[fd % 3];

    std::cout << "DEVIATION FOR S-NODE # " << gid << " DOF: " << sdof << std::endl;

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % 3 == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (fd % 3 == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // Evaluate
    evaluate();
    bool isincontact, activesetchange = false;
    assemble_reg_normal_forces(isincontact, activesetchange);

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      int dim = kcnode->num_dof();

      double fd;
      double dev;

      for (int d = 0; d < dim; d++)
      {
        int dof = kcnode->dofs()[d];

        newlm[dof] = kcnode->mo_data().lm()[d];

        fd = (newlm[dof] - reflm[dof]) / delta;

        dev = deltastorage[dof][sdof] - fd;

        if (dev)
        {
          std::cout << " (" << dof << ", " << sdof << ") :\t fd=" << fd
                    << " derivz=" << deltastorage[dof][sdof] << " DEVIATION: " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " **** WARNING ****";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " **** warning ****";
            w++;
          }
          std::cout << std::endl;

          if ((abs(dev) > 1e-2))
          {
            std::cout << " *************** ERROR *************** " << std::endl;
            // FOUR_C_THROW("un-tolerable deviation");
          }
        }
      }
    }

    // undo finite difference modification
    if (fd % 3 == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % 3 == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }
  }
  std::cout << "\n ******************** GENERATED " << w << " WARNINGS ***************** \n"
            << std::endl;

  w = 0;

  // global loop to apply FD scheme to all MASTER dofs (=3*nodes)
  for (int fd = 0; fd < 3 * mnodefullmap->NumMyElements(); ++fd)
  {
    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / 3);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    int mdof = mnode->dofs()[fd % 3];

    std::cout << "DEVIATION FOR M-NODE # " << gid << " DOF: " << mdof << std::endl;

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % 3 == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (fd % 3 == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // Evaluate
    evaluate();
    bool isincontact, activesetchange = false;
    assemble_reg_normal_forces(isincontact, activesetchange);

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!knode) FOUR_C_THROW("Cannot find node with gid %", kgid);
      Node* kcnode = dynamic_cast<Node*>(knode);

      int dim = kcnode->num_dof();

      double fd;
      double dev;

      // calculate derivative and deviation for every dof
      for (int d = 0; d < dim; d++)
      {
        int dof = kcnode->dofs()[d];

        newlm[dof] = kcnode->mo_data().lm()[d];

        fd = (newlm[dof] - reflm[dof]) / delta;

        dev = deltastorage[dof][mdof] - fd;

        if (dev)
        {
          std::cout << " (" << dof << ", " << mdof << ") :\t fd=" << fd
                    << " derivz=" << deltastorage[dof][mdof] << " DEVIATION: " << dev;

          if (abs(dev) > 1e-4)
          {
            std::cout << " **** WARNING ****";
            w++;
          }
          else if (abs(dev) > 1e-5)
          {
            std::cout << " **** warning ****";
            w++;
          }
          std::cout << std::endl;

          if ((abs(dev) > 1e-2))
          {
            std::cout << " *************** ERROR *************** " << std::endl;
            // FOUR_C_THROW("un-tolerable deviation");
          }
        }
      }
    }

    // undo finite difference modification
    if (fd % 3 == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % 3 == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }
  }
  std::cout << "\n ******************** GENERATED " << w << " WARNINGS ***************** \n"
            << std::endl;

  // back to normal...

  // Initialize
  initialize();

  // compute element areas
  set_element_areas();

  // *******************************************************************
  // contents of evaluate()
  // *******************************************************************
  evaluate();
  bool isincontact, activesetchange = false;
  assemble_reg_normal_forces(isincontact, activesetchange);

  return;
}

/*----------------------------------------------------------------------*
 | Finite difference check of frictional penalty traction     mgit 11/09|
 *----------------------------------------------------------------------*/
void CONTACT::Interface::fd_check_penalty_trac_fric()
{
  // FD checks only for serial case
  std::shared_ptr<Epetra_Map> snodefullmap = Core::LinAlg::allreduce_e_map(*snoderowmap_);
  std::shared_ptr<Epetra_Map> mnodefullmap = Core::LinAlg::allreduce_e_map(*mnoderowmap_);
  if (Core::Communication::num_mpi_ranks(get_comm()) > 1)
    FOUR_C_THROW("FD checks only for serial case");

  // information from interface contact parameter list
  double frcoeff = interface_params().get<double>("FRCOEFF");
  double ppnor = interface_params().get<double>("PENALTYPARAM");
  double pptan = interface_params().get<double>("PENALTYPARAMTAN");

  // create storage for values of complementary function C
  int nrow = snoderowmap_->NumMyElements();
  std::vector<double> reftrac1(nrow);
  std::vector<double> newtrac1(nrow);
  std::vector<double> reftrac2(nrow);
  std::vector<double> newtrac2(nrow);
  std::vector<double> reftrac3(nrow);
  std::vector<double> newtrac3(nrow);

  // store reference
  // loop over proc's slave nodes
  for (int i = 0; i < snoderowmap_->NumMyElements(); ++i)
  {
    int gid = snoderowmap_->GID(i);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    FriNode* cnode = dynamic_cast<FriNode*>(node);

    // get some information form the node
    double gap = cnode->data().getg();
    int dim = cnode->num_dof();
    double kappa = cnode->data().kappa();
    double* n = cnode->mo_data().n();

    // evaluate traction
    Core::LinAlg::SerialDenseMatrix jumpvec(dim, 1);
    Core::LinAlg::SerialDenseMatrix tanplane(dim, dim);
    std::vector<double> trailtraction(dim);
    std::vector<double> tractionold(dim);
    double magnitude = 0;

    // fill vectors and matrices
    for (int j = 0; j < dim; j++)
    {
      jumpvec(j, 0) = cnode->fri_data().jump()[j];
      tractionold[j] = cnode->fri_data().tractionold()[j];
    }

    if (dim == 3)
    {
      tanplane(0, 0) = 1 - (n[0] * n[0]);
      tanplane(0, 1) = -(n[0] * n[1]);
      tanplane(0, 2) = -(n[0] * n[2]);
      tanplane(1, 0) = -(n[1] * n[0]);
      tanplane(1, 1) = 1 - (n[1] * n[1]);
      tanplane(1, 2) = -(n[1] * n[2]);

      tanplane(2, 0) = -(n[2] * n[0]);
      tanplane(2, 1) = -(n[2] * n[1]);
      tanplane(2, 2) = 1 - (n[2] * n[2]);
    }
    else if (dim == 2)
    {
      tanplane(0, 0) = 1 - (n[0] * n[0]);
      tanplane(0, 1) = -(n[0] * n[1]);

      tanplane(1, 0) = -(n[1] * n[0]);
      tanplane(1, 1) = 1 - (n[1] * n[1]);
    }
    else
      FOUR_C_THROW("Error: Unknown dimension.");

    // Evaluate frictional trail traction
    Core::LinAlg::SerialDenseMatrix temptrac(dim, 1);
    Core::LinAlg::multiply(0.0, temptrac, kappa * pptan, tanplane, jumpvec);

    // Lagrange multiplier in normal direction
    double lmuzawan = 0.0;
    for (int j = 0; j < dim; ++j)
      lmuzawan += cnode->mo_data().lmuzawa()[j] * cnode->mo_data().n()[j];

    // Lagrange multiplier from Uzawa algorithm
    Core::LinAlg::SerialDenseMatrix lmuzawa(dim, 1);
    for (int k = 0; k < dim; ++k) lmuzawa(k, 0) = cnode->mo_data().lmuzawa()[k];

    // Lagrange multiplier in tangential direction
    Core::LinAlg::SerialDenseMatrix lmuzawatan(dim, 1);
    Core::LinAlg::multiply(lmuzawatan, tanplane, lmuzawa);

    if ((Teuchos::getIntegralValue<Inpar::CONTACT::SolvingStrategy>(
             interface_params(), "STRATEGY") == Inpar::CONTACT::solution_penalty) ||
        (Teuchos::getIntegralValue<Inpar::CONTACT::SolvingStrategy>(
             interface_params(), "STRATEGY") == Inpar::CONTACT::solution_multiscale))
    {
      for (int j = 0; j < dim; j++)
      {
        trailtraction[j] = tractionold[j] + temptrac(j, 0);
        magnitude += (trailtraction[j] * trailtraction[j]);
      }
    }
    else
    {
      for (int j = 0; j < dim; j++)
      {
        trailtraction[j] = lmuzawatan(j, 0) + temptrac(j, 0);
        magnitude += (trailtraction[j] * trailtraction[j]);
      }
    }

    // evaluate magnitude of trailtraction
    magnitude = sqrt(magnitude);

    // evaluate maximal tangential traction
    double maxtantrac = frcoeff * (lmuzawan - kappa * ppnor * gap);

    if (cnode->active() == true and cnode->fri_data().slip() == false)
    {
      reftrac1[i] = n[0] * (lmuzawan - kappa * ppnor * gap) + trailtraction[0];
      reftrac2[i] = n[1] * (lmuzawan - kappa * ppnor * gap) + trailtraction[1];
      reftrac3[i] = n[2] * (lmuzawan - kappa * ppnor * gap) + trailtraction[2];
    }
    if (cnode->active() == true and cnode->fri_data().slip() == true)
    {
      // compute lagrange multipliers and store into node
      reftrac1[i] =
          n[0] * (lmuzawan - kappa * ppnor * gap) + trailtraction[0] * maxtantrac / magnitude;
      reftrac2[i] =
          n[1] * (lmuzawan - kappa * ppnor * gap) + trailtraction[1] * maxtantrac / magnitude;
      reftrac3[i] =
          n[2] * (lmuzawan - kappa * ppnor * gap) + trailtraction[2] * maxtantrac / magnitude;
    }
  }  // loop over procs slave nodes

  // global loop to apply FD scheme to all slave dofs (=3*nodes)
  for (int fd = 0; fd < 3 * snodefullmap->NumMyElements(); ++fd)
  {
    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = snodefullmap->GID(fd / 3);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find slave node with gid %", gid);
    Node* snode = dynamic_cast<Node*>(node);

    // apply finite difference scheme
    if (Core::Communication::my_mpi_rank(get_comm()) == snode->owner())
    {
      std::cout << "\nBuilding FD for Slave Node: " << snode->id() << " Dof: " << fd % 3
                << " Dof: " << snode->dofs()[fd % 3] << std::endl;
    }

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % 3 == 0)
    {
      snode->xspatial()[0] += delta;
    }
    else if (fd % 3 == 1)
    {
      snode->xspatial()[1] += delta;
    }
    else
    {
      snode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();
    // evaluate_relative_movement();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", kgid);
      FriNode* kcnode = dynamic_cast<FriNode*>(knode);

      // get some information form the node
      double gap = kcnode->data().getg();
      int dim = kcnode->num_dof();
      double kappa = kcnode->data().kappa();
      double* n = kcnode->mo_data().n();

      // evaluate traction
      Core::LinAlg::SerialDenseMatrix jumpvec(dim, 1);
      Core::LinAlg::SerialDenseMatrix tanplane(dim, dim);
      std::vector<double> trailtraction(dim);
      std::vector<double> tractionold(dim);
      double magnitude = 0;

      // fill vectors and matrices
      for (int j = 0; j < dim; j++)
      {
        jumpvec(j, 0) = kcnode->fri_data().jump()[j];
        tractionold[j] = kcnode->fri_data().tractionold()[j];
      }

      if (dim == 3)
      {
        tanplane(0, 0) = 1 - (n[0] * n[0]);
        tanplane(0, 1) = -(n[0] * n[1]);
        tanplane(0, 2) = -(n[0] * n[2]);
        tanplane(1, 0) = -(n[1] * n[0]);
        tanplane(1, 1) = 1 - (n[1] * n[1]);
        tanplane(1, 2) = -(n[1] * n[2]);

        tanplane(2, 0) = -(n[2] * n[0]);
        tanplane(2, 1) = -(n[2] * n[1]);
        tanplane(2, 2) = 1 - (n[2] * n[2]);
      }
      else if (dim == 2)
      {
        tanplane(0, 0) = 1 - (n[0] * n[0]);
        tanplane(0, 1) = -(n[0] * n[1]);

        tanplane(1, 0) = -(n[1] * n[0]);
        tanplane(1, 1) = 1 - (n[1] * n[1]);
      }
      else
        FOUR_C_THROW("Error in AssembleTangentForces: Unknown dimension.");


      // Evaluate frictional trail traction
      Core::LinAlg::SerialDenseMatrix temptrac(dim, 1);
      Core::LinAlg::multiply(0.0, temptrac, kappa * pptan, tanplane, jumpvec);

      // Lagrange multiplier in normal direction
      double lmuzawan = 0.0;
      for (int j = 0; j < dim; ++j)
        lmuzawan += kcnode->mo_data().lmuzawa()[j] * kcnode->mo_data().n()[j];

      // Lagrange multiplier from Uzawa algorithm
      Core::LinAlg::SerialDenseMatrix lmuzawa(dim, 1);
      for (int j = 0; j < dim; ++j) lmuzawa(j, 0) = kcnode->mo_data().lmuzawa()[j];

      // Lagrange multiplier in tangential direction
      Core::LinAlg::SerialDenseMatrix lmuzawatan(dim, 1);
      Core::LinAlg::multiply(lmuzawatan, tanplane, lmuzawa);

      const auto contact_strategy = Teuchos::getIntegralValue<Inpar::CONTACT::SolvingStrategy>(
          interface_params(), "STRATEGY");

      if ((contact_strategy == Inpar::CONTACT::solution_penalty) ||
          (contact_strategy == Inpar::CONTACT::solution_multiscale))
      {
        for (int j = 0; j < dim; j++)
        {
          trailtraction[j] = tractionold[j] + temptrac(j, 0);
          magnitude += (trailtraction[j] * trailtraction[j]);
        }
      }
      else
      {
        for (int j = 0; j < dim; j++)
        {
          trailtraction[j] = lmuzawatan(j, 0) + temptrac(j, 0);
          magnitude += (trailtraction[j] * trailtraction[j]);
        }
      }

      // evaluate magnitude of trailtraction
      magnitude = sqrt(magnitude);

      // evaluate maximal tangential traction
      double maxtantrac = frcoeff * (lmuzawan - kappa * ppnor * gap);

      if (kcnode->active() == true and kcnode->fri_data().slip() == false)
      {
        newtrac1[k] = n[0] * (lmuzawan - kappa * ppnor * gap) + trailtraction[0];
        newtrac2[k] = n[1] * (lmuzawan - kappa * ppnor * gap) + trailtraction[1];
        newtrac3[k] = n[2] * (lmuzawan - kappa * ppnor * gap) + trailtraction[2];
      }
      if (kcnode->active() == true and kcnode->fri_data().slip() == true)
      {
        // compute lagrange multipliers and store into node
        newtrac1[k] =
            n[0] * (lmuzawan - kappa * ppnor * gap) + trailtraction[0] * maxtantrac / magnitude;
        newtrac2[k] =
            n[1] * (lmuzawan - kappa * ppnor * gap) + trailtraction[1] * maxtantrac / magnitude;
        newtrac3[k] =
            n[2] * (lmuzawan - kappa * ppnor * gap) + trailtraction[2] * maxtantrac / magnitude;
      }

      // print results (derivatives) to screen
      if (abs(newtrac1[k] - reftrac1[k]) > 1e-12)
      {
        // std::cout << "SlipCon-FD-derivative for node S" << kcnode->Id() << std::endl;
        // std::cout << "Ref-G: " << refG[k] << std::endl;
        // std::cout << "New-G: " << newG[k] << std::endl;
        std::cout << "Deriv0:      " << kcnode->dofs()[0] << " " << snode->dofs()[fd % 3] << " "
                  << (newtrac1[k] - reftrac1[k]) / delta << std::endl;
        // std::cout << "Analytical: " << snode->Dofs()[fd%3] << " " <<
        // kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]] << std::endl; if
        // (abs(kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]]-(newG[k]-refG[k])/delta)>1.0e-5)
        //  std::cout <<
        //  "***WARNING*****************************************************************************"
        //  << std::endl;
      }

      // print results (derivatives) to screen
      if (abs(newtrac2[k] - reftrac2[k]) > 1e-12)
      {
        // std::cout << "SlipCon-FD-derivative for node S" << kcnode->Id() << std::endl;
        // std::cout << "Ref-G: " << refG[k] << std::endl;
        // std::cout << "New-G: " << newG[k] << std::endl;
        std::cout << "Deriv1:      " << kcnode->dofs()[1] << " " << snode->dofs()[fd % 3] << " "
                  << (newtrac2[k] - reftrac2[k]) / delta << std::endl;
        // std::cout << "Analytical: " << snode->Dofs()[fd%3] << " " <<
        // kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]] << std::endl; if
        // (abs(kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]]-(newG[k]-refG[k])/delta)>1.0e-5)
        //  std::cout <<
        //  "***WARNING*****************************************************************************"
        //  << std::endl;
      }

      // print results (derivatives) to screen
      if (abs(newtrac3[k] - reftrac3[k]) > 1e-12)
      {
        // std::cout << "SlipCon-FD-derivative for node S" << kcnode->Id() << std::endl;
        // std::cout << "Ref-G: " << refG[k] << std::endl;
        // std::cout << "New-G: " << newG[k] << std::endl;
        std::cout << "Deriv2:      " << kcnode->dofs()[2] << " " << snode->dofs()[fd % 3] << " "
                  << (newtrac3[k] - reftrac3[k]) / delta << std::endl;
        // std::cout << "Analytical: " << snode->Dofs()[fd%3] << " " <<
        // kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]] << std::endl; if
        // (abs(kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]]-(newG[k]-refG[k])/delta)>1.0e-5)
        //  std::cout <<
        //  "***WARNING*****************************************************************************"
        //  << std::endl;
      }
    }
    // undo finite difference modification
    if (fd % 3 == 0)
    {
      snode->xspatial()[0] -= delta;
    }
    else if (fd % 3 == 1)
    {
      snode->xspatial()[1] -= delta;
    }
    else
    {
      snode->xspatial()[2] -= delta;
    }
  }  // loop over procs slave nodes

  // global loop to apply FD scheme to all master dofs (=3*nodes)
  for (int fd = 0; fd < 3 * mnodefullmap->NumMyElements(); ++fd)
  {
    // Initialize
    initialize();

    // now get the node we want to apply the FD scheme to
    int gid = mnodefullmap->GID(fd / 3);
    Core::Nodes::Node* node = idiscret_->g_node(gid);
    if (!node) FOUR_C_THROW("Cannot find master node with gid %", gid);
    Node* mnode = dynamic_cast<Node*>(node);

    // apply finite difference scheme
    if (Core::Communication::my_mpi_rank(get_comm()) == mnode->owner())
    {
      std::cout << "\nBuilding FD for Master Node: " << mnode->id() << " Dof: " << fd % 3
                << " Dof: " << mnode->dofs()[fd % 3] << std::endl;
    }

    // do step forward (modify nodal displacement)
    double delta = 1e-8;
    if (fd % 3 == 0)
    {
      mnode->xspatial()[0] += delta;
    }
    else if (fd % 3 == 1)
    {
      mnode->xspatial()[1] += delta;
    }
    else
    {
      mnode->xspatial()[2] += delta;
    }

    // compute element areas
    set_element_areas();

    // *******************************************************************
    // contents of evaluate()
    // *******************************************************************
    evaluate();
    // evaluate_relative_movement();

    // compute finite difference derivative
    for (int k = 0; k < snoderowmap_->NumMyElements(); ++k)
    {
      int kgid = snoderowmap_->GID(k);
      Core::Nodes::Node* knode = idiscret_->g_node(kgid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", kgid);
      FriNode* kcnode = dynamic_cast<FriNode*>(knode);

      // get some information form the node
      double gap = kcnode->data().getg();
      int dim = kcnode->num_dof();
      double kappa = kcnode->data().kappa();
      double* n = kcnode->mo_data().n();

      // evaluate traction
      Core::LinAlg::SerialDenseMatrix jumpvec(dim, 1);
      Core::LinAlg::SerialDenseMatrix tanplane(dim, dim);
      std::vector<double> trailtraction(dim);
      std::vector<double> tractionold(dim);
      double magnitude = 0;

      // fill vectors and matrices
      for (int j = 0; j < dim; j++)
      {
        jumpvec(j, 0) = kcnode->fri_data().jump()[j];
        tractionold[j] = kcnode->fri_data().tractionold()[j];
      }

      if (dim == 3)
      {
        tanplane(0, 0) = 1 - (n[0] * n[0]);
        tanplane(0, 1) = -(n[0] * n[1]);
        tanplane(0, 2) = -(n[0] * n[2]);
        tanplane(1, 0) = -(n[1] * n[0]);
        tanplane(1, 1) = 1 - (n[1] * n[1]);
        tanplane(1, 2) = -(n[1] * n[2]);

        tanplane(2, 0) = -(n[2] * n[0]);
        tanplane(2, 1) = -(n[2] * n[1]);
        tanplane(2, 2) = 1 - (n[2] * n[2]);
      }
      else if (dim == 2)
      {
        tanplane(0, 0) = 1 - (n[0] * n[0]);
        tanplane(0, 1) = -(n[0] * n[1]);

        tanplane(1, 0) = -(n[1] * n[0]);
        tanplane(1, 1) = 1 - (n[1] * n[1]);
      }
      else
        FOUR_C_THROW("Error in AssembleTangentForces: Unknown dimension.");

      // Evaluate frictional trail traction
      Core::LinAlg::SerialDenseMatrix temptrac(dim, 1);
      Core::LinAlg::multiply(0.0, temptrac, kappa * pptan, tanplane, jumpvec);

      // Lagrange multiplier in normal direction
      double lmuzawan = 0.0;
      for (int j = 0; j < dim; ++j)
        lmuzawan += kcnode->mo_data().lmuzawa()[j] * kcnode->mo_data().n()[j];

      // Lagrange multiplier from Uzawa algorithm
      Core::LinAlg::SerialDenseMatrix lmuzawa(dim, 1);
      for (int j = 0; j < dim; ++j) lmuzawa(j, 0) = kcnode->mo_data().lmuzawa()[j];

      // Lagrange multiplier in tangential direction
      Core::LinAlg::SerialDenseMatrix lmuzawatan(dim, 1);
      Core::LinAlg::multiply(lmuzawatan, tanplane, lmuzawa);

      if ((Teuchos::getIntegralValue<Inpar::CONTACT::SolvingStrategy>(
               interface_params(), "STRATEGY") == Inpar::CONTACT::solution_penalty) ||
          (Teuchos::getIntegralValue<Inpar::CONTACT::SolvingStrategy>(
               interface_params(), "STRATEGY") == Inpar::CONTACT::solution_multiscale))
      {
        for (int j = 0; j < dim; j++)
        {
          trailtraction[j] = tractionold[j] + temptrac(j, 0);
          magnitude += (trailtraction[j] * trailtraction[j]);
        }
      }
      else
      {
        for (int j = 0; j < dim; j++)
        {
          trailtraction[j] = lmuzawatan(j, 0) + temptrac(j, 0);
          magnitude += (trailtraction[j] * trailtraction[j]);
        }
      }

      // evaluate magnitude of trailtraction
      magnitude = sqrt(magnitude);

      // evaluate maximal tangential traction
      double maxtantrac = frcoeff * (lmuzawan - kappa * ppnor * gap);

      if (kcnode->active() == true and kcnode->fri_data().slip() == false)
      {
        newtrac1[k] = n[0] * (lmuzawan - kappa * ppnor * gap) + trailtraction[0];
        newtrac2[k] = n[1] * (lmuzawan - kappa * ppnor * gap) + trailtraction[1];
        newtrac3[k] = n[2] * (lmuzawan - kappa * ppnor * gap) + trailtraction[2];
      }
      if (kcnode->active() == true and kcnode->fri_data().slip() == true)
      {
        // compute lagrange multipliers and store into node
        newtrac1[k] =
            n[0] * (lmuzawan - kappa * ppnor * gap) + trailtraction[0] * maxtantrac / magnitude;
        newtrac2[k] =
            n[1] * (lmuzawan - kappa * ppnor * gap) + trailtraction[1] * maxtantrac / magnitude;
        newtrac3[k] =
            n[2] * (lmuzawan - kappa * ppnor * gap) + trailtraction[2] * maxtantrac / magnitude;
      }

      // print results (derivatives) to screen
      if (abs(newtrac1[k] - reftrac1[k]) > 1e-12)
      {
        // std::cout << "SlipCon-FD-derivative for node S" << kcnode->Id() << std::endl;
        // std::cout << "Ref-G: " << refG[k] << std::endl;
        // std::cout << "New-G: " << newG[k] << std::endl;
        std::cout << "Deriv:      " << kcnode->dofs()[0] << " " << mnode->dofs()[fd % 3] << " "
                  << (newtrac1[k] - reftrac1[k]) / delta << std::endl;
        // std::cout << "Analytical: " << snode->Dofs()[fd%3] << " " <<
        // kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]] << std::endl; if
        // (abs(kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]]-(newG[k]-refG[k])/delta)>1.0e-5)
        //  std::cout <<
        //  "***WARNING*****************************************************************************"
        //  << std::endl;
      }

      // print results (derivatives) to screen
      if (abs(newtrac2[k] - reftrac2[k]) > 1e-12)
      {
        // std::cout << "SlipCon-FD-derivative for node S" << kcnode->Id() << std::endl;
        // std::cout << "Ref-G: " << refG[k] << std::endl;
        // std::cout << "New-G: " << newG[k] << std::endl;
        std::cout << "Deriv:      " << kcnode->dofs()[1] << " " << mnode->dofs()[fd % 3] << " "
                  << (newtrac2[k] - reftrac2[k]) / delta << std::endl;
        // std::cout << "Analytical: " << snode->Dofs()[fd%3] << " " <<
        // kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]] << std::endl; if
        // (abs(kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]]-(newG[k]-refG[k])/delta)>1.0e-5)
        //  std::cout <<
        //  "***WARNING*****************************************************************************"
        //  << std::endl;
      }

      // print results (derivatives) to screen
      if (abs(newtrac3[k] - reftrac3[k]) > 1e-12)
      {
        // std::cout << "SlipCon-FD-derivative for node S" << kcnode->Id() << std::endl;
        // std::cout << "Ref-G: " << refG[k] << std::endl;
        // std::cout << "New-G: " << newG[k] << std::endl;
        std::cout << "Deriv:      " << kcnode->dofs()[2] << " " << mnode->dofs()[fd % 3] << " "
                  << (newtrac3[k] - reftrac3[k]) / delta << std::endl;
        // std::cout << "Analytical: " << snode->Dofs()[fd%3] << " " <<
        // kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]] << std::endl; if
        // (abs(kcnode->Data().GetDerivG()[snode->Dofs()[fd%3]]-(newG[k]-refG[k])/delta)>1.0e-5)
        //  std::cout <<
        //  "***WARNING*****************************************************************************"
        //  << std::endl;
      }
    }
    // undo finite difference modification
    if (fd % 3 == 0)
    {
      mnode->xspatial()[0] -= delta;
    }
    else if (fd % 3 == 1)
    {
      mnode->xspatial()[1] -= delta;
    }
    else
    {
      mnode->xspatial()[2] -= delta;
    }
  }

  // back to normal...
  initialize();
  evaluate();
  // evaluate_relative_movement();

  return;
}  // FDCheckPenaltyFricTrac

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::Interface::write_nodal_coordinates_to_file(
    const int interfacel_id, const Epetra_Map& nodal_map, const std::string& full_path) const
{
  // only processor zero writes header
  if (Core::Communication::my_mpi_rank(get_comm()) == 0 and interfacel_id == 0)
  {
    std::ofstream of(full_path, std::ios_base::out);
    of << std::setw(7) << "ID" << std::setw(24) << "x" << std::setw(24) << "y" << std::setw(24)
       << "z" << std::setw(24) << "X" << std::setw(24) << "Y" << std::setw(24) << "Z\n";
    of.close();
  }

  Core::Communication::barrier(get_comm());

  for (int p = 0; p < Core::Communication::num_mpi_ranks(get_comm()); ++p)
  {
    if (p == Core::Communication::my_mpi_rank(get_comm()))
    {
      // open the output stream again on all procs
      std::ofstream of(full_path, std::ios_base::out | std::ios_base::app);

      const int* nodal_map_gids = nodal_map.MyGlobalElements();
      const unsigned nummyeles = nodal_map.NumMyElements();
      for (unsigned lid = 0; lid < nummyeles; ++lid)
      {
        const int gid = nodal_map_gids[lid];

        if (not idiscret_->node_row_map()->MyGID(gid)) continue;

        const Node& cnode = dynamic_cast<const Node&>(*idiscret_->g_node(gid));

        of << std::setw(7) << cnode.id();
        of << std::setprecision(16);
        of << std::setw(24) << std::scientific << cnode.xspatial()[0] << std::setw(24)
           << std::scientific << cnode.xspatial()[1] << std::setw(24) << std::scientific
           << cnode.xspatial()[2] << std::setw(24) << std::scientific << cnode.x()[0]
           << std::setw(24) << std::scientific << cnode.x()[1] << std::setw(24) << std::scientific
           << cnode.x()[2] << "\n";
      }
      of.close();
    }
    Core::Communication::barrier(get_comm());
  }
}

FOUR_C_NAMESPACE_CLOSE
