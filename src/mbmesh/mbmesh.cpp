/*--------------------------------------------------------------------
 *    The MB-system:	mbmesh.cpp	2/5/2026
 *
 *    Copyright (c) 2026-2026 by
 *    David W. Caress (caress@mbari.org)
 *      Monterey Bay Aquarium Research Institute
 *      Moss Landing, CA 95039
 *    and Dale N. Chayes (dale@ldeo.columbia.edu)
 *      Lamont-Doherty Earth Observatory
 *      Palisades, NY 10964
 *
 *    See README file for copying and redistribution conditions.
 *--------------------------------------------------------------------*/
/*
 * MBMESH generates triangular meshes from multibeam bathymetry data.
 * This tool creates high-quality triangular meshes suitable for 
 * numerical modeling, visualization, and analysis applications.
 *
 * Key features:
 * - Multiple mesh generation algorithms (Delaunay, grid-based, adaptive)
 * - Quality-controlled meshing with configurable constraints
 * - Support for multiple output formats (GMT, VTK, OBJ, STL)
 * - Mesh refinement and smoothing capabilities
 * - Boundary preservation for complex coastlines
 * - Integration with GMT for geographic projections
 *
 * Author:	Lyn Larson
 * Date:	February 5, 2026
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "mb_status.h"
#include "mb_define.h"
#include "mb_format.h"
#include "mb_io.h"
#include "mb_aux.h"
#include "mbmesh.h"

static const char program_name[] = "mbmesh";
static const char help_message[] =
    "MBMESH generates triangular meshes from multibeam bathymetry data.\n"
    "The program supports multiple mesh generation algorithms and output formats.\n";

static const char usage_message[] =
    "mbmesh -Iinputfile -Ooutputfile [-Aalgorithm] [-Fformat] [-Rresolution]\n"
    "       [-Qqualitythreshold] [-Nrefineiterations] [-Ssmoothiterations]\n"
    "       [-Bpreserveboundary] [-Pprojection] [-Ggeographicregion] [-V]\n";

/*--------------------------------------------------------------------*/
int main(int argc, char **argv) {
    /* local variables */
    int status = MB_SUCCESS;
    int error = MB_ERROR_NO_ERROR;
    struct mbmesh_control control;
    struct mbmesh_mesh mesh;
    char *input_ptr, *output_ptr;
    int option_index;
    bool errflg = false;
    bool help = false;
    int c;
    double *x_data = NULL, *y_data = NULL, *z_data = NULL;
    int ndata_points = 0;
    clock_t start_time, end_time;
    double cpu_time_used;

    /* initialize control structure */
    if ((status = mbmesh_init_control(&control)) != MB_SUCCESS) {
        error = MB_ERROR_INIT_FAIL;
        exit(error);
    }

    /* initialize mesh structure */
    memset(&mesh, 0, sizeof(struct mbmesh_mesh));

    /* process command line arguments */
    while ((c = getopt(argc, argv, "A:BF:G:I:N:O:P:Q:R:S:VvHh")) != -1) {
        switch (c) {
        case 'A':
            /* mesh algorithm selection */
            control.algorithm = atoi(optarg);
            if (control.algorithm < 1 || control.algorithm > 3) {
                fprintf(stderr, "Invalid algorithm: %d\n", control.algorithm);
                errflg = true;
            }
            break;
        case 'B':
            /* preserve boundary features */
            control.boundary_preserve = MB_YES;
            break;
        case 'F':
            /* output format */
            control.output_format = atoi(optarg);
            if (control.output_format < 1 || control.output_format > 4) {
                fprintf(stderr, "Invalid output format: %d\n", control.output_format);
                errflg = true;
            }
            break;
        case 'G':
            /* geographic region */
            strncpy(control.region, optarg, MB_PATH_MAXLINE-1);
            break;
        case 'I':
            /* input file */
            strncpy(control.input_file, optarg, MB_PATH_MAXLINE-1);
            break;
        case 'N':
            /* number of refinement iterations */
            control.refine_iterations = atoi(optarg);
            break;
        case 'O':
            /* output file */
            strncpy(control.output_file, optarg, MB_PATH_MAXLINE-1);
            break;
        case 'P':
            /* map projection */
            strncpy(control.projection, optarg, MB_PATH_MAXLINE-1);
            break;
        case 'Q':
            /* quality threshold */
            control.quality_threshold = atof(optarg);
            break;
        case 'R':
            /* mesh resolution */
            control.resolution = atof(optarg);
            break;
        case 'S':
            /* number of smoothing iterations */
            control.smooth_iterations = atoi(optarg);
            break;
        case 'V':
        case 'v':
            /* verbose output */
            control.verbose++;
            break;
        case 'H':
        case 'h':
            help = true;
            break;
        case '?':
            errflg = true;
        }
    }

    /* print help message if requested */
    if (help) {
        fprintf(stderr, "\n%s\n", help_message);
        fprintf(stderr, "\nUsage: %s\n", usage_message);
        exit(MB_ERROR_NO_ERROR);
    }

    /* check for required arguments */
    if (strlen(control.input_file) == 0) {
        fprintf(stderr, "ERROR: No input file specified\n");
        errflg = true;
    }
    if (strlen(control.output_file) == 0) {
        fprintf(stderr, "ERROR: No output file specified\n");
        errflg = true;
    }

    /* exit if errors in command line */
    if (errflg) {
        fprintf(stderr, "\nProgram <%s> terminated\n", program_name);
        error = MB_ERROR_BAD_USAGE;
        exit(error);
    }

    /* print starting message */
    if (control.verbose >= 1) {
        fprintf(stderr, "\nProgram <%s>\n", program_name);
        fprintf(stderr, "MB-system Version %s\n", MB_VERSION);
        fprintf(stderr, "Input file:         %s\n", control.input_file);
        fprintf(stderr, "Output file:        %s\n", control.output_file);
        fprintf(stderr, "Mesh algorithm:     %d\n", control.algorithm);
        fprintf(stderr, "Output format:      %d\n", control.output_format);
        fprintf(stderr, "Target resolution:  %g\n", control.resolution);
        fprintf(stderr, "Quality threshold:  %g\n", control.quality_threshold);
    }

    start_time = clock();

    /* Step 1: Read input bathymetry data */
    if (control.verbose >= 1)
        fprintf(stderr, "\nReading input data...\n");
    
    status = mbmesh_read_data(&control, &x_data, &y_data, &z_data, &ndata_points);
    if (status != MB_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to read input data\n");
        error = MB_ERROR_OPEN_FAIL;
        goto cleanup;
    }

    control.ndata = ndata_points;
    if (control.verbose >= 1)
        fprintf(stderr, "Read %d data points\n", ndata_points);

    /* Step 2: Generate initial mesh based on selected algorithm */
    if (control.verbose >= 1)
        fprintf(stderr, "\nGenerating initial mesh...\n");

    switch (control.algorithm) {
    case MBMESH_ALGORITHM_DELAUNAY:
        status = mbmesh_generate_delaunay(&control, &mesh);
        break;
    case MBMESH_ALGORITHM_GRID:
        status = mbmesh_generate_grid(&control, &mesh);
        break;
    case MBMESH_ALGORITHM_ADAPTIVE:
        status = mbmesh_generate_adaptive(&control, &mesh);
        break;
    default:
        fprintf(stderr, "ERROR: Unknown mesh algorithm: %d\n", control.algorithm);
        error = MB_ERROR_BAD_PARAMETER;
        goto cleanup;
    }

    if (status != MB_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to generate initial mesh\n");
        error = MB_ERROR_OTHER;
        goto cleanup;
    }

    /* Step 3: Refine mesh if requested */
    if (control.refine_iterations > 0) {
        if (control.verbose >= 1)
            fprintf(stderr, "Refining mesh (%d iterations)...\n", control.refine_iterations);
        
        status = mbmesh_refine_mesh(&mesh, &control);
        if (status != MB_SUCCESS) {
            fprintf(stderr, "WARNING: Mesh refinement failed\n");
        }
    }

    /* Step 4: Smooth mesh if requested */
    if (control.smooth_iterations > 0) {
        if (control.verbose >= 1)
            fprintf(stderr, "Smoothing mesh (%d iterations)...\n", control.smooth_iterations);
        
        status = mbmesh_smooth_mesh(&mesh, &control);
        if (status != MB_SUCCESS) {
            fprintf(stderr, "WARNING: Mesh smoothing failed\n");
        }
    }

    /* Step 5: Validate and compute mesh quality */
    if (control.verbose >= 1)
        fprintf(stderr, "Validating mesh...\n");
    
    status = mbmesh_validate_mesh(&mesh);
    if (status != MB_SUCCESS) {
        fprintf(stderr, "WARNING: Mesh validation failed\n");
    }

    status = mbmesh_compute_quality(&mesh, &control);
    if (status != MB_SUCCESS) {
        fprintf(stderr, "WARNING: Quality computation failed\n");
    }

    /* Step 6: Write output mesh */
    if (control.verbose >= 1)
        fprintf(stderr, "Writing output mesh...\n");
    
    status = mbmesh_write_mesh(&mesh, &control);
    if (status != MB_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to write output mesh\n");
        error = MB_ERROR_WRITE_FAIL;
        goto cleanup;
    }

    /* print completion message */
    end_time = clock();
    cpu_time_used = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    if (control.verbose >= 1) {
        fprintf(stderr, "\nMesh generation completed successfully!\n");
        mbmesh_print_mesh_stats(&mesh, &control);
        fprintf(stderr, "Processing time: %.2f seconds\n", cpu_time_used);
    }

cleanup:
    /* free allocated memory */
    if (x_data != NULL) free(x_data);
    if (y_data != NULL) free(y_data);
    if (z_data != NULL) free(z_data);
    mbmesh_deallocate_mesh(&mesh);

    /* exit */
    exit(error);
}
