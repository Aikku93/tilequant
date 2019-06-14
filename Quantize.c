/**************************************/
#include <math.h>
/**************************************/
#include "PxType.h"
#include "Quantize.h"
/**************************************/
#define MAX_EMPTY_SPLIT_PASSES 3
/**************************************/
#define DIST_WEIGHT (struct YUVAf_t){1,1,1,1}
/**************************************/

//! Clear training data
static inline void TrainingClear(struct QuantCluster_t *x) {
	x->TrainSum = x->TrainDst = (struct YUVAf_t){0,0,0,0};
	x->TrainCnt = 0;
}

//! Add data to training
static inline void TrainingAdd(struct QuantCluster_t *Dst, const struct YUVAf_t *Data, const struct YUVAf_t *Dist) {
	struct YUVAf_t Out = YUVAf_SignedSquare(Data);
	Dst->TrainSum = YUVAf_Add(&Dst->TrainSum, &Out);
	Dst->TrainDst = YUVAf_Add(&Dst->TrainDst, Dist);
	Dst->TrainCnt++;
}

//! Resolve the centroid from training data
//! Note that an empty set will resolve to {0}
static inline int CentroidResolve(struct QuantCluster_t *x) {
	if(!x->TrainCnt) {
		x->Centroid = (struct YUVAf_t){0,0,0,0};
		return 0;
	} else {
		x->Centroid = YUVAf_Divi(&x->TrainSum, x->TrainCnt);
		x->Centroid = YUVAf_SignedSqrt(&x->Centroid);
		return 1;
	}
}

/**************************************/

//! Resolve distances from cluster centroids
void QuantCluster_SetDistances(struct QuantCluster_t *Clusters, int nCluster, const struct YUVAf_t *Data, int nData, int32_t *DataClusters) {
	int i;

	//! Clear cluster distortions
	for(i=0;i<nCluster;i++) Clusters[i].TrainDst = (struct YUVAf_t){0,0,0,0};

	//! Accumulate distortions from new data
	for(i=0;i<nData;i++) {
		       int     Idx = DataClusters[i];
		struct YUVAf_t Dst = YUVAf_Dist2(&Data[i], &Clusters[Idx].Centroid);
		Clusters[Idx].TrainDst = YUVAf_Add(&Clusters[Idx].TrainDst, &Dst);
	}
}

/**************************************/

//! Split a set of clusters into the target number of clusters
//! This will split the most distorted clusters first
int QuantCluster_SetSplit(struct QuantCluster_t *Set, int n, int nNew) {
	int i, j;
	for(i=n;i<nNew;i++) {
		//! Get most distorted cluster
		//! Note that we are also including the 'new' clusters:
		//!  When a cluster is split, its distortion is evenly
		//!  distributed between it and its spawned cluster.
		//!  This allows splitting a very heavily-disorted
		//!  cluster as much as necessary
		int   MaxIdx = -1;
		float MaxDst = 0.0f;
		for(j=0;j<i;j++) {
			float Dst = YUVAf_Dot(&Set[j].TrainDst, &DIST_WEIGHT);
			if(Dst > MaxDst) MaxIdx = j, MaxDst = Dst;
		}

		//! If we still have no distortion here, then global convergence
		//! has been reached, and no more splitting is required
		if(MaxIdx == -1) return -1;

		//! Split the clusters
		QuantCluster_Split(&Set[MaxIdx], &Set[i]);
	}
	return i;
}

/**************************************/

//! Perform a vector quantization pass
void QuantCluster_VQPass(struct QuantCluster_t *Clusters, int nCluster, const struct YUVAf_t *Data, int nData, int32_t *DataClusters) {
	int i, j, Pass;
	for(Pass=0;Pass<MAX_EMPTY_SPLIT_PASSES;Pass++) {
		//! Clear training
		for(i=0;i<nCluster;i++) TrainingClear(&Clusters[i]);

		//! Correlate and merge
		for(i=0;i<nData;i++) {
			//! Find minimum distance cluster
			int   MinIdx  = 0;
			float MinDist = 1.0e30f;
			for(j=0;j<nCluster;j++) {
				float Dist = YUVAf_ColDistance(&Data[i], &Clusters[j].Centroid);
				if(Dist < MinDist) MinIdx = j, MinDist = Dist;
			}

			//! Merge
			struct YUVAf_t Dist = YUVAf_Dist2(&Data[i], &Clusters[MinIdx].Centroid);
			TrainingAdd(&Clusters[MinIdx], &Data[i], &Dist);
			DataClusters[i] = MinIdx;
		}

		//! Resolve clusters
		int nResolves = 0;
		for(i=0;i<nCluster;i++) {
			//! Cluster resolves?
			if(CentroidResolve(&Clusters[i])) {
				nResolves++;
				continue;
			}

			//! Find most distorted cluster
			int   MaxIdx = -1;
			float MaxDst = 0.0f;
			for(j=0;j<nCluster;j++) {
				float Dst = YUVAf_Dot(&Clusters[j].TrainDst, &DIST_WEIGHT);
				if(Dst > MaxDst && Clusters[j].TrainCnt > 2) MaxIdx = j, MaxDst = Dst;
			}

			//! Have a cluster to split?
			if(MaxIdx != -1) {
				QuantCluster_Split(&Clusters[MaxIdx], &Clusters[i]);
			} else nResolves++;
		}

		//! If fully resolved, exit. Otherwise, try again
		if(nResolves == nCluster) break;
	}
}

/**************************************/

//! Perform total vector quantization
void QuantCluster_Quantize(struct QuantCluster_t *Clusters, int nCluster, const struct YUVAf_t *Data, int nData, int32_t *DataClusters, int nPasses) {
	int i;

	//! Perform first pass from average of data
	QuantCluster_Clear(&Clusters[0]);
	for(i=0;i<nData;i++) {
		TrainingAdd(&Clusters[0], &Data[i], &(struct YUVAf_t){0,0,0,0});
		DataClusters[i] = 0;
	}
	if(!CentroidResolve(&Clusters[0])) return; //! Empty set

	//! Begin splitting clusters
	int nClusterCur = 1;
	while(nClusterCur < nCluster) {
		//! Get distances
		//! This is used by the set-splitting function below
		QuantCluster_SetDistances(Clusters, nClusterCur, Data, nData, DataClusters);

		//! Perform splits
		int nClusterNew = nClusterCur * 2;
		if(nClusterNew > nCluster) nClusterNew = nCluster;
		if(QuantCluster_SetSplit(Clusters, nClusterCur, nClusterNew) == -1) break;
		nClusterCur = nClusterNew;

		//! Do relaxation passes
		for(i=0;i<nPasses;i++) QuantCluster_VQPass(Clusters, nClusterCur, Data, nData, DataClusters);
	}
}

/**************************************/
//! EOF
/**************************************/
