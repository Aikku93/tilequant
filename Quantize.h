/**************************************/
#pragma once
/**************************************/
#include "Colourspace.h"
/**************************************/

struct QuantCluster_t {
	int Prev;
	int nPoints;
	struct BGRAf_t Centroid;
	struct BGRAf_t Train;
	struct BGRAf_t DistCenter;
	struct BGRAf_t DistWeight;
};

/**************************************/

//! Perform total vector quantization
void QuantCluster_Quantize(struct QuantCluster_t *Clusters, int nCluster, const struct BGRAf_t *Data, int nData, int32_t *DataClusters, int nPasses);

/**************************************/
//! EOF
/**************************************/
