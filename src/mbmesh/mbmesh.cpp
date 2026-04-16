/*--------------------------------------------------------------------
 *    The MB-system:  mbmesh.cc  3/6/2026
 *
 *    Copyright (c) 2026 by
 *    David W. Caress (caress@mbari.org)
 *      Monterey Bay Aquarium Research Institute
 *      Moss Landing, California, USA
 *    Dale N. Chayes
 *      Center for Coastal and Ocean Mapping
 *      University of New Hampshire
 *      Durham, New Hampshire, USA
 *    Christian dos Santos Ferreira
 *      MARUM
 *      University of Bremen
 *      Bremen Germany
 *
 *    MB-System was created by Caress and Chayes in 1992 at the
 *      Lamont-Doherty Earth Observatory
 *      Columbia University
 *      Palisades, NY 10964
 *
 *    See README.md file for copying and redistribution conditions.
 *--------------------------------------------------------------------*/
/**
 * @file mbmesh.cc
 * @brief Generate 3D Tiles from swath bathymetry data
 *
 * mbmesh reads swath sonar data files and generates OGC 3D Tiles
 * for visualization of bathymetric data with full 3D structure.
 * This preserves features like cliffs, overhangs, and caves that
 * are lost in traditional 2D gridding.
 *
 * Author:  CSUMB Capstone - Spring 2026
 * Date:    March 6, 2026
 */

/*--------------------------------------------------------------------
 * PHASE 1 SCOPE:
 * This initial implementation focuses ONLY on:
 * 1. Reading datalist files
 * 2. Opening swath files with MB-System API
 * 3. Extracting bathymetry soundings
 * 4. Storing soundings in memory
 * 5. Printing statistics
 *
 * NOT INCLUDED YET:
 * - Spatial indexing (quadtree/octree)
 * - Mesh generation
 * - 3D Tiles output
 * - ECEF coordinate conversion
 *--------------------------------------------------------------------*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <algorithm>

// Mesh generation and GLB export
#include "mesh_generator.h"
#include "glb_writer.h"

// MB-System includes
extern "C" {
#include "mb_define.h"
#include "mb_format.h"
#include "mb_io.h"
#include "mb_status.h"
}

constexpr char program_name[] = "mbmesh";
constexpr char help_message[] =
    "mbmesh generates 3D Tiles from swath sonar bathymetry data.\n"
    "This tool reads swath data files and produces OGC 3D Tiles format\n"
    "output suitable for web-based 3D visualization.\n\n"
    "Phase 1 implementation reads and validates swath data input.";

constexpr char usage_message[] =
    "mbmesh -Idatalist [-Rwest/east/south/north] [-Ooutdir] [-Ddownsample]\n"
    "       [-V -H]";

/*--------------------------------------------------------------------*/
/* SOUNDING STRUCTURE */
/*--------------------------------------------------------------------*/

/**
 * @brief A single sonar sounding (one beam return)
 *
 * This structure represents one bathymetry measurement from the
 * multibeam sonar. For Phase 1, we just store the basic information
 * needed to validate the data input pipeline.
 */
struct Sounding {
  double longitude;   // Degrees east
  double latitude;    // Degrees north
  double depth;       // Meters (negative = below sea level)
  double ecef_x;      // ECEF X coordinate (meters)
  double ecef_y;      // ECEF Y coordinate (meters)
  double ecef_z;      // ECEF Z coordinate (meters)
  char beamflag;      // MB-System beam quality flag
  int beam_number;    // Beam index within ping
  double time_d;      // Unix timestamp (seconds since epoch)

  // TODO Phase 4: Add ECEF coordinates for 3D Tiles (x, y, z)
  // TODO Phase 3: Add amplitude, acrosstrack, alongtrack for mesh quality
};

/*--------------------------------------------------------------------*/
/* GLOBAL VARIABLES */
/*--------------------------------------------------------------------*/

// Command-line options
static int verbose = 0;
static char read_datalist[MB_PATH_MAXLINE] = "datalist.mb-1";
static char output_dir[MB_PATH_MAXLINE] = "./tileset";
static bool bounds_specified = false;
static double bounds[4] = {-180.0, 180.0, -90.0, 90.0};  // west, east, south, north
static int downsample_factor = 1;  // 1 = no downsampling, N = keep 1 in N points

// Statistics
static int nfile = 0;               // Number of files in datalist
static int nfile_read = 0;          // Number successfully read
static int npings = 0;              // Total pings processed
static int nbeams_total = 0;        // Total beams encountered
static int nbeams_good = 0;         // Valid beams
static int nbeams_flagged = 0;      // Flagged/rejected beams
static uint64_t nbeams_in_bounds = 0; // Good beams that pass geographic bounds

// Storage for soundings (in-memory for Phase 1)
static std::vector<Sounding> all_soundings;

/*--------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES */
/*--------------------------------------------------------------------*/

static void print_help();
static int parse_options(int argc, char **argv);
static int read_datalist_file(int verbose);
static int read_swath_file(int verbose, char *file, int format, double file_weight);
static int process_ping(int verbose, int beams_bath, char *beamflag,
                       double *bath, double *bathlon, double *bathlat,
                       double navlon, double navlat, double heading,
                       double time_d);
static int write_xyz_file(const char *filename);
static void print_statistics();
static int ensure_directory_exists(const char *path);
static void geodetic_to_ecef(double lon_deg, double lat_deg, double height_m,
                             double *x_m, double *y_m, double *z_m);
static void local_offsets_to_geodetic(double ref_lon_deg, double ref_lat_deg,
                                      double east_m, double north_m,
                                      double *lon_deg, double *lat_deg);

// WGS84 constants used for the local geodetic-to-ECEF conversion.
constexpr double wgs84_a = 6378137.0;
constexpr double wgs84_f = 1.0 / 298.257223563;
constexpr double wgs84_e2 = wgs84_f * (2.0 - wgs84_f);
constexpr double degrees_to_radians = M_PI / 180.0;

/*--------------------------------------------------------------------*/
/* MAIN FUNCTION */
/*--------------------------------------------------------------------*/

int main(int argc, char **argv) {
  fprintf(stderr, "\nProgram %s\n", program_name);
  fprintf(stderr, "MB-system Version %s\n", MB_VERSION);

  /* Parse command-line options */
  if (parse_options(argc, argv) != MB_SUCCESS) {
    fprintf(stderr, "\nProgram <%s> Terminated\n", program_name);
    exit(MB_ERROR_BAD_USAGE);
  }

  /* Print starting info */
  if (verbose > 0) {
    fprintf(stderr, "\nmbmesh settings:\n");
    fprintf(stderr, "  Input datalist: %s\n", read_datalist);
    fprintf(stderr, "  Output directory: %s\n", output_dir);
    if (bounds_specified) {
      fprintf(stderr, "  Geographic bounds: %.6f/%.6f/%.6f/%.6f\n",
              bounds[0], bounds[1], bounds[2], bounds[3]);
    } else {
      fprintf(stderr, "  Geographic bounds: [unbounded]\n");
    }
    fprintf(stderr, "  Verbose level: %d\n", verbose);
  }

  /* Read swath data from datalist */
  fprintf(stderr, "\n=== Phase 1: Reading Swath Data ===\n");
  int status = read_datalist_file(verbose);

  if (status != MB_SUCCESS) {
    fprintf(stderr, "\nError reading datalist\n");
    fprintf(stderr, "Program <%s> Terminated\n", program_name);
    exit(status);
  }

  /* Print statistics */
  fprintf(stderr, "\nSwath data reading complete\n");
  print_statistics();

  if (ensure_directory_exists(output_dir) != MB_SUCCESS) {
    fprintf(stderr, "\nProgram <%s> Terminated\n", program_name);
    exit(MB_FAILURE);
  }

  /* Write XYZ point cloud */
  char xyz_file[MB_PATH_MAXLINE];
  snprintf(xyz_file, sizeof(xyz_file), "%s/pointcloud.xyz", output_dir);
  fprintf(stderr, "\nConverting soundings to ECEF coordinates...\n");
  write_xyz_file(xyz_file);

  fprintf(stderr, "\n=== Phase 1 Complete ===\n");
  fprintf(stderr, "Soundings collected: %zu\n", all_soundings.size());
  fprintf(stderr, "XYZ file written: %s\n", xyz_file);

  /* Phase 2-3: Generate mesh from point cloud and write GLB */
  if (!all_soundings.empty()) {
    /* Convert soundings to flat coordinate array for mesh generation */
    std::vector<double> point_cloud;
    point_cloud.reserve(all_soundings.size() * 3);
    for (const auto& s : all_soundings) {
      point_cloud.push_back(s.ecef_x);
      point_cloud.push_back(s.ecef_y);
      point_cloud.push_back(s.ecef_z);
    }
    
    /* Generate mesh using greedy nearest-neighbor approach */
    Mesh mesh = generate_greedy_mesh(point_cloud, 6, verbose);

    if (mesh.vertices.empty() || mesh.triangles.empty()) {
      fprintf(stderr, "Mesh generation did not produce a valid surface; skipping GLB export.\n");
    } else {
    
      /* Write mesh to GLB file */
      char glb_file[MB_PATH_MAXLINE];
      snprintf(glb_file, sizeof(glb_file), "%s/mesh.glb", output_dir);
      int glb_status = write_glb_file(mesh, glb_file, verbose);

      if (glb_status == 0 && verbose > 0) {
        fprintf(stderr, "GLB file successfully written: %s\n", glb_file);
      } else if (glb_status != 0) {
        fprintf(stderr, "Failed to write GLB file: %s\n", glb_file);
      }
    }
  }

  /* TODO Phase 4: Build spatial index (octree) for multiple LOD tiles */
  /* TODO Phase 5: Write OGC 3D Tiles tileset.json + multiple .glb files */

  fprintf(stderr, "\nReady for Phase 4 (spatial indexing for LOD)\n");
  fprintf(stderr, "\nProgram <%s> completed successfully\n", program_name);
  exit(MB_SUCCESS);
}

/*--------------------------------------------------------------------*/
/* PARSE COMMAND-LINE OPTIONS */
/*--------------------------------------------------------------------*/

static int parse_options(int argc, char **argv) {
  int option_index;
  int errflg = 0;
  int c;
  bool help = false;

  static struct option long_options[] = {
      {"verbose", no_argument, nullptr, 0},
      {"help", no_argument, nullptr, 0},
      {"input", required_argument, nullptr, 0},
      {nullptr, 0, nullptr, 0}};

  /* Process command line options */
  while ((c = getopt_long(argc, argv, "I:O:R:D:VvHh", long_options, &option_index)) != -1) {
    switch (c) {
    case 0:
      /* Handle long options */
      break;

    case 'I':
      sscanf(optarg, "%s", read_datalist);
      break;

    case 'O':
      sscanf(optarg, "%s", output_dir);
      break;

    case 'R':
      /* Parse bounds: west/east/south/north */
      {
        int n = sscanf(optarg, "%lf/%lf/%lf/%lf",
                      &bounds[0], &bounds[1], &bounds[2], &bounds[3]);
        if (n == 4) {
          bounds_specified = true;
        } else {
          fprintf(stderr, "Error parsing -R option: %s\n", optarg);
          fprintf(stderr, "Expected format: -Rwest/east/south/north\n");
          errflg++;
        }
      }
      break;

    case 'D':
      sscanf(optarg, "%d", &downsample_factor);
      if (downsample_factor < 1) downsample_factor = 1;
      break;

    case 'V':
    case 'v':
      verbose++;
      break;

    case 'H':
    case 'h':
      help = true;
      break;

    case '?':
      errflg++;
      break;
    }
  }

  if (errflg || help) {
    print_help();
    return MB_FAILURE;
  }

  return MB_SUCCESS;
}

static int ensure_directory_exists(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      return MB_SUCCESS;
    }

    fprintf(stderr, "Error: Output path exists but is not a directory: %s\n", path);
    return MB_FAILURE;
  }

  if (mkdir(path, 0755) == 0) {
    if (verbose > 0) {
      fprintf(stderr, "Created output directory: %s\n", path);
    }
    return MB_SUCCESS;
  }

  if (errno == EEXIST) {
    return MB_SUCCESS;
  }

  fprintf(stderr, "Error: Cannot create output directory %s: %s\n", path, strerror(errno));
  return MB_FAILURE;
}

/*--------------------------------------------------------------------*/
/* PRINT HELP MESSAGE */
/*--------------------------------------------------------------------*/

static void print_help() {
  fprintf(stderr, "\n%s\n", help_message);
  fprintf(stderr, "\nusage: %s\n", usage_message);
  fprintf(stderr, "\nRequired:\n");
  fprintf(stderr, "  -I<datalist>       Input datalist file [datalist.mb-1]\n");
  fprintf(stderr, "\nOptional:\n");
  fprintf(stderr, "  -O<outputdir>      Output directory [./tileset]\n");
  fprintf(stderr, "  -R<w/e/s/n>        Geographic bounds (degrees)\n");
  fprintf(stderr, "  -D<factor>         Downsample good beams by keeping 1 in N [1]\n");
  fprintf(stderr, "  -V                 Increase verbosity (can repeat: -V -V)\n");
  fprintf(stderr, "  -H                 Print this help message\n");
  fprintf(stderr, "\nPhase 1 Implementation:\n");
  fprintf(stderr, "  This version reads swath data and validates input only.\n");
  fprintf(stderr, "  Spatial indexing and 3D Tiles output coming in Phase 2.\n");
  fprintf(stderr, "\nExample:\n");
  fprintf(stderr, "  mbmesh -Idatalist.mb-1 -R-122.5/-121.8/36.5/37.2 -V\n\n");
}

/*--------------------------------------------------------------------*/
/* READ DATALIST FILE */
/*--------------------------------------------------------------------*/

/**
 * @brief Read datalist and process all swath files
 *
 * This function follows the pattern from mbgrid.cc lines 1800-2100.
 * It opens the datalist, iterates through each file entry, and
 * calls read_swath_file() for each valid swath file.
 *
 * @param verbose Verbosity level
 * @return MB_SUCCESS or error code
 */
static int read_datalist_file(int verbose) {
  void *datalist = nullptr;
  int look_processed = MB_DATALIST_LOOK_UNSET;
  int error = MB_ERROR_NO_ERROR;

  /* Open datalist */
  int status = mb_datalist_open(verbose, &datalist, read_datalist,
                                look_processed, &error);
  if (status != MB_SUCCESS) {
    fprintf(stderr, "\nUnable to open datalist file: %s\n", read_datalist);
    fprintf(stderr, "Error: %d\n", error);
    return status;
  }

  if (verbose > 0) {
    fprintf(stderr, "Datalist opened: %s\n", read_datalist);
  }

  
  /* Variables for mb_datalist_read3() */
  int pstatus = MB_PROCESSED_NONE; // Indicates whether to use raw or processed file
  int astatus = MB_ALTNAV_NONE; // Indicates whether alternative navigation is available
  char path[MB_PATH_MAXLINE] = ""; // Raw file path
  char ppath[MB_PATH_MAXLINE] = ""; // Processed file path (if available)
  char apath[MB_PATH_MAXLINE] = ""; // Alternative navigation file path (if available)
  char dpath[MB_PATH_MAXLINE] = ""; // Optional data file path (not used in this project)
  int format = 0; // MB-System format code
  double file_weight = 1.0; // Weight for this file (usually 1.0)

  // int mb_datalist_read3(int verbose, void *datalist,
  //                       int *pstatus, char *path, char *ppath,
  //                       int *astatus, char *apath, char *dpath,
  //                       int *format, double *file_weight, int *error);

  while (mb_datalist_read3(verbose, datalist,
                           &pstatus, path, ppath,
                           &astatus, apath, dpath,
                           &format, &file_weight, &error) == MB_SUCCESS) {
    // Skip non-swath files (format <= 0)
    if (format <= 0) continue;

    // Skip comment lines
    if (path[0] == '#') continue;

    // Choose raw or processed file
    char *file_to_read = (pstatus == MB_PROCESSED_USE) ? ppath : path;

    // Count files
    nfile++;

    // Read this file
    if (verbose > 0) {
      fprintf(stderr, "\nProcessing file %d: %s\n", nfile, file_to_read);
    }

    status = read_swath_file(verbose, file_to_read, format, file_weight);

    if (status != MB_SUCCESS) {
      fprintf(stderr, "Warning: Failed to read file: %s\n", file_to_read);
      // Continue with next file
    }
  }

  /* Close datalist */
  mb_datalist_close(verbose, &datalist, &error);

  return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* READ SINGLE SWATH FILE */
/*--------------------------------------------------------------------*/

/**
 * @brief Read bathymetry data from a single swath file
 *
 * This function follows the pattern from mbgrid.cc lines 2600-2800.
 * It initializes MB-System I/O, reads pings in a loop, and extracts
 * bathymetry soundings from each ping.
 *
 * @param verbose Verbosity level
 * @param file Path to swath file
 * @param format MB-System format code
 * @param file_weight Weight for this file (usually 1.0)
 * @return MB_SUCCESS or error code
 */
static int read_swath_file(int verbose, char *file, int format,
                          double file_weight) {
  (void)file_weight;
  if (verbose > 0) {
    fprintf(stderr, "  Opening file (format %d)...\n", format);
  }

  /* MB-System I/O variables */
  void *mbio_ptr = nullptr;
  void *store_ptr = nullptr;
  int error = MB_ERROR_NO_ERROR;

  /* Data arrays (will be allocated after mb_read_init) */
  char *beamflag = nullptr;
  double *bath = nullptr;
  double *bathacrosstrack = nullptr;
  double *bathalongtrack = nullptr;
  double *amp = nullptr;
  double *ss = nullptr;
  double *ssacrosstrack = nullptr;
  double *ssalongtrack = nullptr;
  char comment[MB_COMMENT_MAXLINE] = "";
  //int kind, time_i[7];
  //double time_d, navlon, navlat, speed, heading, distance, altitude, sensordepth;

  /* Ping data variables */
  int kind;
  int time_i[7];
  double time_d;
  double navlon, navlat;
  double speed, heading;
  double distance, altitude, sensordepth;
  int beams_bath, beams_amp, pixels_ss;

  // Initializing MB-System I/O and reading pings will be implemented in the next steps.
  //REFERENCE: See mbgrid.cc lines 2630-2650 for example

  // Time limits: full range = no filtering,]
  // full range of valid times referenced from mb_time.cc lines 72-77
  int btime_i[7] = {1930, 1, 1, 0, 0, 0, 0};
  int etime_i[7] = {3000, 1, 1, 0, 0, 0, 0};
  double btime_d, etime_d;

  int status = mb_read_init(verbose, file, format, 1, 0, bounds,
                            btime_i, etime_i, 0.0, 1.0,
                            &mbio_ptr, &btime_d, &etime_d,
                            &beams_bath, &beams_amp, &pixels_ss,
                            &error);

  // if mb_read_init fails, print error message and return failure
  if (status != MB_SUCCESS) {
    char *message = nullptr;
    mb_error(verbose, error, &message);
    fprintf(stderr, "  Error initializing file: %s\n", message);
    return MB_FAILURE;
  }
  // if mb_read_init succeeds, print number of beams and pixels
  if (verbose > 0) {
    fprintf(stderr, "  File opened: %d beams, %d amp, %d ss\n",
            beams_bath, beams_amp, pixels_ss);
  }

  // Register arrays with MB-System I/O system
  // This is CRITICAL - arrays must be registered before mb_read/mb_get_all calls
  status = mb_register_array(verbose, mbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(char),
                            (void **)&beamflag, &error);
  if (status != MB_SUCCESS) {
    fprintf(stderr, "Error registering beamflag array\n");
    mb_close(verbose, &mbio_ptr, &error);
    return MB_FAILURE;
  }

  status = mb_register_array(verbose, mbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                            (void **)&bath, &error);
  if (status != MB_SUCCESS) {
    fprintf(stderr, "Error registering bath array\n");
    mb_close(verbose, &mbio_ptr, &error);
    return MB_FAILURE;
  }

  status = mb_register_array(verbose, mbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                            (void **)&bathacrosstrack, &error);
  if (status != MB_SUCCESS) {
    fprintf(stderr, "Error registering bathacrosstrack array\n");
    mb_close(verbose, &mbio_ptr, &error);
    return MB_FAILURE;
  }

  status = mb_register_array(verbose, mbio_ptr, MB_MEM_TYPE_BATHYMETRY, sizeof(double),
                            (void **)&bathalongtrack, &error);
  if (status != MB_SUCCESS) {
    fprintf(stderr, "Error registering bathalongtrack array\n");
    mb_close(verbose, &mbio_ptr, &error);
    return MB_FAILURE;
  }

  status = mb_register_array(verbose, mbio_ptr, MB_MEM_TYPE_AMPLITUDE, sizeof(double),
                            (void **)&amp, &error);
  if (status != MB_SUCCESS) {
    fprintf(stderr, "Error registering amp array\n");
    mb_close(verbose, &mbio_ptr, &error);
    return MB_FAILURE;
  }

  status = mb_register_array(verbose, mbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double),
                            (void **)&ss, &error);
  if (status != MB_SUCCESS) {
    fprintf(stderr, "Error registering ss array\n");
    mb_close(verbose, &mbio_ptr, &error);
    return MB_FAILURE;
  }

  status = mb_register_array(verbose, mbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double),
                            (void **)&ssacrosstrack, &error);
  if (status !=  MB_SUCCESS) {
    fprintf(stderr, "Error registering ssacrosstrack array\n");
    mb_close(verbose, &mbio_ptr, &error);
    return MB_FAILURE;
  }

  status = mb_register_array(verbose, mbio_ptr, MB_MEM_TYPE_SIDESCAN, sizeof(double),
                            (void **)&ssalongtrack, &error);
  if (status != MB_SUCCESS) {
    fprintf(stderr, "Error registering ssalongtrack array\n");
    mb_close(verbose, &mbio_ptr, &error);
    return MB_FAILURE;
  }

  // For now, don't pre-allocate arrays - let mb_get_all handle it
  // Just initialize pointers to NULL

/* Read pings in loop */
  int pings = 0; // Local counter for this file
  int total_records = 0;
  int data_records = 0;

  fprintf(stderr, "  [DEBUG] About to start mb_read loop, mbio_ptr=%p\n", (void*)mbio_ptr);
  while ((status = mb_read(
      verbose, mbio_ptr, &kind, &pings,
      time_i, &time_d,
      &navlon, &navlat, &speed, &heading,
      &distance, &altitude, &sensordepth,
      &beams_bath, &beams_amp, &pixels_ss,
      beamflag, bath, amp, bathacrosstrack, bathalongtrack,
      ss, ssacrosstrack, ssalongtrack,
      comment, &error)) == MB_SUCCESS) {
    
    total_records++;
    
    /* Only process survey data */
    if (kind == MB_DATA_DATA) {
      data_records++;
      /* Process this ping */
      process_ping(verbose, beams_bath, beamflag,
                  bath, bathacrosstrack, bathalongtrack,
                  navlon, navlat, heading, time_d);

      /* Update global counter */
      npings++;
      pings++;

      /* Progress report */
      if (verbose > 1 && pings % 100 == 0) {
        fprintf(stderr, "    Processed %d pings...\r", pings);
        fflush(stderr);
      }
    }
  }

  fprintf(stderr, "  [DEBUG] mb_read loop exited: status=%d\n", status);

  if (verbose > 0) {
    fprintf(stderr, "  File complete: %d pings processed\n", pings);
  }

  // Cleanup and close file
  if (ssalongtrack != nullptr)
    mb_freed(verbose, __FILE__, __LINE__, (void **)&ssalongtrack, &error);
  if (ssacrosstrack != nullptr)
    mb_freed(verbose, __FILE__, __LINE__, (void **)&ssacrosstrack, &error);
  if (ss != nullptr)
    mb_freed(verbose, __FILE__, __LINE__, (void **)&ss, &error);
  if (bathalongtrack != nullptr)
    mb_freed(verbose, __FILE__, __LINE__, (void **)&bathalongtrack, &error);
  if (bathacrosstrack != nullptr)
    mb_freed(verbose, __FILE__, __LINE__, (void **)&bathacrosstrack, &error);
  if (bath != nullptr)
    mb_freed(verbose, __FILE__, __LINE__, (void **)&bath, &error);
  if (amp != nullptr)
    mb_freed(verbose, __FILE__, __LINE__, (void **)&amp, &error);
  if (beamflag != nullptr)
    mb_freed(verbose, __FILE__, __LINE__, (void **)&beamflag, &error);

  // Reset arrays to NULL for next file
  ssalongtrack = nullptr;
  ssacrosstrack = nullptr;
  ss = nullptr;
  bathalongtrack = nullptr;
  bathacrosstrack = nullptr;
  bath = nullptr;
  amp = nullptr;
  beamflag = nullptr;

  status = mb_close(verbose, &mbio_ptr, &error);
  if (status != MB_SUCCESS && verbose > 0) {
    fprintf(stderr, "  Warning: Error closing file\n");
  }

  nfile_read++;
  return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* PROCESS SINGLE PING */
/*--------------------------------------------------------------------*/

/**
 * @brief Process bathymetry beams from one ping
 *
 * This function extracts valid soundings from a ping and stores
 * them in the global all_soundings vector for later processing.
 *
 * @param verbose Verbosity level
 * @param beams_bath Number of bathymetry beams in ping
 * @param beamflag Quality flag array [beams_bath]
 * @param bath Depth array [beams_bath] (meters)
 * @param bathacrosstrack Across-track distance array [beams_bath] (meters)
 * @param bathalongtrack Along-track distance array [beams_bath] (meters)
 * @param time_d Timestamp (Unix seconds)
 * @return MB_SUCCESS
 */
static int process_ping(int verbose, int beams_bath, char *beamflag,
                       double *bath, double *bathacrosstrack, double *bathalongtrack,
                       double navlon, double navlat, double heading,
                       double time_d) {

  // Process each beam in the ping
   
  // Loop through all beams and extract valid soundings.
  // Estimate each beam position from the vessel navigation and beam offsets,
  // then convert the result to ECEF for downstream 3D use.
   
    for (int i = 0; i < beams_bath; i++) {
      // Count total beams
      nbeams_total++;
   
      // Check beam quality
      if (!mb_beam_ok(beamflag[i])) {
        nbeams_flagged++;
        continue;  // Skip bad beam
      }
   
      double beam_lon = navlon;
      double beam_lat = navlat;

      // Approximate each beam position from vessel navigation plus beam offsets.
      double heading_rad = heading * degrees_to_radians;
      double east_m = bathalongtrack[i] * std::sin(heading_rad) + bathacrosstrack[i] * std::cos(heading_rad);
      double north_m = bathalongtrack[i] * std::cos(heading_rad) - bathacrosstrack[i] * std::sin(heading_rad);
      local_offsets_to_geodetic(navlon, navlat, east_m, north_m, &beam_lon, &beam_lat);

      // Store both the geographic estimate and its ECEF representation.
      Sounding s;
      s.longitude = beam_lon;
      s.latitude = beam_lat;
      s.depth = bath[i];
      geodetic_to_ecef(s.longitude, s.latitude, -s.depth, &s.ecef_x, &s.ecef_y, &s.ecef_z);
      s.beamflag = beamflag[i];
      s.beam_number = i;
      s.time_d = time_d;
   
      // Filter by geographic bounds if specified
      if (bounds_specified) {
        if (s.longitude < bounds[0] || s.longitude > bounds[1] ||
            s.latitude < bounds[2] || s.latitude > bounds[3]) {
          continue;  // Outside bounds, skip
        }
      }

      // Optional decimation of accepted beams to keep runtime manageable.
      nbeams_in_bounds++;
      if (downsample_factor > 1 && (nbeams_in_bounds % downsample_factor) != 0) {
        continue;
      }
   
      // Add to collection
      all_soundings.push_back(s);
      nbeams_good++;
    }
   
  return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* CREATE .XYZ FILE */
/*--------------------------------------------------------------------*/

/**
 * @brief Write soundings to XYZ point cloud file
 * @param filename Output file path
 * @return MB_SUCCESS or error code
 */
static int write_xyz_file(const char *filename) {
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    fprintf(stderr, "Error: Cannot create XYZ file: %s\n", filename);
    return MB_FAILURE;
  }

  fprintf(stderr, "\nWriting XYZ point cloud: %s\n", filename);
  fprintf(stderr, "  Points: %zu\n", all_soundings.size());

  // This is a temporary debug-friendly XYZ export of the 3D point cloud.
  fprintf(fp, "# X(ecef_m) Y(ecef_m) Z(ecef_m)\n");

  // Write points
  for (const auto &s : all_soundings) {
    fprintf(fp, "%.3f %.3f %.3f\n", s.ecef_x, s.ecef_y, s.ecef_z);
  }

  fclose(fp);
  fprintf(stderr, "  XYZ file written successfully\n");
  return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* PRINT STATISTICS */
/*--------------------------------------------------------------------*/

/**
 * @brief Print statistics about data reading
 */
static void print_statistics() {
  fprintf(stderr, "\n");
  fprintf(stderr, "========================================\n");
  fprintf(stderr, "    Swath Data Reading Statistics\n");
  fprintf(stderr, "========================================\n");
  fprintf(stderr, "Files in datalist:       %d\n", nfile);
  fprintf(stderr, "Files successfully read: %d\n", nfile_read);
  fprintf(stderr, "Pings processed:         %d\n", npings);
  fprintf(stderr, "Beams total:             %d\n", nbeams_total);
  fprintf(stderr, "Beams good:              %d\n", nbeams_good);
  fprintf(stderr, "Beams flagged:           %d\n", nbeams_flagged);
  fprintf(stderr, "Soundings stored:        %zu\n", all_soundings.size());

  if (nbeams_total > 0) {
    double percent_good = 100.0 * nbeams_good / nbeams_total;
    fprintf(stderr, "Acceptance rate:         %.1f%%\n", percent_good);
  }

  fprintf(stderr, "========================================\n");

  /* Print sample soundings if verbose */
  if (verbose > 1 && !all_soundings.empty()) {
    fprintf(stderr, "\nSample soundings (first 10):\n");
    fprintf(stderr, "  %-13s %-13s %-11s %-6s\n",
            "Longitude", "Latitude", "Depth", "Flag");
    fprintf(stderr, "  %-13s %-13s %-11s %-6s\n",
            "-------------", "-------------", "-----------", "------");

    int nsamples = std::min(10, (int)all_soundings.size());
    for (int i = 0; i < nsamples; i++) {
      const Sounding &s = all_soundings[i];
      fprintf(stderr, "  %13.7f %13.7f %11.2f %6d\n",
              s.longitude, s.latitude, s.depth, (int)s.beamflag);
    }
  }

  /* Compute geographic bounds */
  if (!all_soundings.empty()) {
    double min_lon = 999.0, max_lon = -999.0;
    double min_lat = 999.0, max_lat = -999.0;
    double min_depth = 99999.0, max_depth = -99999.0;

    for (const auto &s : all_soundings) {
      min_lon = std::min(min_lon, s.longitude);
      max_lon = std::max(max_lon, s.longitude);
      min_lat = std::min(min_lat, s.latitude);
      max_lat = std::max(max_lat, s.latitude);
      min_depth = std::min(min_depth, s.depth);
      max_depth = std::max(max_depth, s.depth);
    }

    fprintf(stderr, "\nData bounds:\n");
    fprintf(stderr, "  Longitude: %11.6f to %11.6f\n", min_lon, max_lon);
    fprintf(stderr, "  Latitude:  %11.6f to %11.6f\n", min_lat, max_lat);
    fprintf(stderr, "  Depth:     %11.2f to %11.2f meters\n", min_depth, max_depth);
  }

  fprintf(stderr, "\n");
}

/*--------------------------------------------------------------------*/
/* COORDINATE CONVERSION HELPERS */
/*--------------------------------------------------------------------*/

static void geodetic_to_ecef(double lon_deg, double lat_deg, double height_m,
                             double *x_m, double *y_m, double *z_m) {
  // Standard WGS84 geodetic-to-ECEF conversion.
  double lon_rad = lon_deg * degrees_to_radians;
  double lat_rad = lat_deg * degrees_to_radians;

  double sin_lat = std::sin(lat_rad);
  double cos_lat = std::cos(lat_rad);
  double sin_lon = std::sin(lon_rad);
  double cos_lon = std::cos(lon_rad);

  double prime_vertical = wgs84_a / std::sqrt(1.0 - wgs84_e2 * sin_lat * sin_lat);

  if (x_m != nullptr)
    *x_m = (prime_vertical + height_m) * cos_lat * cos_lon;
  if (y_m != nullptr)
    *y_m = (prime_vertical + height_m) * cos_lat * sin_lon;
  if (z_m != nullptr)
    *z_m = (prime_vertical * (1.0 - wgs84_e2) + height_m) * sin_lat;
}

static void local_offsets_to_geodetic(double ref_lon_deg, double ref_lat_deg,
                                      double east_m, double north_m,
                                      double *lon_deg, double *lat_deg) {
  // Convert local meter offsets into a small-angle geodetic estimate.
  double ref_lat_rad = ref_lat_deg * degrees_to_radians;

  double meters_per_deg_lat =
      111132.92 - 559.82 * std::cos(2.0 * ref_lat_rad) +
      1.175 * std::cos(4.0 * ref_lat_rad) - 0.0023 * std::cos(6.0 * ref_lat_rad);

  double meters_per_deg_lon =
      111412.84 * std::cos(ref_lat_rad) -
      93.5 * std::cos(3.0 * ref_lat_rad) +
      0.118 * std::cos(5.0 * ref_lat_rad);

  if (lat_deg != nullptr && meters_per_deg_lat != 0.0)
    *lat_deg = ref_lat_deg + (north_m / meters_per_deg_lat);
  if (lon_deg != nullptr && meters_per_deg_lon != 0.0)
    *lon_deg = ref_lon_deg + (east_m / meters_per_deg_lon);
}
