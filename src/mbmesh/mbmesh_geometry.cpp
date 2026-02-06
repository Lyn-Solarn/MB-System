/*--------------------------------------------------------------------
 *    The MB-system:	mbmesh_geometry.cpp	2/5/2026
 *
 *    Basic geometric functions for mbmesh
 *--------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "mb_status.h"
#include "mb_define.h"
#include "mbmesh.h"

/*--------------------------------------------------------------------*/
double mbmesh_distance_2d(double x1, double y1, double x2, double y2) {
    /* Calculate 2D Euclidean distance between two points */
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx*dx + dy*dy);
}

/*--------------------------------------------------------------------*/
double mbmesh_distance_3d(struct mbmesh_vertex *v1, struct mbmesh_vertex *v2) {
    /* Calculate 3D Euclidean distance between two vertices */
    double dx = v2->x - v1->x;
    double dy = v2->y - v1->y;
    double dz = v2->z - v1->z;
    return sqrt(dx*dx + dy*dy + dz*dz);
}

/*--------------------------------------------------------------------*/
double mbmesh_triangle_area(struct mbmesh_vertex *v1, struct mbmesh_vertex *v2, struct mbmesh_vertex *v3) {
    /* Calculate triangle area using cross product */
    double ax = v2->x - v1->x;
    double ay = v2->y - v1->y;
    double bx = v3->x - v1->x;
    double by = v3->y - v1->y;
    
    /* Area = 0.5 * |cross product| */
    return 0.5 * fabs(ax * by - ay * bx);
}

/*--------------------------------------------------------------------*/
int mbmesh_point_in_triangle(double px, double py, struct mbmesh_vertex *v1, 
                            struct mbmesh_vertex *v2, struct mbmesh_vertex *v3) {
    /* Test if point (px,py) is inside triangle using barycentric coordinates */
    double denom = (v2->y - v3->y)*(v1->x - v3->x) + (v3->x - v2->x)*(v1->y - v3->y);
    
    if (fabs(denom) < 1e-12) return 0; /* Degenerate triangle */
    
    double a = ((v2->y - v3->y)*(px - v3->x) + (v3->x - v2->x)*(py - v3->y)) / denom;
    double b = ((v3->y - v1->y)*(px - v3->x) + (v1->x - v3->x)*(py - v3->y)) / denom;
    double c = 1 - a - b;
    
    return (a >= 0 && b >= 0 && c >= 0);
}

/*--------------------------------------------------------------------*/
double mbmesh_triangle_circumradius(struct mbmesh_vertex *v1, struct mbmesh_vertex *v2, struct mbmesh_vertex *v3) {
    /* Calculate circumradius of triangle */
    double a = mbmesh_distance_2d(v1->x, v1->y, v2->x, v2->y);
    double b = mbmesh_distance_2d(v2->x, v2->y, v3->x, v3->y);
    double c = mbmesh_distance_2d(v3->x, v3->y, v1->x, v1->y);
    double area = mbmesh_triangle_area(v1, v2, v3);
    
    if (area < 1e-12) return 1e12; /* Degenerate triangle */
    
    return (a * b * c) / (4.0 * area);
}

/*--------------------------------------------------------------------*/
int mbmesh_point_in_circumcircle(double px, double py, struct mbmesh_vertex *v1,
                                 struct mbmesh_vertex *v2, struct mbmesh_vertex *v3) {
    /* Test if point is inside circumcircle of triangle - critical for Delaunay */
    double ax = v1->x - px;
    double ay = v1->y - py;
    double bx = v2->x - px;
    double by = v2->y - py;
    double cx = v3->x - px;
    double cy = v3->y - py;
    
    double det = (ax*ax + ay*ay) * (bx*cy - by*cx) -
                 (bx*bx + by*by) * (ax*cy - ay*cx) +
                 (cx*cx + cy*cy) * (ax*by - ay*bx);
    
    return det > 0;
}

/*--------------------------------------------------------------------*/
double mbmesh_triangle_min_angle(struct mbmesh_vertex *v1, struct mbmesh_vertex *v2, struct mbmesh_vertex *v3) {
    /* Calculate minimum angle in triangle (in degrees) */
    double a = mbmesh_distance_2d(v1->x, v1->y, v2->x, v2->y);
    double b = mbmesh_distance_2d(v2->x, v2->y, v3->x, v3->y);
    double c = mbmesh_distance_2d(v3->x, v3->y, v1->x, v1->y);
    
    if (a < 1e-12 || b < 1e-12 || c < 1e-12) return 0.0;
    
    /* Use law of cosines to find angles */
    double angle1 = acos((b*b + c*c - a*a) / (2.0*b*c));
    double angle2 = acos((a*a + c*c - b*b) / (2.0*a*c));
    double angle3 = acos((a*a + b*b - c*c) / (2.0*a*b));
    
    double min_angle = fmin(angle1, fmin(angle2, angle3));
    return min_angle * 180.0 / M_PI; /* Convert to degrees */
}

/*--------------------------------------------------------------------*/
int mbmesh_orientation(struct mbmesh_vertex *p, struct mbmesh_vertex *q, struct mbmesh_vertex *r) {
    /* Find orientation of ordered triplet (p, q, r) 
     * Returns: 0 -> p, q, r are colinear
     *          1 -> Clockwise
     *          2 -> Counterclockwise */
    double val = (q->y - p->y) * (r->x - q->x) - (q->x - p->x) * (r->y - q->y);
    
    if (fabs(val) < 1e-12) return 0; /* colinear */
    return (val > 0) ? 1 : 2; /* clockwise or counterclockwise */
}

/*--------------------------------------------------------------------*/
int mbmesh_segments_intersect(struct mbmesh_vertex *p1, struct mbmesh_vertex *q1,
                             struct mbmesh_vertex *p2, struct mbmesh_vertex *q2) {
    /* Check if line segments (p1,q1) and (p2,q2) intersect */
    int o1 = mbmesh_orientation(p1, q1, p2);
    int o2 = mbmesh_orientation(p1, q1, q2);
    int o3 = mbmesh_orientation(p2, q2, p1);
    int o4 = mbmesh_orientation(p2, q2, q1);
    
    /* General case */
    if (o1 != o2 && o3 != o4) return 1;
    
    /* Special cases for collinear points would go here */
    return 0;
}
