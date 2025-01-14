/**
 * @author Stefano Nuvoli
 * @author Alessandro Muntoni
 */
#include "faf_visibilitycheck.h"

#include <cg3/geometry/transformations3.h>
#include <cg3/geometry/point2.h>
#include <cg3/geometry/point3.h>
#include <cg3/geometry/triangle2.h>
#include <cg3/geometry/triangle2_utils.h>

#include <cg3/data_structures/trees/aabbtree.h>

#include <cg3/cgal/aabb_tree3.h>

#ifndef FAF_NO_GL_VISIBILITY
#include "includes/view_renderer.h"
#endif



namespace FourAxisFabrication {


/* ----- INTERNAL FUNCTION DECLARATION ----- */

namespace internal {

#ifndef FAF_NO_GL_VISIBILITY
/* Methods for computing visibility (GL) */

void computeVisibilityGL(
        const cg3::EigenMesh& mesh,
        const unsigned int nDirections,
        const unsigned int resolution,
        const double heightfieldAngle,
        const bool includeXDirections,
        const std::vector<unsigned int>& minExtremes,
        const std::vector<unsigned int>& maxExtremes,
        std::vector<cg3::Vec3d>& directions,
        std::vector<double>& directionsAngle,
        cg3::Array2D<int>& visibility);

void computeVisibilityGL(
        ViewRenderer& vr,
        const unsigned int dirIndex,
        const std::vector<cg3::Vec3d>& directions,
        const std::vector<cg3::Vec3d>& faceNormals,
        const double heightfieldAngle,
        cg3::Array2D<int>& visibility);
#endif

/* Methods for computing visibility (Projection and RAY) */

void computeVisibilityProjectionRay(
        const cg3::EigenMesh& mesh,
        const unsigned int nDirections,
        const double heightfieldAngle,
        const bool includeXDirections,
        const std::vector<unsigned int>& minExtremes,
        const std::vector<unsigned int>& maxExtremes,
        std::vector<cg3::Vec3d>& directions,
        std::vector<double>& directionsAngle,
        cg3::Array2D<int>& visibility,
        const CheckMode checkMode);


/* Check visibility (ray shooting) */

void getVisibilityRayShootingOnZ(
        const cg3::EigenMesh& mesh,
        const std::vector<unsigned int>& faces,
        const unsigned int directionIndex,
        const int oppositeDirectionIndex,
        cg3::Array2D<int>& visibility,
        const double heightfieldAngle);

void getVisibilityRayShootingOnZ(
        const cg3::EigenMesh& mesh,
        const unsigned int faceId,
        const unsigned int directionIndex,
        const cg3::Vec3d& direction,
        cg3::cgal::AABBTree3& aabbTree,
        cg3::Array2D<int>& visibility,
        const double heightfieldAngle);



/* Check visibility (projection) */

void getVisibilityProjectionOnZ(
        const cg3::EigenMesh& mesh,
        const std::vector<unsigned int>& faces,
        const unsigned int directionIndex,
        const int oppositeDirectionIndex,
        cg3::Array2D<int>& visibility,
        const double heightfieldAngle);

void getVisibilityProjectionOnZ(
        const cg3::EigenMesh& mesh,
        const unsigned int faceId,
        const unsigned int directionIndex,
        const cg3::Vec3d& direction,
        cg3::AABBTree<2, cg3::Triangle2d>& aabbTree,
        cg3::Array2D<int>& visibility,
        const double heightfieldAngle);


/* Comparators */

struct TriangleZComparator {
    const cg3::SimpleEigenMesh& m;
    TriangleZComparator(const cg3::SimpleEigenMesh& m);
    bool operator()(unsigned int f1, unsigned int f2);
};
bool triangle2DComparator(const cg3::Triangle2d& t1, const cg3::Triangle2d& t2);


/* AABB extractor functions */

double triangle2DAABBExtractor(
        const cg3::Triangle2d& triangle,
        const cg3::AABBValueType& valueType,
        const int& dim);

/* Detection of non-visible faces */
void detectNonVisibleFaces(
        const cg3::Array2D<int>& visibility,
        std::vector<unsigned int>& nonVisibleFaces);


} //namespace internal



/* ----- VISIBILITY METHODS ----- */


/**
 * @brief Get visibility of each face of the mesh from a given number of
 * different directions.
 * It is implemented by a ray casting algorithm or checking the intersections
 * in a 2D projection from a given direction.
 * @param[in] mesh Input mesh
 * @param[in] nDirections Number of directions to be checked
 * @param[in] resolution Resolution for the rendering
 * @param[in] heightfieldAngle Limit angle with triangles normal in order to be a heightfield
 * @param[in] includeXDirections Compute visibility for +x and -x directions
 * @param[out] data Four axis fabrication data
 * @param[in] checkMode Check mode for visibility
 */
void getVisibility(
        const cg3::EigenMesh& mesh,
        const unsigned int nDirections,
        const unsigned int resolution,
        const double heightfieldAngle,
        const bool includeXDirections,
        Data& data,
        const CheckMode checkMode)
{
    if (checkMode == OPENGL) {
#ifndef FAF_NO_GL_VISIBILITY
        internal::computeVisibilityGL(mesh, nDirections, resolution, heightfieldAngle, includeXDirections, data.minExtremes, data.maxExtremes, data.directions, data.angles, data.visibility);
#else
        (void)resolution;
        throw std::runtime_error("OpenGL visibility not supported. Use another check mode.");
#endif
    }
    else {
        internal::computeVisibilityProjectionRay(mesh, nDirections, heightfieldAngle, includeXDirections, data.minExtremes, data.maxExtremes, data.directions, data.angles, data.visibility, checkMode);
    }
    internal::detectNonVisibleFaces(data.visibility, data.nonVisibleFaces);
}




/* ----- INTERNAL FUNCTION DEFINITION ----- */

namespace internal {

#ifndef FAF_NO_GL_VISIBILITY
/* ----- METHODS FOR COMPUTING VISIBILITY (GL) ----- */

/**
 * @brief Get visibility of each face of the mesh from a given number of
 * different directions.
 * It is implemented by a ray casting algorithm or checking the intersections
 * in a 2D projection from a given direction.
 * @param[in] mesh Input mesh
 * @param[in] numberDirections Number of directions to be checked
 * @param[in] resolution Resolution for the rendering
 * @param[in] heightfieldAngle Limit angle with triangles normal in order to be a heightfield
 * @param[in] includeXDirections Compute visibility for +x and -x directions
 * @param[in] minExtremes Min extremes
 * @param[in] maxExtremes Max extremes
 * @param[out] directions Vector of directions
 * @param[out] angles Vector of angle (respect to z-axis)
 * @param[out] visibility Output visibility
 */
void computeVisibilityGL(
        const cg3::EigenMesh& mesh,
        const unsigned int nDirections,
        const unsigned int resolution,
        const double heightfieldAngle,
        const bool includeXDirections,
        const std::vector<unsigned int>& minExtremes,
        const std::vector<unsigned int>& maxExtremes,
        std::vector<cg3::Vec3d>& directions,
        std::vector<double>& angles,
        cg3::Array2D<int>& visibility)
{
    //Initialize visibility (number of directions, two for extremes)
    visibility.clear();
    visibility.resize(nDirections + 2, mesh.numberFaces());
    visibility.fill(0);

    //Initialize direction vector
    directions.clear();
    directions.resize(nDirections + 2);

    //Initialize angles
    angles.clear();
    angles.resize(nDirections);

    //Cos of the height field angle
    const double heightFieldLimit = cos(heightfieldAngle);

    //View renderer
    ViewRenderer vr(mesh, mesh.boundingBox(), resolution);

    //Set target faces to be checked
    std::vector<unsigned int> targetFaces;
    for (unsigned int fId = 0; fId < mesh.numberFaces(); fId++) {
        targetFaces.push_back(fId);
    }

    //Compute normals
    std::vector<cg3::Vec3d> faceNormals(mesh.numberFaces());
    for (unsigned int fId = 0; fId < mesh.numberFaces(); fId++) {
        faceNormals[fId] = mesh.faceNormal(fId);
    }

    //Step angle for getting all the directions (on 180 degrees)
    double stepAngle = 2*M_PI / nDirections;


    //Index for min, max extremes
    const unsigned int minIndex = nDirections;
    const unsigned int maxIndex = nDirections + 1;

    //Get rotation matrix around the x-axis
    const cg3::Vec3d xAxis(1,0,0);
    Eigen::Matrix3d rot;
    cg3::rotationMatrix(xAxis, stepAngle, rot);

    //Set angles and directions
    cg3::Vec3d dir(0,0,1);
    double sum = 0;
    for(unsigned int i = 0; i < nDirections; i++) {
        angles[i] = sum;
        directions[i] = dir;

        sum += stepAngle;
        dir.rotate(rot);
    }

    //For each direction
    for(unsigned int dirIndex = 0; dirIndex < nDirections; dirIndex++) {
        computeVisibilityGL(vr, dirIndex, directions, faceNormals, heightFieldLimit, visibility);
    }

    //Add min and max extremes directions
    directions[minIndex] = cg3::Vec3d(-1,0,0);
    directions[maxIndex] = cg3::Vec3d(1,0,0);

    if (includeXDirections) {
        //Compute -x and +x visibility        
        computeVisibilityGL(vr, minIndex, directions, faceNormals, heightFieldLimit, visibility);
        computeVisibilityGL(vr, maxIndex, directions, faceNormals, heightFieldLimit, visibility);
    }
    else {
        //Set visibility of the min extremes
        #pragma omp parallel for
        for (size_t i = 0; i < minExtremes.size(); i++){
            unsigned int faceId = minExtremes[i];
            visibility(minIndex, faceId) = 1;
        }

        //Set visibility of the max extremes
        #pragma omp parallel for
        for (size_t i = 0; i < maxExtremes.size(); i++){
            unsigned int faceId = maxExtremes[i];
            visibility(maxIndex, faceId) = 1;
        }
    }
}

/**
 * @brief Compute visibility from a given direction
 * @param[in] vr View renderer
 * @param[in] dirIndex Index of the direction
 * @param[in] directions Directions
 * @param[in] faceNormals Face normals
 * @param[in] heightFieldLimit Cos of the angle
 * @param[in] visibility Visibility output array
 */
void computeVisibilityGL(
        ViewRenderer& vr,
        const unsigned int dirIndex,
        const std::vector<cg3::Vec3d>& directions,
        const std::vector<cg3::Vec3d>& faceNormals,
        const double heightFieldLimit,
        cg3::Array2D<int>& visibility)
{
    const cg3::Vec3d& dir = directions[dirIndex];

    //Compute visibility from the direction
    std::vector<bool> faceVisibility = vr.renderVisibility(dir, true, false);

    #pragma omp parallel for
    for (size_t fId = 0; fId < faceVisibility.size(); fId++) {
        if (faceVisibility[fId] && faceNormals[fId].dot(dir) >= heightFieldLimit) {
            visibility(dirIndex, fId) = true;
        }
        else {
            visibility(dirIndex, fId) = false;
        }
    }
}
#endif

/**
 * @brief Detect faces that are not visible
 * @param[in] visibility Visibility
 * @param[out] nonVisibleFaces Collection of non-visible faces
 */
void detectNonVisibleFaces(        
        const cg3::Array2D<int>& visibility,
        std::vector<unsigned int>& nonVisibleFaces)
{

    //Detect non-visible faces
    nonVisibleFaces.clear();
    for (unsigned int faceId = 0; faceId < visibility.sizeY(); faceId++){
        //Check if at least a direction has been found for each face
        bool found = false;

        for (unsigned int i = 0; i < visibility.sizeX() && !found; i++){
            if (visibility(i, faceId) == 1) {
                found = true;
            }
        }

        //Add to the non-visible face
        if (!found)
            nonVisibleFaces.push_back(faceId);
    }

}





/* ----- CHECK VISIBILITY (PROJECTION AND RAY) ----- */


/**
 * @brief Get visibility of each face of the mesh from a given number of
 * different directions.
 * It is implemented by a ray casting algorithm or checking the intersections
 * in a 2D projection from a given direction.
 * @param[in] mesh Input mesh
 * @param[in] numberDirections Number of directions to be checked
 * @param[out] directions Vector of directions
 * @param[out] angles Vector of angle (respect to z-axis)
 * @param[out] visibility Output visibility
 * @param[in] heightfieldAngle Limit angle with triangles normal in order to be a heightfield
 * @param[in] checkMode Visibility check mode. Default is projection mode.
 */
void computeVisibilityProjectionRay(
        const cg3::EigenMesh& mesh,
        const unsigned int nDirections,
        const double heightfieldAngle,
        const bool includeXDirections,
        const std::vector<unsigned int>& minExtremes,
        const std::vector<unsigned int>& maxExtremes,
        std::vector<cg3::Vec3d>& directions,
        std::vector<double>& angles,
        cg3::Array2D<int>& visibility,
        const CheckMode checkMode)
{
    const unsigned int halfNDirections = nDirections/2;

    //Initialize visibility (number of directions, two for extremes)
    visibility.clear();
    visibility.resize(halfNDirections*2 + 2, mesh.numberFaces());
    visibility.fill(0);

    //Initialize direction vector
    directions.clear();
    directions.resize(halfNDirections*2 + 2);

    //Initialize angles
    angles.clear();
    angles.resize(halfNDirections*2);

    //Set target faces to be checked
    std::vector<unsigned int> targetFaces;

    for (unsigned int i = 0; i < mesh.numberFaces(); i++) {
        targetFaces.push_back(i);
    }

    //Copy the mesh
    cg3::EigenMesh rotatingMesh = mesh;

    //Step angle for getting all the directions (on 180 degrees)
    double stepAngle = M_PI / halfNDirections;

    const cg3::Vec3d xAxis(1,0,0);
    const cg3::Vec3d yAxis(0,1,0);

    //Get rotation matrix
    Eigen::Matrix3d rotationMatrix;
    Eigen::Matrix3d inverseRotationMatrix;
    cg3::rotationMatrix(xAxis, stepAngle, rotationMatrix);
    cg3::rotationMatrix(xAxis, -stepAngle, inverseRotationMatrix);

    //Vector that is opposite to the milling direction
    cg3::Vec3d dir(0,0,1);

    //Set angles
    angles.resize(halfNDirections*2);
    double sum = 0;
    for(unsigned int i = 0; i < halfNDirections*2; i++) {
        angles[i] = sum;
        sum += stepAngle;
    }

    //For each direction
    for(unsigned int dirIndex = 0; dirIndex < halfNDirections; dirIndex++){
        if (checkMode == RAYSHOOTING) {
            //Check visibility ray shooting
            internal::getVisibilityRayShootingOnZ(rotatingMesh, targetFaces, dirIndex, halfNDirections + dirIndex, visibility, heightfieldAngle);
        }
        else {
            //Check visibility with projection
            internal::getVisibilityProjectionOnZ(rotatingMesh, targetFaces, dirIndex, halfNDirections + dirIndex, visibility, heightfieldAngle);
        }

        //Add the current directions
        directions[dirIndex] = dir;
        directions[halfNDirections + dirIndex] = -dir;

        //Rotate the direction and the mesh
        rotatingMesh.rotate(inverseRotationMatrix);
        dir.rotate(rotationMatrix);
    }

    //Index for min, max extremes
    unsigned int minIndex = halfNDirections*2;
    unsigned int maxIndex = halfNDirections*2 + 1;

    //Add min and max extremes directions
    directions[minIndex] = cg3::Vec3d(-1,0,0);
    directions[maxIndex] = cg3::Vec3d(1,0,0);

    if (includeXDirections) {
        //Compute -x and +x visibility
        rotatingMesh = mesh;
        cg3::rotationMatrix(yAxis, M_PI/2, rotationMatrix);
        rotatingMesh.rotate(rotationMatrix);
        if (checkMode == RAYSHOOTING) {
            //Check visibility ray shooting
            internal::getVisibilityRayShootingOnZ(rotatingMesh, targetFaces, minIndex, maxIndex, visibility, heightfieldAngle);
        }
        else {
            //Check visibility with projection
            internal::getVisibilityProjectionOnZ(rotatingMesh, targetFaces, minIndex, maxIndex, visibility, heightfieldAngle);
        }
    }
    else {
        //Set visibility of the min extremes
        #pragma omp parallel for
        for (size_t i = 0; i < minExtremes.size(); i++){
            unsigned int faceId = minExtremes[i];
            visibility(minIndex, faceId) = 1;
        }

        //Set visibility of the max extremes
        #pragma omp parallel for
        for (size_t i = 0; i < maxExtremes.size(); i++){
            unsigned int faceId = maxExtremes[i];
            visibility(maxIndex, faceId) = 1;
        }
    }
}




/* ----- CHECK VISIBILITY (PROJECTION) ----- */

/**
 * @brief Check visibility of each face of the mesh from a given direction.
 * It is implemented by checking the intersection of the projection of the
 * segments from a given direction.
 * @param[in] mesh Input mesh. Normals must be updated before.
 * @param[in] nDirection Total number of directions
 * @param[in] directionIndex Index of the direction
 * @param[out] direction Current direction
 * @param[out] visibility Map the visibility from the given directions
 * to each face.
 * @param[in] heightfieldAngle Limit angle with triangles normal in order to be a heightfield
 */
void getVisibilityProjectionOnZ(
        const cg3::EigenMesh& mesh,
        const std::vector<unsigned int>& faces,
        const unsigned int directionIndex,
        const int oppositeDirectionIndex,
        cg3::Array2D<int>& visibility,
        const double heightfieldAngle)
{
    cg3::AABBTree<2, cg3::Triangle2d> aabbTreeMax(
                &internal::triangle2DAABBExtractor, &internal::triangle2DComparator);
    cg3::AABBTree<2, cg3::Triangle2d> aabbTreeMin(
                &internal::triangle2DAABBExtractor, &internal::triangle2DComparator);

    //Order the face by z-coordinate of the barycenter
    std::vector<unsigned int> orderedZFaces(faces);
    std::sort(orderedZFaces.begin(), orderedZFaces.end(), internal::TriangleZComparator(mesh));

    //Directions to be checked
    cg3::Vec3d zDirMax(0,0,1);
    cg3::Vec3d zDirMin(0,0,-1);

    //Start from the max z-coordinate face
    for (int i = orderedZFaces.size()-1; i >= 0; i--) {
        unsigned int faceId = orderedZFaces[i];

        internal::getVisibilityProjectionOnZ(
                    mesh, faceId, directionIndex, zDirMax, aabbTreeMax, visibility, heightfieldAngle);
    }

    if (oppositeDirectionIndex >= 0) {
        //Start from the min z-coordinate face
        for (unsigned int i = 0; i < orderedZFaces.size(); i++) {
            unsigned int faceId = orderedZFaces[i];

            internal::getVisibilityProjectionOnZ(
                        mesh, faceId, oppositeDirectionIndex, zDirMin, aabbTreeMin, visibility, heightfieldAngle);
        }
    }
}

/**
 * @brief Check visibility of a face from a given direction.
 * It is implemented by checking the intersection of the projection of the
 * segments from a given direction.
 * @param[in] mesh Input mesh
 * @param[in] faceId Id of the face of the mesh
 * @param[in] directionIndex Current direction index
 * @param[in] direction Direction
 * @param[out] aabbTree AABB tree with projections
 * @param[out] visibility Map the visibility from the given directions
 * to each face.
 * @param[in] heightfieldAngle Limit angle with triangles normal in order to be a heightfield
 */
void getVisibilityProjectionOnZ(
        const cg3::EigenMesh& mesh,
        const unsigned int faceId,
        const unsigned int directionIndex,
        const cg3::Vec3d& direction,
        cg3::AABBTree<2, cg3::Triangle2d>& aabbTree,
        cg3::Array2D<int>& visibility,
        const double heightfieldAngle)
{
    const double heightFieldLimit = cos(heightfieldAngle);

    //If it is visible (checking the angle between normal and the target direction)
    if (direction.dot(mesh.faceNormal(faceId)) >= heightFieldLimit) {
        const cg3::Point3i& faceData = mesh.face(faceId);
        const cg3::Point3d& v1 = mesh.vertex(faceData.x());
        const cg3::Point3d& v2 = mesh.vertex(faceData.y());
        const cg3::Point3d& v3 = mesh.vertex(faceData.z());

        //Project on the z plane
        cg3::Point2d v1Projected(v1.x(), v1.y());
        cg3::Point2d v2Projected(v2.x(), v2.y());
        cg3::Point2d v3Projected(v3.x(), v3.y());

        //Create triangle
        cg3::Triangle2d triangle(v1Projected, v2Projected, v3Projected);
        cg3::sortTriangle2DPointsAndReorderCounterClockwise(triangle);

        //Check for intersections
        bool intersectionFound =
                aabbTree.aabbOverlapCheck(triangle, &cg3::triangleOverlap);

        //If no intersections have been found
        if (!intersectionFound) {
            //Set visibility
            visibility(directionIndex, faceId) = 1;

            aabbTree.insert(triangle);
        }
    }

}


/* ----- COMPARATORS ----- */

/**
 * @brief Comparator for barycenter on z values
 * @param m Input mesh
 */
TriangleZComparator::TriangleZComparator(const cg3::SimpleEigenMesh& m) : m(m) {}
bool TriangleZComparator::operator()(unsigned int f1, unsigned int f2){
    const cg3::Point3i& ff1 = m.face(f1);
    const cg3::Point3i& ff2 = m.face(f2);
    double minZ1 = std::min(std::min(m.vertex(ff1.x()).z(), m.vertex(ff1.y()).z()),m.vertex(ff1.z()).z());
    double minZ2 = std::min(std::min(m.vertex(ff2.x()).z(), m.vertex(ff2.y()).z()),m.vertex(ff2.z()).z());
    return minZ1 < minZ2;
}

/**
 * @brief Comparator for triangles
 * @param t1 Triangle 1
 * @param t2 Triangle 2
 * @return True if triangle 1 is less than triangle 2
 */
bool triangle2DComparator(const cg3::Triangle2d& t1, const cg3::Triangle2d& t2) {
    if (t1.v1() < t2.v1())
        return true;
    if (t2.v1() < t1.v1())
        return false;

    if (t1.v2() < t2.v2())
        return true;
    if (t2.v2() < t1.v2())
        return false;

    return t1.v3() < t2.v3();
}

/* ----- TRIANGLE OVERLAP AND AABB FUNCTIONS ----- */

/**
 * @brief Extract a 2D triangle AABB
 * @param[in] triangle Input triangle
 * @param[in] valueType Type of the value requested (MIN or MAX)
 * @param[in] dim Dimension requested of the value (0 for x, 1 for y)
 * @return Requested coordinate of the AABB
 */
double triangle2DAABBExtractor(
        const cg3::Triangle2d& triangle,
        const cg3::AABBValueType& valueType,
        const int& dim)
{
    if (valueType == cg3::AABBValueType::MIN) {
        switch (dim) {
        case 1:
            return (double) std::min(std::min(triangle.v1().x(), triangle.v2().x()), triangle.v3().x());
        case 2:
            return (double) std::min(std::min(triangle.v1().y(), triangle.v2().y()), triangle.v3().y());
        }
    }
    else if (valueType == cg3::AABBValueType::MAX) {
        switch (dim) {
        case 1:
            return (double) std::max(std::max(triangle.v1().x(), triangle.v2().x()), triangle.v3().x());
        case 2:
            return (double) std::max(std::max(triangle.v1().y(), triangle.v2().y()), triangle.v3().y());
        }
    }
    throw new std::runtime_error("Impossible to extract an AABB value.");
}





/* ----- CHECK VISIBILITY (RAY SHOOTING) ----- */

/**
 * @brief Check visibility of each face of the mesh from a given direction.
 * It is implemented by a ray casting algorithm.
 * @param[in] mesh Input mesh. Normals must be updated before.
 * @param[in] nDirection Total number of directions
 * @param[in] directionIndex Index of the direction
 * @param[out] direction Current direction
 * @param[out] visibility Map the visibility from the given directions
 * to each face.
 * @param[in] heightfieldAngle Limit angle with triangles normal in order to be a heightfield
 */
void getVisibilityRayShootingOnZ(
        const cg3::EigenMesh& mesh,
        const std::vector<unsigned int>& faces,
        const unsigned int directionIndex,
        const int oppositeDirectionIndex,
        cg3::Array2D<int>& visibility,
        const double heightfieldAngle)
{
    const double heightFieldLimit = cos(heightfieldAngle);

    //Create cgal AABB on the current mesh
    cg3::cgal::AABBTree3 tree(mesh);

    //Get bounding box min and max z-coordinate
    double minZ = mesh.boundingBox().minZ()-1;
    double maxZ = mesh.boundingBox().maxZ()+1;

    for(unsigned int faceIndex : faces){
        //Get the face data
        cg3::Point3i f = mesh.face(faceIndex);
        cg3::Vec3d v1 = mesh.vertex(f.x());
        cg3::Vec3d v2 = mesh.vertex(f.y());
        cg3::Vec3d v3 = mesh.vertex(f.z());

        //Barycenter of the face
        cg3::Point3d bar((v1 + v2 + v3) / 3);

        //Calculate the intersection in the mesh
        std::list<int> barIntersection;

        tree.getIntersectedEigenFaces(
                    cg3::Point3d(bar.x(), bar.y(), maxZ),
                    cg3::Point3d(bar.x(), bar.y(), minZ),
                    barIntersection);


        assert(barIntersection.size() >= 2);
#ifdef RELEASECHECK
        if (barIntersection.size() < 2) {
            std::cout << "ERROR: intersections are less than 2 from the direction " << std::endl;
            exit(1);
        }
#endif

        //Directions to be checked
        cg3::Vec3d zDirMax(0,0,1);
        cg3::Vec3d zDirMin(0,0,-1);

        //Set the visibility of the face which has
        //the highest z-coordinate barycenter
        double maxZCoordinate = minZ;
        int maxZFace = -1;

        //Set the visibility of the face which has
        //the lowest z-coordinate barycenter
        double minZCoordinate = maxZ;
        int minZFace = -1;

        for (int intersectedFace : barIntersection) {
            //Get the face data
            cg3::Point3i faceData = mesh.face(intersectedFace);
            cg3::Point3d currentBarycenter = (
                        mesh.vertex(faceData.x()) +
                        mesh.vertex(faceData.y()) +
                        mesh.vertex(faceData.z())
            ) / 3;



            //Save face with the maximum Z barycenter and that is visible
            //from (0,0,1)
            if (currentBarycenter.z() > maxZCoordinate &&
                    zDirMax.dot(mesh.faceNormal(intersectedFace)) >= heightFieldLimit)
            {
                maxZFace = intersectedFace;
                maxZCoordinate = currentBarycenter.z();
            }


            if (oppositeDirectionIndex >= 0) {
                //Save face with the minimum Z barycenter and that is visible
                //from (0,0,-1) direction
                if (currentBarycenter.z() < minZCoordinate &&
                        zDirMin.dot(mesh.faceNormal(intersectedFace)) >= heightFieldLimit)
                {
                    minZFace = intersectedFace;
                    minZCoordinate = currentBarycenter.z();
                }
            }
        }

        //Set the visibility
        visibility(directionIndex, maxZFace) = 1;

        if (oppositeDirectionIndex >= 0) {
            visibility(oppositeDirectionIndex, minZFace) = 1;
        }


        assert(zDirMax.dot(mesh.faceNormal(maxZFace)) >= heightFieldLimit);
#ifdef RELEASECHECK
        //TO BE DELETED ON FINAL RELEASE
        if (zDirMax.dot(mesh.faceNormal(maxZFace)) < heightFieldLimit) {
            std::cout << "ERROR: not visible triangle, dot product with z-axis is less than 0 for the direction " << mesh.faceNormal(maxZFace) << std::endl;
            exit(1);
        }
#endif

        assert(maxZFace >= 0);
#ifdef RELEASECHECK
        //TO BE DELETED ON FINAL RELEASE
        if (maxZFace < 0) {
            std::cout << "ERROR: no candidate max face has been found" << std::endl;
            exit(1);
        }
#endif


        if (oppositeDirectionIndex >= 0) {
            assert(zDirMin.dot(mesh.faceNormal(minZFace)) >= heightFieldLimit);
#ifdef RELEASECHECK
            //TO BE DELETED ON FINAL RELEASE
            if (zDirMin.dot(mesh.faceNormal(minZFace)) < heightFieldLimit) {
                std::cout << "ERROR: not visible triangle, dot product with opposite of z-axis is less than 0 for the direction " << mesh.faceNormal(minZFace) << std::endl;
                exit(1);
            }
#endif

            assert(minZFace >= 0);
#ifdef RELEASECHECK
            //TO BE DELETED ON FINAL RELEASE
            if (minZFace < 0) {
                std::cout << "ERROR: no candidate min face has been found" << std::endl;
                exit(1);
            }
#endif
        }
    }
}



}

}
