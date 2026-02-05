/*--------------------------------------------------------------------
 *    The MB-system:	mbmesh.h	2/5/2026
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

#ifndef MBMESH_H_
#define MBMESH_H_

#include "mb_define.h"
#include "mb_status.h"
#include "mb_io.h"

/* mesh algorithm types */
#define MBMESH_ALGORITHM_DELAUNAY    1
#define MBMESH_ALGORITHM_GRID        2
#define MBMESH_ALGORITHM_ADAPTIVE    3

/* mesh output formats */
#define MBMESH_FORMAT_GMT           1
#define MBMESH_FORMAT_VTK           2
#define MBMESH_FORMAT_OBJ           3
#define MBMESH_FORMAT_STL            4
#define MBMESH_FORMAT_JSON          5  /* Add this for web viewing */

/* mesh quality metrics */
#define MBMESH_QUALITY_ASPECT_RATIO 1
#define MBMESH_QUALITY_SKEWNESS     2
#define MBMESH_QUALITY_AREA         3

/* data structures */
struct mbmesh_vertex {
    double x, y, z;           /* coordinates */
    int id;                   /* vertex identifier */
    int boundary_flag;        /* boundary vertex flag */
};

struct mbmesh_triangle {
    int vertices[3];          /* vertex indices */
    int neighbors[3];         /* neighboring triangle indices */
    double quality;           /* triangle quality metric */
    int material_id;          /* material/region identifier */
};

struct mbmesh_edge {
    int vertices[2];          /* vertex indices */
    int triangles[2];         /* adjacent triangle indices */
    int boundary_flag;        /* boundary edge flag */
};

struct mbmesh_mesh {
    int nvertices;            /* number of vertices */
    int ntriangles;           /* number of triangles */
    int nedges;               /* number of edges */
    struct mbmesh_vertex *vertices;     /* vertex array */
    struct mbmesh_triangle *triangles;  /* triangle array */
    struct mbmesh_edge *edges;          /* edge array */
    double bounds[6];         /* mesh bounds: xmin,xmax,ymin,ymax,zmin,zmax */
    double quality_min;       /* minimum triangle quality */
    double quality_avg;       /* average triangle quality */
};

struct mbmesh_control {
    /* input parameters */
    char input_file[MB_PATH_MAXLINE];
    char output_file[MB_PATH_MAXLINE];
    int algorithm;            /* mesh algorithm */
    int output_format;        /* output format */
    double resolution;        /* target mesh resolution */
    double aspect_ratio_max;  /* maximum allowed aspect ratio */
    double angle_min;         /* minimum triangle angle */
    int refine_iterations;    /* number of refinement iterations */
    int smooth_iterations;    /* number of smoothing iterations */
    int boundary_preserve;    /* preserve boundary features flag */
    int verbose;              /* verbosity level */
    
    /* data bounds and statistics */
    double data_bounds[6];    /* data bounds */
    int ndata;                /* number of input data points */
    double data_density;      /* average data point density */
    
    /* mesh quality constraints */
    double quality_threshold; /* minimum acceptable triangle quality */
    int quality_metric;       /* quality metric to use */
    
    /* GMT integration */
    char projection[MB_PATH_MAXLINE];  /* map projection */
    char region[MB_PATH_MAXLINE];      /* geographic region */
};

/* function prototypes */

/* main mesh generation functions */
int mbmesh_generate_delaunay(struct mbmesh_control *control, struct mbmesh_mesh *mesh);
int mbmesh_generate_grid(struct mbmesh_control *control, struct mbmesh_mesh *mesh);
int mbmesh_generate_adaptive(struct mbmesh_control *control, struct mbmesh_mesh *mesh);

/* mesh processing functions */
int mbmesh_refine_mesh(struct mbmesh_mesh *mesh, struct mbmesh_control *control);
int mbmesh_smooth_mesh(struct mbmesh_mesh *mesh, struct mbmesh_control *control);
int mbmesh_validate_mesh(struct mbmesh_mesh *mesh);
int mbmesh_compute_quality(struct mbmesh_mesh *mesh, struct mbmesh_control *control);

/* input/output functions */
int mbmesh_read_data(struct mbmesh_control *control, double **x, double **y, double **z, int *npoints);
int mbmesh_write_mesh(struct mbmesh_mesh *mesh, struct mbmesh_control *control);
int mbmesh_write_gmt(struct mbmesh_mesh *mesh, struct mbmesh_control *control);
int mbmesh_write_vtk(struct mbmesh_mesh *mesh, struct mbmesh_control *control);
int mbmesh_write_json(struct mbmesh_mesh *mesh, struct mbmesh_control *control); /* Add this function prototype */

/* utility functions */
int mbmesh_allocate_mesh(struct mbmesh_mesh *mesh, int nvertices, int ntriangles, int nedges);
int mbmesh_deallocate_mesh(struct mbmesh_mesh *mesh);
int mbmesh_init_control(struct mbmesh_control *control);
double mbmesh_triangle_quality(struct mbmesh_vertex *v1, struct mbmesh_vertex *v2, struct mbmesh_vertex *v3, int metric);
int mbmesh_find_neighbors(struct mbmesh_mesh *mesh);

/* debugging and statistics */
int mbmesh_print_mesh_stats(struct mbmesh_mesh *mesh, struct mbmesh_control *control);
int mbmesh_check_mesh_topology(struct mbmesh_mesh *mesh);

/* geometric utility functions */
double mbmesh_distance_2d(double x1, double y1, double x2, double y2);
double mbmesh_distance_3d(struct mbmesh_vertex *v1, struct mbmesh_vertex *v2);
double mbmesh_triangle_area(struct mbmesh_vertex *v1, struct mbmesh_vertex *v2, struct mbmesh_vertex *v3);
int mbmesh_point_in_triangle(double px, double py, struct mbmesh_vertex *v1, 
                           struct mbmesh_vertex *v2, struct mbmesh_vertex *v3);
double mbmesh_triangle_circumradius(struct mbmesh_vertex *v1, struct mbmesh_vertex *v2, struct mbmesh_vertex *v3);
int mbmesh_point_in_circumcircle(double px, double py, struct mbmesh_vertex *v1,
                                struct mbmesh_vertex *v2, struct mbmesh_vertex *v3);
double mbmesh_triangle_min_angle(struct mbmesh_vertex *v1, struct mbmesh_vertex *v2, struct mbmesh_vertex *v3);
int mbmesh_orientation(struct mbmesh_vertex *p, struct mbmesh_vertex *q, struct mbmesh_vertex *r);
int mbmesh_segments_intersect(struct mbmesh_vertex *p1, struct mbmesh_vertex *q1,
                             struct mbmesh_vertex *p2, struct mbmesh_vertex *q2);

/* simple file reading functions */
int mbmesh_read_xyz_file(const char *filename, double **x, double **y, double **z, int *npoints);
int mbmesh_determine_file_format(const char *filename);


#endif /* MBMESH_H_ */
