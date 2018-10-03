/**
 * @author Stefano Nuvoli
 * @author Alessandro Muntoni
 */
#ifndef FOURAXISFABRICATIONMANAGER_H
#define FOURAXISFABRICATIONMANAGER_H


#include <cg3/viewer/mainwindow.h>
#include <cg3/viewer/utilities/loadersaver.h>

#include <cg3/viewer/interfaces/drawable_object.h>
#include <cg3/viewer/interfaces/drawable_container.h>
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

    explicit FourAxisFabricationManager(QWidget *parent = nullptr);
    ~FourAxisFabricationManager();


private:    

    /* Data fields */

    FourAxisFabrication::Data data;


    /* Drawable objects */

    cg3::DrawableEigenMesh drawableOriginalMesh;
    cg3::DrawableEigenMesh drawableSmoothedMesh;

    cg3::DrawableEigenMesh drawableRestoredMesh;

    cg3::DrawableEigenMesh drawableMinComponent;
    cg3::DrawableEigenMesh drawableMaxComponent;
    cg3::DrawableEigenMesh drawableFourAxisComponent;

    std::vector<cg3::DrawableEigenMesh> drawableSurfaces;
    cg3::DrawableContainer drawableSurfacesContainer;
    cg3::DrawableEigenMesh drawableMinSurface;
    cg3::DrawableEigenMesh drawableMaxSurface;

    std::vector<cg3::DrawableEigenMesh> drawableStocks;
    cg3::DrawableContainer drawableStocksContainer;

    std::vector<cg3::DrawableEigenMesh> drawableResults;
    cg3::DrawableContainer drawableResultsContainer;
    cg3::DrawableEigenMesh drawableMinResult;
    cg3::DrawableEigenMesh drawableMaxResult;

    cg3::DrawableEigenMesh drawableMinSupport;
    cg3::DrawableEigenMesh drawableMaxSupport;


    /* UI Fields */

    Ui::FourAxisFabricationManager* ui;
    cg3::viewer::MainWindow& mainWindow;

    cg3::viewer::LoaderSaver loaderSaverObj;
    cg3::viewer::LoaderSaver loaderSaverData;


    /* UI methods */

    void initialize();
    void updateUI();    
    void clearData();

    /* Computing methods */

    void optimalOrientation();
    void selectExtremes();
    void checkVisibility();
    void getTargetDirections();
    void getAssociation();
    void optimizeAssociation();
    void restoreFrequencies();
    void recheckVisibilityAfterRestore();
    void cutComponents();
    void extractResults();


    /* Visualization methods */

    void addDrawableMeshes();
    void addDrawableRestoredMesh();
    void addDrawableCutComponents();
    void addDrawableResults();
    void updateDrawableMeshes();
    void updateDrawableRestoredMesh();

    void deleteDrawableObjects();

    void resetCameraDirection();
    void setCameraDirection(const cg3::Vec3& dir);

    void initializeVisualizationSlider();
    void updateVisualizationColors();



    void colorizeMesh();
    void colorizeExtremes();
    void colorizeVisibility();
    void colorizeTargetDirections();
    void colorizeAssociation();
    void colorizeAssociation(
            cg3::DrawableEigenMesh& drawableMesh,
            const std::vector<int>& association,
            const std::vector<unsigned int>& targetDirections,
            const std::vector<unsigned int>& nonVisibleFaces);
    void showCurrentStatusDescription();


private slots:

    /* UI slots Mesh */

    void on_loadMeshButton_clicked();
    void on_clearMeshButton_clicked();
    void on_reloadMeshButton_clicked();
    void on_saveResultsButton_clicked();

    /* UI slots for saving/loading data */

    void on_loadDataButton_clicked();
    void on_saveDataButton_clicked();


    /* UI slots Four Axis Fabrication */

    void on_optimalOrientationButton_clicked();
    void on_selectExtremesButton_clicked();
    void on_checkVisibilityButton_clicked();
    void on_targetDirectionsButton_clicked();
    void on_getAssociationButton_clicked();
    void on_optimizationButton_clicked();
    void on_restoreFrequenciesButton_clicked();
    void on_recheckVisibilityButton_clicked();
    void on_cutComponentsButton_clicked();
    void on_extractResultsButton_clicked();


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

    void on_showNonVisibleCheck_clicked();
    void on_resetCameraButton_clicked();
    void on_visualizationSlider_valueChanged(int value);
};

#endif // FOURAXISFABRICATIONMANAGER_H
