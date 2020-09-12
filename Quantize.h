/**************************************/
#pragma once
/**************************************/
#include "Colourspace.h"
/**************************************/

struct QuantCluster_t {
	int nSamples;
	int Prev;
	float ColourDist;
	struct BGRAf_t Centroid;
	struct BGRAf_t Train;
	struct BGRAf_t TrainWeight;
	struct BGRAf_t Dist;
	struct BGRAf_t DistWeight;
};

/**************************************/

//! Perform total vector quantization
void QuantCluster_Quantize(struct QuantCluster_t *Clusters, int nCluster, const struct BGRAf_t *Data, int nData, int32_t *DataClusters, int nPasses);

/**************************************/
//! EOF
/**************************************/
