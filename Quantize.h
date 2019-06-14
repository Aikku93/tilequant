/**************************************/
#pragma once
/**************************************/
#include <math.h>
/**************************************/
#include "PxType.h"
/**************************************/

struct QuantCluster_t {
	struct YUVAf_t Centroid;
	struct YUVAf_t TrainSum;
	struct YUVAf_t TrainDst;
	       int     TrainCnt;
};

/**************************************/

//! Clear quantization cluster data
static inline void QuantCluster_Clear(struct QuantCluster_t *x) {
	x->Centroid = x->TrainSum = x->TrainDst = (struct YUVAf_t){0,0,0,0};
	x->TrainCnt = 0;
}

//! Split a quantization cluster
//! NOTE: The distortion measure is assumed to be equally distributed
static inline void QuantCluster_Split(struct QuantCluster_t *Src, struct QuantCluster_t *Dst) {
	Src->TrainDst = YUVAf_Muli(&Src->TrainDst, 0.5f);
	*Dst = *Src;

	struct YUVAf_t SplitParam;
	SplitParam = YUVAf_Divi(&Src->TrainDst, Src->TrainCnt);
	SplitParam = YUVAf_Sqrt(&SplitParam);
	Dst->Centroid = YUVAf_Add(&Dst->Centroid, &SplitParam);
	Src->Centroid = YUVAf_Sub(&Src->Centroid, &SplitParam);
}

//! Resolve distances from cluster centroids
//! NOTE: This relies on knowing where each data point ended up,
//! which is obtained from DataClusters in QuantCluster_VQPass().
void QuantCluster_SetDistances(struct QuantCluster_t *Clusters, int nCluster, const struct YUVAf_t *Data, int nData, int32_t *DataClusters);

//! Split a set of clusters into the target number of clusters
//! and return the final number of clusters
//! NOTE: This will split the most distorted clusters first
//! NOTE: This will destroy the stored distortion data for the
//! original set
//! NOTE: If the set has reached global convergence, -1 is returned
int QuantCluster_SetSplit(struct QuantCluster_t *Set, int n, int nNew);

//! Perform a vector quantization pass, and store the
//! cluster index for the data into DataClusters
//! NOTE: When a cluster winds up empty, the cluster with
//! largest amount of distortion is split into it
void QuantCluster_VQPass(struct QuantCluster_t *Clusters, int nCluster, const struct YUVAf_t *Data, int nData, int32_t *DataClusters);

//! Perform total vector quantization
//! Under most circumstances, this is all that needs to be called.
//! To get the quantized values, extract them from the cluster centroids.
void QuantCluster_Quantize(struct QuantCluster_t *Clusters, int nCluster, const struct YUVAf_t *Data, int nData, int32_t *DataClusters, int nPasses);

/**************************************/
//! EOF
/**************************************/
