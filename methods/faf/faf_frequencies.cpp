/**
 * @author Stefano Nuvoli
 * @author Alessandro Muntoni
 */
#include "faf_frequencies.h"

#include <set>
#include <unordered_set>
#include <utility>

#ifdef CG3_LIBIGL_DEFINED
#include <cg3/libigl/mesh_adjacencies.h>
#endif

#include <cg3/geometry/triangle.h>

#include <cg3/geometry/2d/triangle2d_utils.h>
#include <cg3/geometry/utils3d.h>

#include <cg3/geometry/transformations.h>

#include "faf_utilities.h"
#include "faf_visibilitycheck.h"
#include "faf_minimization.h"
#include "faf_association.h"

#define BINARY_SEARCH_ITERATIONS 10

namespace FourAxisFabrication {


/* ----- INTERNAL FUNCTION DECLARATION ----- */

namespace internal {

bool restoreFrequenciesValidHeightfields(
        cg3::EigenMesh& mesh,
        const std::vector<cg3::Vec3>& differentialCoordinates,
        const std::vector<std::vector<int>>& vertexVertexAdjacencies,
        const std::vector<std::vector<int>>& vertexFaceAdjacencies,
        const Data& data,
        const double heightfieldAngle);

std::vector<cg3::Vec3> computeDifferentialCoordinates(
        const cg3::EigenMesh& mesh,
        const std::vector<std::vector<int>>& vertexVertexAdjacencies);

cg3::Pointd getTargetPoint(
        const cg3::EigenMesh& mesh,
        const std::vector<cg3::Vec3>& differentialCoordinates,
        const int vId,
        const std::vector<std::vector<int>>& vertexVertexAdjacencies);

bool isHeightFieldValid(
        const cg3::EigenMesh& mesh,
        const Data& data,
        const int vId,
        const cg3::Pointd& newPoint,
        const std::vector<std::vector<int>>& vertexFaceAdjacencies,
        const double heightfieldAngle);

}

/* ----- RESTORE FREQUENCIES ----- */

/**
 * @brief Restore frequencies of a smoothed mesh.
 * Note that the meshes must have the same number of vertices and faces.
 * @param[in] iterations Number of iterations of the algorithm
 * @param[in] heightfieldAngle Limit angle with triangles normal in order to be a heightfield
 * @param[in] originalMesh Original detailed mesh
 * @param[out] smoothedMesh mesh Target smoothed mesh
 * @param[out] data Four axis fabrication data
 */
void restoreFrequencies(
        const unsigned int iterations,
        const double heightfieldAngle,
        const cg3::EigenMesh& originalMesh,
        cg3::EigenMesh& smoothedMesh,
        Data& data)
{
    assert(originalMesh.getNumberVertices() == smoothedMesh.getNumberVertices());
    assert(originalMesh.getNumberFaces() == smoothedMesh.getNumberFaces());

    //Get vertex-vertex adjacencies
    const std::vector<std::vector<int>> vertexVertexAdjacencies =
            cg3::libigl::getVertexVertexAdjacencies(originalMesh);
    assert(vertexVertexAdjacencies.size() == originalMesh.getNumberVertices());

    //Get vertex-face adjacencies
    const std::vector<std::vector<int>> vertexFaceAdjacencies =
            cg3::libigl::getVertexFaceAdjacencies(originalMesh);
    assert(vertexFaceAdjacencies.size() == originalMesh.getNumberVertices());

    //Get face-face adjacencies
    const std::vector<std::vector<int>> faceFaceAdjacencies =
            cg3::libigl::getFaceFaceAdjacencies(originalMesh);
    assert(faceFaceAdjacencies.size() == originalMesh.getNumberFaces());

    const std::vector<cg3::Vec3> differentialCoordinates =
            internal::computeDifferentialCoordinates(originalMesh, vertexVertexAdjacencies);

    //Copy the target mesh
    cg3::EigenMesh& targetMesh = data.restoredMesh;
    targetMesh = smoothedMesh;


    for (unsigned int i = 0; i < iterations; ++i) {
        internal::restoreFrequenciesValidHeightfields(
                    targetMesh,
                    differentialCoordinates,
                    vertexVertexAdjacencies,
                    vertexFaceAdjacencies,
                    data,
                    heightfieldAngle);
    }

    //Needed because they changed
    targetMesh.updateFacesAndVerticesNormals();
    targetMesh.updateBoundingBox();

    data.restoredMeshAssociation = data.association;
    data.restoredMeshNonVisibleFaces = data.nonVisibleFaces;
    data.restoredMeshVisibility = data.visibility;
}



/**
 * @brief Check visibility of each face of the mesh from the associated direction.
 * It is implemented by a ray casting algorithm or checking the intersections
 * in a 2D projection from a given direction.
 * @param[out] data Four axis fabrication data
 * @param[in] heightfieldAngle Limit angle with triangles normal in order to be a heightfield
 * @param[in] checkMode Visibility check mode. Default is projection mode.
 * @returns The number of no longer visible triangles
 */
unsigned int checkVisibilityAfterFrequenciesAreRestored(
        Data& data,
        const double heightfieldAngle,
        CheckMode checkMode)
{
    cg3::EigenMesh& targetMesh = data.restoredMesh;
    const int nDirections = (data.directions.size()-2)/2;

    //Initialize new data
    Data newData;
    newData.minExtremes = data.minExtremes;
    newData.maxExtremes = data.maxExtremes;

    //Get new visibility
    getVisibility(targetMesh, nDirections, true, newData, heightfieldAngle, checkMode);
    data.restoredMeshVisibility = newData.visibility;

    data.restoredMeshNonVisibleFaces.clear();

    for (size_t faceId = 0; faceId < data.restoredMeshAssociation.size(); faceId++) {
        if (newData.visibility(data.restoredMeshAssociation[faceId], faceId) < 1) {
            data.restoredMeshNonVisibleFaces.push_back(faceId);
        }
    }

    return (unsigned int) data.restoredMeshNonVisibleFaces.size() - data.nonVisibleFaces.size();
}



/* ----- INTERNAL FUNCTION DEFINITION ----- */

namespace internal {


/**
 * @brief Restore frequencies with no occlusion (just heightfield validation)
 * @param[out] targetMesh Target mesh
 * @param[in] differentialCoordinates Differential coordinate of the mesh
 * @param[in] vertexVertexAdjacencies Vertex-vertex adjacencies of the mesh
 * @param[in] vertexFaceAdjacencies Vertex-Face adjacencies of the mesh
 * @param[in] data Four axis fabrication data
 * @param[in] heightfieldAngle Limit angle with triangles normal in order to be a heightfield
 * @return True if at least a vertex has been moved
 */
bool restoreFrequenciesValidHeightfields(
        cg3::EigenMesh& targetMesh,
        const std::vector<cg3::Vec3>& differentialCoordinates,
        const std::vector<std::vector<int>>& vertexVertexAdjacencies,
        const std::vector<std::vector<int>>& vertexFaceAdjacencies,
        const Data& data,
        const double heightfieldAngle)
{
    bool aVertexHasMoved = false;
    for(unsigned int vId = 0; vId < targetMesh.getNumberVertices(); ++vId) {
        //Get current and target point
        cg3::Pointd currentPoint = targetMesh.getVertex(vId);
        cg3::Pointd targetPoint = internal::getTargetPoint(targetMesh, differentialCoordinates, vId, vertexVertexAdjacencies);

        //Do binary search until the face normals do not violate the heightfield conditions
        int count = 0;
        while (!internal::isHeightFieldValid(targetMesh, data, vId, targetPoint, vertexFaceAdjacencies, heightfieldAngle) &&
               count < BINARY_SEARCH_ITERATIONS)
        {
            targetPoint = 0.5 * (targetPoint + currentPoint);

            count++;
        }

        if (count < BINARY_SEARCH_ITERATIONS) {
            targetMesh.setVertex(vId, targetPoint);
            aVertexHasMoved = true;
        }
    }
    return aVertexHasMoved;
}


/**
 * @brief Compute differential coordinates for the vertices of a mesh
 * @param[in] mesh Input mesh
 * @param[in] vertexVertexAdjacencies Vertex-vertex adjacencies
 * @return differentialCoordinates Vector of differential coordinates for each vertex
 */
std::vector<cg3::Vec3> computeDifferentialCoordinates(
        const cg3::EigenMesh& mesh,
        const std::vector<std::vector<int>>& vertexVertexAdjacencies)
{
    //Resulting vector
    std::vector<cg3::Vec3> differentialCoordinates;
    differentialCoordinates.resize(mesh.getNumberVertices());

    #pragma omp parallel for
    for(unsigned int vId = 0; vId < mesh.getNumberVertices(); ++vId) {
        //Calculate differential coordinates for each point
        cg3::Pointd currentPoint = mesh.getVertex(vId);
        cg3::Vec3 delta(0,0,0);

        const std::vector<int>& neighbors = vertexVertexAdjacencies.at(vId);
        for(const int& neighborId : neighbors) {
            delta += currentPoint - mesh.getVertex(neighborId);
        }

        delta /= neighbors.size();

        differentialCoordinates[vId] = delta;
    }

    return differentialCoordinates;
}

/**
 * @brief Get the target point using the differential coordinates with
 * respect to the neighborhood
 * @param targetMesh Target mesh
 * @param differentialCoordinates Differential coordinate of the mesh
 * @param vId Target vertex id
 * @param vertexVertexAdjacencies Vertex-vertex adjacencies of the mesh
 * @return Target point
 */
cg3::Pointd getTargetPoint(
        const cg3::EigenMesh& targetMesh,
        const std::vector<cg3::Vec3>& differentialCoordinates,
        const int vId,        
        const std::vector<std::vector<int>>& vertexVertexAdjacencies)
{
    const std::vector<int>& neighbors = vertexVertexAdjacencies.at(vId);

    cg3::Pointd delta(0,0,0);

    //Calculate delta
    for(const int& neighborId : neighbors) {
        delta += targetMesh.getVertex(neighborId);
    }
    delta /= neighbors.size();

    cg3::Pointd targetPoint = differentialCoordinates.at(vId) + delta;

    return targetPoint;
}


/**
 * @brief Validate move of a vertex. Height-field check
 * @param[in] targetMesh Target mesh
 * @param[in] data Four axis fabrication data
 * @param[in] vId Vertex id
 * @param[in] newPos New position
 * @param[in] vertexFaceAdjacencies Vertex-face adjacencies of the mesh
 * @param[in] heightfieldAngle Limit angle with triangles normal in order to be a heightfield
 * @return True if the move is valid, false otherwise
 */
bool isHeightFieldValid(
        const cg3::EigenMesh& targetMesh,
        const Data& data,
        const int vId,
        const cg3::Pointd& newPoint,
        const std::vector<std::vector<int>>& vertexFaceAdjacencies,
        const double heightfieldAngle)
{    
    const double heightfieldLimit = cos(heightfieldAngle);

    const std::vector<int>& faces = vertexFaceAdjacencies.at(vId);
    for (const int& fId : faces) {
        const cg3::Pointi face = targetMesh.getFace(fId);

        cg3::Pointd p1, p2, p3;

        //Get the triangle with new point
        if (face.x() == vId) {
            p1 = newPoint;
            p2 = targetMesh.getVertex(face.y());
            p3 = targetMesh.getVertex(face.z());
        }
        else if (face.y() == vId) {
            p1 = targetMesh.getVertex(face.x());
            p2 = newPoint;
            p3 = targetMesh.getVertex(face.z());
        }
        else {
            assert(face.z() == vId);
            p1 = targetMesh.getVertex(face.x());
            p2 = targetMesh.getVertex(face.y());
            p3 = newPoint;
        }
        cg3::Triangle3Dd triangle(p1, p2, p3);

        cg3::Vec3 faceNormal = triangle.normal();

        //Check if the triangle normal has an angle less than 90° with the given direction
        const cg3::Vec3& associatedDirection = data.directions[data.association[fId]];
        if (faceNormal.dot(associatedDirection) < heightfieldLimit)
            return false;
    }
    return true;
}

} //namespace internal

} //namespace FourAxisFabrication
