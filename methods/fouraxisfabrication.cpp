/**
 * @author Stefano Nuvoli
 * @author Alessandro Muntoni
 */
#include "fouraxisfabrication.h"


namespace FourAxisFabrication {


/* ----- FOUR AXIS FABRICATION ----- */

/**
 * @brief Compute the entire algorithm for four axis fabrication.
 *
 * @param[out] mesh Original mesh
 * @param[out] smoothedMesh Smoothed mesh
 * @param[in] nDirections Number of directions to check
 * @param[in] deterministic Deterministic approach (if false it is randomized)
 * @param[in] nDirections Number of directions
 * @param[in] fixExtremeAssociation Set if faces in the extremes must be already and unconditionally
 * assigned to the x-axis directions
 * @param[in] setCoverage Flag to resolve set coverage problem or not
 * @param[in] compactness Compactness
 * @param[in] limitAngle Limit angle
 * @param[in] Number of iterations of the algorithm for restoring frequencies
 * @param data Four axis fabrication data
 * @param[in] checkMode Visibility check mode. Default is projection mode.
 */
void computeEntireAlgorithm(
        cg3::EigenMesh& mesh,
        cg3::EigenMesh& smoothedMesh,
        const unsigned int nOrientations,
        const bool deterministic,
        const unsigned int nDirections,
        const bool fixExtremeAssociation,
        const bool setCoverage,
        const double compactness,
        const double limitAngle,
        const int frequenciesIterations,
        Data& data,
        CheckMode checkMode)
{

    //Get optimal mesh orientation
    FourAxisFabrication::rotateToOptimalOrientation(
                mesh,
                smoothedMesh,
                nOrientations,
                deterministic);


    //Get extremes on x-axis to be cut
    FourAxisFabrication::selectExtremesOnXAxis(smoothedMesh, data);


    //Initialize data before visibility check
    FourAxisFabrication::initializeDataForVisibilityCheck(
                smoothedMesh,
                nDirections,
                fixExtremeAssociation,
                data);


    //Visibility check
    FourAxisFabrication::checkVisibility(
                smoothedMesh,
                nDirections,
                data,
                checkMode);


    //Detect non-visible faces
    FourAxisFabrication::detectNonVisibleFaces(
                data);


    //Get the target directions
    FourAxisFabrication::getTargetDirections(
                setCoverage,
                data);

    //Get optimized association
    FourAxisFabrication::getOptimizedAssociation(
                smoothedMesh,
                compactness,
                limitAngle,
                data);

    //Restore frequencies
    FourAxisFabrication::restoreFrequencies(
                mesh,
                data,
                frequenciesIterations,
                smoothedMesh);

    //Cut components
    FourAxisFabrication::cutComponents(
                smoothedMesh,
                data);

}


}