#ifndef FAF_EXTREMES_H
#define FAF_EXTREMES_H

#include <cg3/meshes/eigenmesh/eigenmesh.h>

#include "faf_data.h"

namespace FourAxisFabrication {

/* Extremes selection */

void selectExtremesOnXAxis(
        const cg3::EigenMesh &mesh,
        Data& data);

}


#endif // FAF_EXTREMES_H
