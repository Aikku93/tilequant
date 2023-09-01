/**************************************/
#pragma once
/**************************************/
#include "colourspace.h"
/**************************************/

struct QuantCluster_t
{
    int   Next;
    int   nPoints;
    float TrainWeight;
    float DistWeight;
    struct BGRAf_t Train;
    struct BGRAf_t Dist;
    struct BGRAf_t Centroid;
};

/**************************************/

//! Perform total vector quantization
void QuantCluster_Quantize(struct QuantCluster_t *Clusters, int nCluster, const struct BGRAf_t *Data, int nData, int32_t *DataClusters, int nPasses);

/**************************************/
//! EOF
/**************************************/
