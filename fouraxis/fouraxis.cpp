#include "fouraxis.h"
#include "cg3/geometry/transformations.h"
#include "cg3/cgal/cgal_aabbtree.h"
#include <cg3/libigl/libigl.h>

#include "lib/MultiLabelOptimization/GCoptimization.h"

#ifdef GUROBI_DEFINED
#include <gurobi_c++.h>
#endif

void FourAxisFabrication::findOptimalRotation(cg3::EigenMesh& m, cg3::EigenMesh& m2) {
    Eigen::Matrix3d rot = Orientation::optimalOrientation(m);
    m.rotate(rot);
    m2.rotate(rot);
    cg3::BoundingBox b = m.getBoundingBox();
    if (b.getLengthY() > b.getLengthX() && b.getLengthY() > b.getLengthZ()){
        m.rotate(cg3::getRotationMatrix(cg3::Vec3(0,0,1), M_PI/2));
        m2.rotate(cg3::getRotationMatrix(cg3::Vec3(0,0,1), M_PI/2));
    }
    else if (b.getLengthZ() > b.getLengthX() && b.getLengthZ() > b.getLengthY()){
        m.rotate(cg3::getRotationMatrix(cg3::Vec3(0,1,0), M_PI/2));
        m2.rotate(cg3::getRotationMatrix(cg3::Vec3(0,1,0), M_PI/2));
    }
    m.translate(-m.getBoundingBox().center());
    m2.translate(-m.getBoundingBox().center());
}

int FourAxisFabrication::maxYFace(std::vector<int> &list, const cg3::EigenMesh &mesh) {
    int max = list[0];
    for(int i : list){
        if(mesh.getVertex(mesh.getFace(i).y()).y() > mesh.getVertex(mesh.getFace(max).y()).y()){
            max = i;
        }
    }
    return max;
}

struct triangleComparator {
    const cg3::EigenMesh& m;
    triangleComparator(const cg3::EigenMesh& m) : m(m) {}
    bool operator()(unsigned int f1, unsigned int f2){
        cg3::Pointi ff1 = m.getFace(f1);
        cg3::Pointi ff2 = m.getFace(f2);
        cg3::Pointd c1 = (m.getVertex(ff1.x()) + m.getVertex(ff1.y()) + m.getVertex(ff1.z()))/3;
        cg3::Pointd c2 = (m.getVertex(ff2.x()) + m.getVertex(ff2.y()) + m.getVertex(ff2.z()))/3;
        return c1 < c2;
    }
};

void FourAxisFabrication::cutExtremes(const cg3::EigenMesh &m, std::vector<unsigned int> &minExtreme, std::vector<unsigned int> &maxExtreme) {
    std::vector<unsigned int> fIndices(m.getNumberFaces());
    minExtreme.clear();
    maxExtreme.clear();
    for (unsigned int i = 0; i < fIndices.size(); i++)
        fIndices[i] = i;

    std::sort(fIndices.begin(), fIndices.end(), triangleComparator(m));
    const cg3::Vec3 minuxX(-1,0,0);
    const cg3::Vec3 plusX(1,0,0);

    unsigned int i = 0;
    while(m.getFaceNormal(fIndices[i]).dot(minuxX) >= -std::numeric_limits<double>::epsilon()){
        minExtreme.push_back(fIndices[i]);
        i++;
    }

    i = fIndices.size()-1;
    while(m.getFaceNormal(fIndices[i]).dot(plusX) >= -std::numeric_limits<double>::epsilon()){
        maxExtreme.push_back(fIndices[i]);
        i--;
    }
}

int FourAxisFabrication::minYFace(std::vector<int> &list, const cg3::EigenMesh &mesh) {
    int min = list[0];
    for(int i : list){
        if(mesh.getVertex(mesh.getFace(i).y()).y() < mesh.getVertex(mesh.getFace(min).y()).y()){
            min = i;
        }
    }
    return min;
}

void FourAxisFabrication::checkPlane(cg3::Array2D<int> &visibility, const cg3::EigenMesh &mesh, int indexPlane, int numberPlanes) {
    cg3::cgal::AABBTree tree(mesh);
    cg3::Pointi f;

    cg3::Vec3 e1, e2, e3;
    int face;
    double max = mesh.getBoundingBox().maxY()+50;
    double min = mesh.getBoundingBox().minY()-50;
    //int indexNotVisibleVector = checker.size()-1;

    for(unsigned int i = 0; i < mesh.getNumberFaces(); i++){
        f = mesh.getFace(i);
        e1 = mesh.getVertex(f.x());
        e2 = mesh.getVertex(f.y());
        e3 = mesh.getVertex(f.z());

        cg3::Pointd bar((e1+e2+e3)/3);
        //cerco le intersezioni della retta passante per la i-esima faccia
        std::vector<int> blackList;
        tree.getIntersectEigenFaces(cg3::Pointd(bar.x(), max, bar.z()), cg3::Pointd(bar.x(), min, bar.z()), blackList);
        if (blackList.size() > 0){
            //Prendo quella che si trova più in alto
            face = maxYFace(blackList, mesh);
            visibility(indexPlane, face) = 1;
            //Prendo quella che si trova più in basso
            face = minYFace(blackList, mesh);
            visibility(numberPlanes + indexPlane, face) = 1;
        }
    }
}

void FourAxisFabrication::checkVisibilityAllPlanes(const cg3::EigenMesh &mesh, cg3::Array2D<int> &visibility, int numberPlanes) {
    cg3::EigenMesh meshEigen = mesh;

    double stepAngle = 180 / numberPlanes;

    visibility.resize(numberPlanes*2 + 1, mesh.getNumberFaces());
    visibility.fill(0);

    Eigen::Matrix3d rotation;
    cg3::Vec3 axis(1,0,0);
    cg3::getRotationMatrix(axis, (stepAngle * (M_PI / 180)), rotation);

    for(int i = 0; i < numberPlanes; i++){
        checkPlane(visibility, meshEigen, i, numberPlanes);

        meshEigen.rotate(rotation);
    }

    for (unsigned int j = 0; j < visibility.getSizeY(); j++){
        bool found = false;
        for (unsigned int i = 0; i < visibility.getSizeX() && !found; i++){
            if (visibility(i,j) == 1)
                found = true;
        }
        if (!found)
            visibility(visibility.getSizeX()-1, j) = 1;
    }
}

void FourAxisFabrication::minimizeNumberPlanes(std::vector<int>& survivedPlanes, const cg3::Array2D<int> &visibility) {
    survivedPlanes.clear();
    int nOrientations = visibility.getSizeX();
    #ifdef GUROBI_DEFINED
    int nTriangles = visibility.getSizeY();
    try{
        GRBEnv env = GRBEnv();

        GRBModel model = GRBModel(env);

        GRBVar *orientation = model.addVars(nOrientations, GRB_BINARY);

        //creo le variabili o e t per gli orientamenti e per i triangoli
        for (int i = 0; i < nOrientations; i++) {
            std::ostringstream vname;
            vname << "o" << i;
            orientation[i].set(GRB_StringAttr_VarName, vname.str());
        }

        model.update();

        for(int i = 0; i < nTriangles; i++){
            GRBLinExpr sum = 0;
            for(int j = 0 ; j < nOrientations ; j++){
                sum+=orientation[j]*visibility(j,i);
            }
            model.addConstr(sum >= 1);
        }

        GRBLinExpr expr = 0;

        for (int j = 0; j < nOrientations; j++) {
            expr += orientation[j];
        }

        model.setObjective(expr, GRB_MINIMIZE);

        model.optimize();

        for (int i = 0; i < nOrientations; i++) {
            std::cout << orientation[i].get(GRB_StringAttr_VarName) << " "
                 << orientation[i].get(GRB_DoubleAttr_X) << std::endl;
            if(orientation[i].get(GRB_DoubleAttr_X) == 1) survivedPlanes.push_back(i);
        }
    }
    catch (GRBException e) {
        std::cout << "Error code = " << e.getErrorCode() << std::endl;
        std::cout << e.getMessage() << std::endl;
    }
    catch (...) {
        std::cout << "Exception during optimization" << std::endl;
    }
    #else
    for (unsigned int i = 0; i < (unsigned int)nOrientations; i++)
        survivedPlanes.push_back(i);
    #endif
}

#ifdef MULTI_LABEL_OPTIMIZATION_INCLUDED
float smoothTerm(int site_1, int site_2, int label_1, int label_2, void *extra_data) {
    CG3_SUPPRESS_WARNING(site_1);
    CG3_SUPPRESS_WARNING(site_2);
    CG3_SUPPRESS_WARNING(extra_data);
    if (label_1 == label_2) return 0.f;
    else return 2.f;
}


std::vector<int> FourAxisFabrication::getAssociation(const std::vector<int> &survivedPlanes, cg3::Array2D<int> &visibility, const cg3::EigenMesh& mesh) {
    unsigned int n_triangles = mesh.getNumberFaces();
    std::vector<int> segmentation(n_triangles);

    Eigen::MatrixXi adj = cg3::libigl::getFaceAdjacences(mesh);

    try {
        GCoptimizationGeneralGraph *gc = new GCoptimizationGeneralGraph(n_triangles, survivedPlanes.size());

        std::cerr << "Survived Planes: " << survivedPlanes.size() << "\n";

        for (unsigned int label = 0; label < survivedPlanes.size(); ++label) {
            int plane = survivedPlanes[label];
            GCoptimizationGeneralGraph::SparseDataCost *data = new GCoptimizationGeneralGraph::SparseDataCost[n_triangles];
            int dataCount = 0;
            for (unsigned int f = 0; f < mesh.getNumberFaces(); f++){
                if (visibility(plane, f) == 0){
                    GCoptimizationGeneralGraph::SparseDataCost record;
                    record.site = f;
                    record.cost = 100000;
                    data[dataCount++] = record;
                }
                else {
                    GCoptimizationGeneralGraph::SparseDataCost record;
                    record.site = f;
                    record.cost = 0;
                    data[dataCount++] = record;
                }
            }

            gc->setDataCost(label, data, dataCount);
            delete [] data;
        }

        gc->setSmoothCost(smoothTerm, nullptr);

        std::vector<bool> visited(n_triangles, false);
        for (unsigned int f = 0; f < n_triangles; f++) {
            visited[f] = true;
            for (int i=0; i<3; ++i) {
                int nid = adj(f, i);
                if (!visited[nid]) gc->setNeighbors(f, nid);
            }
        }

        gc->swap(-1); // -1 => run until convergence [convergence is guaranteed]

        for (unsigned int f = 0; f < n_triangles; f++){
            segmentation[f] = gc->whatLabel(f);
        }

        delete gc;
    }
    catch (GCException e) {
        std::cerr << "\n\n!!!GRAPH-CUT EXCEPTION!!!\nCheck logfile\n\n" << std::endl;
        e.Report();
    }
    return segmentation;
}
#endif
