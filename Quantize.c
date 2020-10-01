/**************************************/
#include "Colourspace.h"
#include "Quantize.h"
/**************************************/

//! Clear quantization cluster data
static inline void QuantCluster_Clear(struct QuantCluster_t *x) {
	x->Prev = -1;
	x->Centroid = x->Train = x->TrainWeight = x->Dist = x->DistWeight = (struct BGRAf_t){0,0,0,0};
}

//! Clear training data (ie. do not destroy the centroid or linked list position)
static inline void QuantCluster_ClearTraining(struct QuantCluster_t *x) {
	x->Train = x->TrainWeight = x->Dist = x->DistWeight = (struct BGRAf_t){0,0,0,0};
}

//! Add data to training
//! The weights used mimic a least-squares formulation, where
//! 'less matching' data is accumulated as sort of 'where this
//! data wants to be', and 'more matching' data is accumulated
//! as 'how much this data belongs'
static inline void QuantCluster_Train(struct QuantCluster_t *Dst, const struct BGRAf_t *Data) {
	struct BGRAf_t Dist = BGRAf_Sub( Data, &Dst->Centroid);
	               Dist = BGRAf_Abs(&Dist);
	struct BGRAf_t DistWeight = BGRAf_Mul (&Dist, &Dist);
	struct BGRAf_t DataWeight = BGRAf_Addi(&Dist, 1.0f);
	               DataWeight = BGRAf_Mul (&DataWeight, &DataWeight);
	               DataWeight = BGRAf_InvDivi(&DataWeight, 1.0f);
	struct BGRAf_t wDist = BGRAf_Mul(Data, &DistWeight);
	struct BGRAf_t wData = BGRAf_Mul(Data, &DataWeight);
	Dst->Train       = BGRAf_Add(&Dst->Train,       &wData);
	Dst->TrainWeight = BGRAf_Add(&Dst->TrainWeight, &DataWeight);
	Dst->Dist        = BGRAf_Add(&Dst->Dist,        &wDist);
	Dst->DistWeight  = BGRAf_Add(&Dst->DistWeight,  &DistWeight);
}

//! Resolve the centroid from training data
static inline int QuantCluster_Resolve(struct QuantCluster_t *x) {
	if( //! As long as a single value is non-zero, this cluster contains data
		x->TrainWeight.b != 0.0f/* ||
		x->TrainWeight.g != 0.0f ||
		x->TrainWeight.r != 0.0f ||
		x->TrainWeight.a != 0.0f*/
	) {
		x->Centroid = BGRAf_Div(&x->Train, &x->TrainWeight);
		return 1;
	} else return 0;
}

//! Split a quantization cluster
//! NOTE: When DistWeight's values are 0, this means that it already had
//! a perfect match in the last iteration, and the centroid (even after
//! resolving) should still be the same, and so we replace the value
//! we split to with that.
static inline void QuantCluster_Split(struct QuantCluster_t *Src, struct QuantCluster_t *Dst) {
	Dst->Centroid = BGRAf_DivSafe(&Src->Dist, &Src->DistWeight, &Src->Centroid);
}

//! NOTE: Cluster must already be resolved before calling this
static inline float SplitDistortionMetric(const struct QuantCluster_t *x) {
	struct BGRAf_t DistTarget = BGRAf_DivSafe(&x->Dist, &x->DistWeight, &x->Centroid);
	DistTarget = BGRAf_Sub(&DistTarget, &x->Centroid);
	return BGRAf_Len2(&DistTarget);
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

	//! Perform first pass from average of data
	QuantCluster_Clear(&Clusters[0]);
	for(i=0;i<nData;i++) {
		DataClusters[i] = 0;
		QuantCluster_Train(&Clusters[0], &Data[i]);
	}
	if(!QuantCluster_Resolve(&Clusters[0])) return; //! Empty set

	//! Second pass to properly train the distortion measures
	QuantCluster_ClearTraining(&Clusters[0]);
	for(i=0;i<nData;i++) {
		DataClusters[i] = 0;
		QuantCluster_Train(&Clusters[0], &Data[i]);
	}
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
				Clusters[nClusterCur].Prev = EmptyCluster, EmptyCluster = nClusterCur;
				QuantCluster_Split(&Clusters[MaxDistCluster], &Clusters[EmptyCluster]);

				//! New cluster added - update count and distortion linked list
				nClusterCur++;
				if(nClusterCur >= nCluster) break;
				MaxDistCluster = Clusters[MaxDistCluster].Prev;
				if(MaxDistCluster == -1) break;
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
			//! NOTE: Do not split on the last pass
			while(EmptyCluster != -1 && MaxDistCluster != -1) {
				int PrevEmpty = Clusters[EmptyCluster].Prev;
				QuantCluster_Split(&Clusters[MaxDistCluster], &Clusters[EmptyCluster]);
				MaxDistCluster = Clusters[MaxDistCluster].Prev;
				EmptyCluster   = PrevEmpty;
			}
		}
	}
}

/**************************************/
//! EOF
/**************************************/
