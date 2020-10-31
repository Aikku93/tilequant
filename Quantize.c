/**************************************/
#include "Colourspace.h"
#include "Quantize.h"
/**************************************/

//! Clear training data (NOTE: Do NOT destroy the centroid or linked list position)
static inline void QuantCluster_ClearTraining(struct QuantCluster_t *x) {
	x->nPoints = 0;
	x->Train = x->DistCenter = x->DistWeight = (struct BGRAf_t){0,0,0,0};
}

//! Add data to training
//! The weights used mimic a least-squares formulation, where
//! 'less matching' data is accumulated as sort of 'where this
//! data wants to be', and 'more matching' data is accumulated
//! as 'how much this data belongs'
static inline void QuantCluster_Train(struct QuantCluster_t *Dst, const struct BGRAf_t *Data) {
	struct BGRAf_t Dist = BGRAf_Sub( Data, &Dst->Centroid);
	               Dist = BGRAf_Mul(&Dist, &Dist);
	struct BGRAf_t wData = BGRAf_Mul(Data, &Dist);
	Dst->nPoints++;
	Dst->Train       = BGRAf_Add(&Dst->Train, Data);
	Dst->DistCenter  = BGRAf_Add(&Dst->DistCenter, &wData);
	Dst->DistWeight  = BGRAf_Add(&Dst->DistWeight, &Dist);
}

//! Resolve the centroid from training data
static inline int QuantCluster_Resolve(struct QuantCluster_t *x) {
	if(x->nPoints) x->Centroid = BGRAf_Divi(&x->Train, x->nPoints);
	return x->nPoints;
}

//! Split a quantization cluster
//! NOTE: When DistWeight's values are 0, this means that it already had
//! a perfect match in the last iteration, and the centroid (even after
//! resolving) should still be the same, and so we replace the value
//! we split to with that.
static inline void QuantCluster_Split(struct QuantCluster_t *Clusters, int SrcCluster, int DstCluster, const struct BGRAf_t *Data, int nData, int32_t *DataClusters) {
	Clusters[DstCluster].Centroid = BGRAf_DivSafe(&Clusters[SrcCluster].DistCenter, &Clusters[SrcCluster].DistWeight, &Clusters[SrcCluster].Centroid);

	int n;
	QuantCluster_ClearTraining(&Clusters[SrcCluster]);
	QuantCluster_ClearTraining(&Clusters[DstCluster]);
	for(n=0;n<nData;n++) if(DataClusters[n] == SrcCluster) {
		float DistSrc = BGRAf_ColDistance(&Data[n], &Clusters[SrcCluster].Centroid);
		float DistDst = BGRAf_ColDistance(&Data[n], &Clusters[DstCluster].Centroid);
		if(DistSrc < DistDst) {
			QuantCluster_Train(&Clusters[SrcCluster], &Data[n]);
		} else {
			QuantCluster_Train(&Clusters[DstCluster], &Data[n]);
			DataClusters[n] = DstCluster;
		}
	}
	QuantCluster_Resolve(&Clusters[SrcCluster]);
	QuantCluster_Resolve(&Clusters[DstCluster]);
}

//! Distortion metric for splitting
static inline float SplitDistortionMetric(const struct QuantCluster_t *x) {
	return BGRAf_Len2(&x->DistWeight);
}

/**************************************/

//! Place a cluster in a distortion linked list (Head = Most distorted)
static int QuantCluster_InsertToDistortionList(struct QuantCluster_t *Clusters, int Idx, int Head) {
	int Next = -1;
	int Prev = Head;
	float Dist = SplitDistortionMetric(&Clusters[Idx]);
	if(Dist != 0.0f) {
		while(Prev != -1 && Dist < SplitDistortionMetric(&Clusters[Prev])) {
			Next = Prev;
			Prev = Clusters[Prev].Prev;
		}
		Clusters[Idx].Prev = Prev;
		if(Next != -1) Clusters[Next].Prev = Idx;
		else Head = Idx;
	}
	return Head;
}

/**************************************/

//! Perform total vector quantization
void QuantCluster_Quantize(struct QuantCluster_t *Clusters, int nCluster, const struct BGRAf_t *Data, int nData, int32_t *DataClusters, int nPasses) {
	int i, j;
	if(!nData) return;

	//! Perform first pass from average of data
	QuantCluster_ClearTraining(&Clusters[0]);
	for(i=0;i<nData;i++) {
		DataClusters[i] = 0;
		Clusters[0].Centroid = BGRAf_Add(&Clusters[0].Centroid, &Data[i]);
	}
	Clusters[0].Centroid = BGRAf_Divi(&Clusters[0].Centroid, nData);;

	//! Second pass to properly train the distortion measures
	QuantCluster_ClearTraining(&Clusters[0]);
	for(i=0;i<nData;i++) QuantCluster_Train(&Clusters[0], &Data[i]);
	if(BGRAf_Len2(&Clusters[0].DistWeight) == 0.0f) return; //! Global convergence already reached (ie. single item)
	Clusters[0].Prev = -1;

	//! Begin splitting clusters to form the initial codebook
	int nClusterCur = 1;
	int MaxDistCluster = 0;
	int EmptyCluster = -1;
	while(MaxDistCluster != -1 && nClusterCur < nCluster) {
		//! Split the most distorted cluster into a new one
		{
			//! Setting N=1 uses iterative splitting (slow)
			//! Setting N=nClusterCur uses binary splitting (faster)
			//! Both settings have different pros/cons, but binary splitting
			//! is generally good enough for most cases
			int N = nClusterCur;
			for(i=0;i<N;i++) {
				//! Find the target cluster index and update the EmptyCluster linked list
				int DstCluster = EmptyCluster;
				if(DstCluster == -1) DstCluster = nClusterCur++;
				else EmptyCluster = Clusters[DstCluster].Prev;

				//! Split and recluster
				int SrcCluster = MaxDistCluster;
				MaxDistCluster = Clusters[SrcCluster].Prev;
				QuantCluster_Split(Clusters, SrcCluster, DstCluster, Data, nData, DataClusters);
				MaxDistCluster = QuantCluster_InsertToDistortionList(Clusters, SrcCluster, MaxDistCluster);
				MaxDistCluster = QuantCluster_InsertToDistortionList(Clusters, DstCluster, MaxDistCluster);

				//! Check if we have more clusters that need splitting
				MaxDistCluster = Clusters[MaxDistCluster].Prev;
				if(MaxDistCluster == -1) break;

				//! If we already hit the maximum number of clusters, break out
				if(nClusterCur >= nCluster) break;
			}
		}

		//! Perform refinement passes
		int Pass;
		for(Pass=0;Pass<nPasses;Pass++) {
			for(i=0;i<nClusterCur;i++) QuantCluster_ClearTraining(&Clusters[i]);
			for(i=0;i<nData;i++) {
				int   BestIdx  = -1;
				float BestDist = 8.0e37f; //! Arbitrarily large number
				for(j=0;j<nClusterCur;j++) {
					float Dist = BGRAf_ColDistance(&Data[i], &Clusters[j].Centroid);
					if(Dist < BestDist) BestIdx = j, BestDist = Dist;
				}
				DataClusters[i] = BestIdx;
				QuantCluster_Train(&Clusters[BestIdx], &Data[i]);
			}

			//! Resolve clusters
			int nResolves  =  0;
			MaxDistCluster = -1;
			EmptyCluster   = -1;
			for(i=0;i<nClusterCur;i++) {
				//! If the cluster resolves, update the distortion linked list
				if(QuantCluster_Resolve(&Clusters[i])) {
					MaxDistCluster = QuantCluster_InsertToDistortionList(Clusters, i, MaxDistCluster);
					nResolves++;
				} else {
					//! No resolve - append to empty-cluster linked list
					Clusters[i].Prev = EmptyCluster;
					EmptyCluster = i;
				}
			}

			//! Split the most distorted clusters into any empty ones
			while(EmptyCluster != -1 && MaxDistCluster != -1) {
				int SrcCluster = MaxDistCluster; MaxDistCluster = Clusters[SrcCluster].Prev;
				int DstCluster = EmptyCluster;   EmptyCluster   = Clusters[DstCluster].Prev;
				MaxDistCluster = Clusters[SrcCluster].Prev;
				QuantCluster_Split(Clusters, SrcCluster, DstCluster, Data, nData, DataClusters);
				MaxDistCluster = QuantCluster_InsertToDistortionList(Clusters, SrcCluster, MaxDistCluster);
				MaxDistCluster = QuantCluster_InsertToDistortionList(Clusters, DstCluster, MaxDistCluster);
			}
		}
	}
}

/**************************************/
//! EOF
/**************************************/
