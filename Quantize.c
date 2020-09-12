/**************************************/
#include "Colourspace.h"
#include "Quantize.h"
/**************************************/

//! Clear quantization cluster data
static inline void QuantCluster_Clear(struct QuantCluster_t *x) {
	x->nSamples   =  0;
	x->Prev       = -1;
	x->ColourDist =  0.0f;
	x->Centroid = x->Train = x->TrainWeight = x->Dist = x->DistWeight = (struct BGRAf_t){0,0,0,0};
}

//! Clear training data (ie. do not destroy the centroid or linked list position)
static inline void QuantCluster_ClearTraining(struct QuantCluster_t *x) {
	x->nSamples   = 0;
	x->ColourDist = 0.0f;
	x->Train = x->TrainWeight = x->Dist = x->DistWeight = (struct BGRAf_t){0,0,0,0};
}

//! Add data to training
//! Weights are applied to converge (and diverge what doesn't fit) more quickly.
static inline void QuantCluster_Train(struct QuantCluster_t *Dst, const struct BGRAf_t *Data, float w) {
	struct BGRAf_t Dist       = BGRAf_Sub    ( Data, &Dst->Centroid);
	struct BGRAf_t Weight     = BGRAf_Mul    (&Dist, &Dist);
	               Weight     = BGRAf_Addi   (&Weight, 1.0f + w*w);
	struct BGRAf_t DataWeight = BGRAf_InvDivi(&Weight, 1.0f);
	struct BGRAf_t wDist      = BGRAf_Mul    (&Dist, &Weight);
	struct BGRAf_t wData      = BGRAf_Mul    ( Data, &DataWeight);
	Dst->Train       = BGRAf_Add(&Dst->Train,       &wData);
	Dst->TrainWeight = BGRAf_Add(&Dst->TrainWeight, &DataWeight);
	Dst->Dist        = BGRAf_Add(&Dst->Dist,        &wDist);
	Dst->DistWeight  = BGRAf_Add(&Dst->DistWeight,  &Weight);
	Dst->ColourDist += w*w;
	Dst->nSamples++;
}

//! Resolve the centroid from training data
static inline int QuantCluster_Resolve(struct QuantCluster_t *x) {
	if(x->nSamples) {
		x->Centroid = BGRAf_Div(&x->Train, &x->TrainWeight);
		return 1;
	} else return 0;
}

//! Split a quantization cluster
static inline void QuantCluster_Split(struct QuantCluster_t *Src, struct QuantCluster_t *Dst) {
	*Dst = *Src;

	struct BGRAf_t SplitParam = BGRAf_Div(&Dst->Dist, &Dst->DistWeight);
	Dst->Centroid = BGRAf_Add(&Dst->Centroid, &SplitParam);
}

/**************************************/

//! Place a cluster in a distortion linked list (Head = Most distorted)
static int QuantCluster_InsertToDistortionList(struct QuantCluster_t *Clusters, int Idx, int Head) {
	int Next = -1;
	int Prev = Head;
	float Dist = Clusters[Idx].ColourDist;
	if(Dist != 0.0f) {
		while(Prev != -1 && Dist < Clusters[Prev].ColourDist) {
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
		QuantCluster_Train(&Clusters[0], &Data[i], 0.0f);
	}
	if(!QuantCluster_Resolve(&Clusters[0])) return; //! Empty set

	//! Second pass to properly train the distortion measures
	QuantCluster_ClearTraining(&Clusters[0]);
	for(i=0;i<nData;i++) {
		DataClusters[i] = 0;
		QuantCluster_Train(&Clusters[0], &Data[i], BGRAf_ColDistance(&Data[i], &Clusters[0].Centroid));
	}
	if(Clusters[0].ColourDist == 0.0f) return; //! Global convergence already reached (ie. single item)
	Clusters[0].Prev = -1;

	//! Begin splitting clusters to form the initial codebook
	int nClusterCur = 1;
	int MaxDistCluster = 0;
	int EmptyCluster = -1;
	while(MaxDistCluster != -1 && nClusterCur < nCluster) {
		if(EmptyCluster == -1) {
			EmptyCluster = nClusterCur;
			Clusters[EmptyCluster].Prev = -1;
		}

		//! Split the most distorted cluster and rematch its data
		QuantCluster_Split(&Clusters[MaxDistCluster], &Clusters[EmptyCluster]);
		QuantCluster_ClearTraining(&Clusters[MaxDistCluster]);
		QuantCluster_ClearTraining(&Clusters[EmptyCluster]);
		for(i=0;i<nData;i++) if(DataClusters[i] == MaxDistCluster) {
			float DistA = BGRAf_ColDistance(&Data[i], &Clusters[MaxDistCluster].Centroid);
			float DistB = BGRAf_ColDistance(&Data[i], &Clusters[EmptyCluster  ].Centroid);
			if(DistA < DistB) { QuantCluster_Train(&Clusters[MaxDistCluster], &Data[i], DistA); }
			else              { QuantCluster_Train(&Clusters[EmptyCluster  ], &Data[i], DistB); DataClusters[i] = EmptyCluster; }
		}

		//! Do both clusters resolve?
		int ResolveBits = 0;
		ResolveBits |= QuantCluster_Resolve(&Clusters[MaxDistCluster]) << 0;
		ResolveBits |= QuantCluster_Resolve(&Clusters[EmptyCluster  ]) << 1;
		if(ResolveBits == 0b11) {
			//! Re-link the linked list and continue
			MaxDistCluster = QuantCluster_InsertToDistortionList(Clusters, MaxDistCluster, Clusters[MaxDistCluster].Prev);
			MaxDistCluster = QuantCluster_InsertToDistortionList(Clusters, EmptyCluster,   MaxDistCluster);
			EmptyCluster = -1;
			nClusterCur++;
		} else {
			//! Splitting didn't help this cluster. Try the next one
			MaxDistCluster = Clusters[MaxDistCluster].Prev;
			if((ResolveBits & 2)) {
				//! Clusters got swapped places: swap back
				int t = EmptyCluster;
				EmptyCluster = MaxDistCluster;
				MaxDistCluster = t;
			}
		}
	}

	//! Perform relaxation/refinement passes
	if(nPasses) do {
		//! Clear all cluster training
		for(i=0;i<nCluster;i++) QuantCluster_ClearTraining(&Clusters[i]);

		//! Fit data
		for(i=0;i<nData;i++) {
			int   BestIdx  = -1;
			float BestDist = 0x1.0p126;
			for(j=0;j<nCluster;j++) {
				float Dist = BGRAf_ColDistance(&Data[i], &Clusters[j].Centroid);
				if(Dist < BestDist) BestIdx = j, BestDist = Dist;
			}
			DataClusters[i] = BestIdx;
			QuantCluster_Train(&Clusters[BestIdx], &Data[i], BestDist);
		}

		//! Resolve clusters
		MaxDistCluster = -1;
		EmptyCluster   = -1;
		for(i=0;i<nCluster;i++) {
			//! If the cluster resolves, update the distortion linked list
			if(QuantCluster_Resolve(&Clusters[i])) {
				MaxDistCluster = QuantCluster_InsertToDistortionList(Clusters, i, MaxDistCluster);
			} else {
				//! No resolve - append to empty-cluster linked list
				Clusters[i].Prev = EmptyCluster;
				EmptyCluster = i;
			}
		}

		//! Split the most distorted clusters into any empty ones
		while(EmptyCluster != -1 && MaxDistCluster != -1) {
			int PrevEmpty = Clusters[EmptyCluster].Prev;
			QuantCluster_Split(&Clusters[MaxDistCluster], &Clusters[EmptyCluster]);
			MaxDistCluster = Clusters[MaxDistCluster].Prev;
			EmptyCluster   = PrevEmpty;
		}
	} while(--nPasses);
}

/**************************************/
//! EOF
/**************************************/
