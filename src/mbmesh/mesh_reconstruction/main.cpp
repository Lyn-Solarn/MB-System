#include <Open3D/Open3D.h>
#include <iostream>

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: ./mesh_ballpivot input.xyz output.ply\n";
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    // 1. Load point cloud
    auto pcd = std::make_shared<open3d::geometry::PointCloud>();
    if (!open3d::io::ReadPointCloud(input_file, *pcd)) {
        std::cerr << "Failed to read " << input_file << "\n";
        return 1;
    }

    std::cout << "Loaded point cloud with "
              << pcd->points_.size() << " points\n";

    // 2. Estimate normals (REQUIRED for ball pivoting)
    pcd->EstimateNormals(open3d::geometry::KDTreeSearchParamKNN(80));
    pcd->OrientNormalsConsistentTangentPlane(100);

    // 3. Ball Pivoting
    std::vector<double> radii = {0.5, 1.0, 2.0};

    auto mesh = open3d::geometry::TriangleMesh::CreateFromPointCloudBallPivoting(
        *pcd,
        radii   // (no DoubleVector)
    );

    std::cout << "Mesh has "
              << mesh->vertices_.size() << " vertices, "
              << mesh->triangles_.size() << " faces\n";

    // 4. Save mesh
    if (!open3d::io::WriteTriangleMesh(output_file, *mesh)) {
        std::cerr << "Failed to write " << output_file << "\n";
        return 1;
    }

    std::cout << "Mesh written to " << output_file << "\n";

    return 0;
}
