/**************************************/
#include "Colourspace.h"
#include "Quantize.h"
/**************************************/

//! Clear training data (NOTE: Do NOT destroy the centroid or linked list position)
static inline void QuantCluster_ClearTraining(struct QuantCluster_t *x) {
	x->nPoints = 0;
	x->TrainWeight = x->DistWeight = 0.0f;
	x->Train = x->Dist = (struct BGRAf_t){0,0,0,0};
}

//! Add data to training
static inline void QuantCluster_Train(struct QuantCluster_t *Dst, const struct BGRAf_t *Data) {
	struct BGRAf_t Dist = BGRAf_Sub(Data, &Dst->Centroid);

	float DistW  = BGRAf_Len2(&Dist);
	float TrainW = 0.01f + DistW; //! <- This will help outliers pop out more often (must not be 0.0!)
	DistW *= 1.0f + fabsf(Dist.b); //! <- Further penalize distortion by luma distortion
	struct BGRAf_t TrainData = BGRAf_Muli( Data, TrainW);
	struct BGRAf_t DistData  = BGRAf_Muli(&Dist, DistW);
	Dst->TrainWeight += TrainW, Dst->Train = BGRAf_Add(&Dst->Train, &TrainData);
	Dst->DistWeight  += DistW,  Dst->Dist  = BGRAf_Add(&Dst->Dist,  &DistData);
	Dst->nPoints++;
}

//! Resolve the centroid from training data
static inline int QuantCluster_Resolve(struct QuantCluster_t *x) {
	if(x->nPoints) x->Centroid = BGRAf_Divi(&x->Train, x->TrainWeight);
	return x->nPoints;
}

//! Split a quantization cluster
static inline void QuantCluster_Split(struct QuantCluster_t *Clusters, int SrcCluster, int DstCluster, const struct BGRAf_t *Data, int nData, int32_t *DataClusters, int Recluster) {
	//! Shift the cluster in either direction of the distortion vector
	struct BGRAf_t Dist = BGRAf_Divi(&Clusters[SrcCluster].Dist, Clusters[SrcCluster].DistWeight);
	Clusters[DstCluster].Centroid = BGRAf_Add(&Clusters[SrcCluster].Centroid, &Dist);
	Clusters[SrcCluster].Centroid = BGRAf_Sub(&Clusters[SrcCluster].Centroid, &Dist);

	//! Re-assign clusters
	if(Recluster) {
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
}

/**************************************/

//! Place a cluster in a distortion linked list (Head = Most distorted)
static int QuantCluster_InsertToDistortionList(struct QuantCluster_t *Clusters, int Idx, int Head) {
	int Next = Head;
	int Prev = -1;
	float Dist = Clusters[Idx].DistWeight;
	if(Dist != 0.0f) {
		while(Next != -1 && Dist < Clusters[Next].DistWeight) {
			Prev = Next;
			Next = Clusters[Next].Next;
		}
		Clusters[Idx].Next = Next;
		if(Prev != -1) Clusters[Prev].Next = Idx;
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
	Clusters[0].Centroid = (struct BGRAf_t){0,0,0,0};
	QuantCluster_ClearTraining(&Clusters[0]);
	for(i=0;i<nData;i++) {
		DataClusters[i] = 0;
		Clusters[0].Centroid = BGRAf_Add(&Clusters[0].Centroid, &Data[i]);
	}
	Clusters[0].Centroid = BGRAf_Divi(&Clusters[0].Centroid, nData);

	//! Second pass to properly train the distortion measures
	QuantCluster_ClearTraining(&Clusters[0]);
	for(i=0;i<nData;i++) QuantCluster_Train(&Clusters[0], &Data[i]);
	if(Clusters[0].DistWeight == 0.0f) return; //! Global convergence already reached (ie. single item)
	Clusters[0].Next = -1;

	//! Begin splitting clusters to form the initial codebook
	int nClusterCur = 1;
	int MaxDistCluster = 0;
	int EmptyCluster = -1;
	while(MaxDistCluster != -1 && nClusterCur < nCluster) {
		//! Split the most distorted cluster into a new one
		{
			//! Setting N=1 uses iterative splitting (slow)
			//! Setting N=nClusterCur uses binary splitting (faster)
			//! We use binary splitting, and just use more refinement passes,
			//! as this is much faster for the same convergence rate.
			int N = nClusterCur;
			for(i=0;i<N;i++) {
				//! If we already hit the maximum number of clusters, break out
				if(nClusterCur >= nCluster) break;

				//! Find the target cluster index and update the EmptyCluster linked list
				int DstCluster;
				if(EmptyCluster != -1) DstCluster = EmptyCluster, EmptyCluster = Clusters[EmptyCluster].Next;
				else DstCluster = nClusterCur++;

				//! Split cluster, but do NOT recluster the data.
				//! By not re-clustering, we give outliers a better chance
				//! of making it through to a better-fitting cluster.
				QuantCluster_Split(Clusters, MaxDistCluster, DstCluster, Data, nData, DataClusters, 0);

				//! Check if we have more clusters that need splitting
				MaxDistCluster = Clusters[MaxDistCluster].Next;
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
			MaxDistCluster = -1;
			EmptyCluster   = -1;
			for(i=0;i<nClusterCur;i++) {
				//! If the cluster resolves, update the distortion linked list
				if(QuantCluster_Resolve(&Clusters[i])) {
					//! Only insert to the list if the distortion is non-zero
					if(Clusters[i].DistWeight != 0.0f) {
						MaxDistCluster = QuantCluster_InsertToDistortionList(Clusters, i, MaxDistCluster);
					}
				} else {
					//! No resolve - append to empty-cluster linked list
					Clusters[i].Next = EmptyCluster, EmptyCluster = i;
				}
			}

			//! Split the most distorted clusters into any empty ones
			while(EmptyCluster != -1 && MaxDistCluster != -1) {
				QuantCluster_Split(Clusters, MaxDistCluster, EmptyCluster, Data, nData, DataClusters, 1);
				MaxDistCluster = Clusters[MaxDistCluster].Next;
				EmptyCluster   = Clusters[EmptyCluster].Next;
			}
		}
	}
}

/**************************************/
//! EOF
/**************************************/
