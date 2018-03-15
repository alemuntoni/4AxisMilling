/**
 * @author Stefano Nuvoli
 * @author Alessandro Muntoni
 */
#ifndef FOURAXISFABRICATIONMANAGER_H
#define FOURAXISFABRICATIONMANAGER_H


#include <cg3/viewer/mainwindow.h>
#include <cg3/viewer/utilities/loadersaver.h>

#include <cg3/viewer/interfaces/drawable_object.h>
#include <cg3/viewer/drawable_objects/drawable_eigenmesh.h>

#include <cg3/data_structures/arrays/array2d.h>

#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QDebug>
#include <QFrame>

#include "methods/fouraxisfabrication.h"

namespace Ui {
class FourAxisFabricationManager;
}

class FourAxisFabricationManager : public QFrame
{
    Q_OBJECT

public:

    /* Constructor/Destructors */

    explicit FourAxisFabricationManager(QWidget *parent = 0);
    ~FourAxisFabricationManager();


private:    

    /* Data fields */

    cg3::EigenMesh originalMesh;
    cg3::EigenMesh smoothedMesh;

    FourAxisFabrication::Data data;


    /* Drawable objects */

    cg3::DrawableEigenMesh drawableOriginalMesh;
    cg3::DrawableEigenMesh drawableSmoothedMesh;
    cg3::DrawableEigenMesh drawableMinResult;
    cg3::DrawableEigenMesh drawableMaxResult;
    cg3::DrawableEigenMesh drawableFourAxisResult;


    /* UI Fields */

    Ui::FourAxisFabricationManager* ui;
    cg3::viewer::MainWindow& mainWindow;

    cg3::viewer::LoaderSaver loaderSaverObj;

    std::string loadedMeshFile;
    std::string loadedSmoothedMeshFile;

    bool isMeshLoaded;
    bool isMeshOriented;
    bool areExtremesSelected;
    bool isVisibilityChecked;
    bool areTargetDirectionsFound;
    bool isAssociationComputed;
    bool areFrequenciesRestored;
    bool areComponentsCut;


    /* UI methods */

    void initialize();
    void updateUI();    
    void clearData();

    /* Computing methods */

    void computeEntireAlgorithm();

    void optimalOrientation();
    void selectExtremes();
    void checkVisibility();
    void getTargetDirections();
    void getAssociation();
    void restoreFrequencies();
    void cutComponents();


    /* Visualization methods */

    void updateDrawableMeshes();
    void addDrawableResults();

    void resetCameraDirection();
    void setCameraDirection(cg3::Vec3 dir);

    void initializeVisualizationSlider();
    void updateVisualization();

    void visualizeMesh();
    void visualizeExtremes();
    void visualizeVisibility();
    void visualizeTargetDirections();
    void visualizeAssociation();


private slots:

    /* UI slots Mesh */

    void on_loadMeshButton_clicked();
    void on_clearMeshButton_clicked();
    void on_reloadMeshButton_clicked();
    void on_saveMeshButton_clicked();


    /* UI slots Four Axis Fabrication */

    void on_computeEntireAlgorithmButton_clicked();

    void on_optimalOrientationButton_clicked();
    void on_selectExtremesButton_clicked();
    void on_checkVisibilityButton_clicked();
    void on_targetDirectionsButton_clicked();
    void on_getAssociationButton_clicked();
    void on_restoreFrequenciesButton_clicked();
    void on_cutComponentsButton_clicked();


    /* UI slots Transformations */

    void on_centerOnOriginButton_clicked();

    void on_plusXButton_clicked();
    void on_minusXButton_clicked();
    void on_plusYButton_clicked();
    void on_minusYButton_clicked();
    void on_plusZButton_clicked();
    void on_minusZButton_clicked();

    void on_rotateButton_clicked();

    void on_scaleButton_clicked();

    void on_inverseScaleButton_clicked();


    /* UI slots Visulization */

    void on_meshRadio_clicked();
    void on_extremesRadio_clicked();
    void on_visibilityRadio_clicked();
    void on_targetDirectionsRadio_clicked();
    void on_associationRadio_clicked();

    void on_visualizationSlider_valueChanged(int value);
    void on_resetCameraButton_clicked();
};

#endif // FOURAXISFABRICATIONMANAGER_H