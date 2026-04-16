/*--------------------------------------------------------------------
 *    The MB-system:  glb_writer.h  4/15/2026
 *
 *    Copyright (c) 2026 by
 *    David W. Caress (caress@mbari.org)
 *      Monterey Bay Aquarium Research Institute
 *      Moss Landing, California, USA
 *
 *    See README.md file for copying and redistribution conditions.
 *--------------------------------------------------------------------*/
/**
 * @file glb_writer.h
 * @brief Write 3D mesh to GLB (binary glTF) format using tinygltf
 */

#ifndef GLB_WRITER_H
#define GLB_WRITER_H

#include "mesh_generator.h"
#include <string>

/**
 * @brief Write mesh to GLB file
 * 
 * @param mesh The mesh to write
 * @param filename Output GLB filename
 * @param verbose Print progress messages
 * @return 0 on success, -1 on error
 */
int write_glb_file(const Mesh& mesh, const std::string& filename, int verbose = 0);

#endif // GLB_WRITER_H
