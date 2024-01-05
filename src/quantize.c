/**************************************/
#include <math.h>
/**************************************/
#include "colourspace.h"
#include "quantize.h"
/**************************************/

//! Clear training data (NOTE: Do NOT destroy the centroid or linked list position)
static inline void QuantCluster_ClearTraining(struct QuantCluster_t *x)
{
    x->nPoints = 0;
    x->MaxDistIdx = -1;
    x->MaxDistVal = 0.0f;
    x->Train = (struct BGRAf_t)
    {
        0,0,0,0
    };
}

//! Convert data error to distance measure
static inline float QuantCluster_ErrorToDist(struct BGRAf_t *x) {
#if 0 //! Minimize L2 norm
    float Dist = BGRAf_Len2(x);
#else //! Minimize L1 norm - this seems to give better results
    struct BGRAf_t xAbs = BGRAf_Abs(x);
    float Dist = BGRAf_Sum(&xAbs);
#endif
    return Dist;
}

//! Add data to training
static inline void QuantCluster_Train(struct QuantCluster_t *Dst, const struct BGRAf_t *Data, int DataIdx)
{
    struct BGRAf_t Error = BGRAf_Sub(Data, &Dst->Centroid);
    float Dist = QuantCluster_ErrorToDist(&Error);
    if(Dist > Dst->MaxDistVal) Dst->MaxDistIdx = DataIdx, Dst->MaxDistVal = Dist;
    Dst->Train = BGRAf_Add(&Dst->Train, Data);
    Dst->nPoints++;
}

//! Resolve the centroid from training data
static inline int QuantCluster_Resolve(struct QuantCluster_t *x)
{
    if(x->nPoints) x->Centroid = BGRAf_Divi(&x->Train, x->nPoints);
    return x->nPoints;
}

//! Split a quantization cluster
static inline void QuantCluster_Split(struct QuantCluster_t *Clusters, int SrcCluster, int DstCluster, const struct BGRAf_t *Data, int nData, int32_t *DataClusters, int Recluster)
{
    //! Create a new cluster from this "most-distorted" data - this helps
    //! us make it out of a local optimum into a better cluster fit
    Clusters[DstCluster].Centroid = Data[Clusters[SrcCluster].MaxDistIdx];
#if 0 //! This hurts more than it helps
    //! ... and remove said cluster from the original centroid so we can
    //! correctly assign the new clusters. This can have some floating-point
    //! error in the subtraction, but hopefully this will be negligible
    Clusters[SrcCluster].Train = BGRAf_Sub(&Clusters[SrcCluster].Train, &Data[Clusters[SrcCluster].MaxDistIdx]);
    Clusters[SrcCluster].nPoints--;
    Clusters[SrcCluster].Centroid = BGRAf_Divi(&Clusters[SrcCluster].Train, Clusters[SrcCluster].nPoints);
#endif
    //! Re-assign clusters
    if(Recluster)
    {
        int n;
        QuantCluster_ClearTraining(&Clusters[SrcCluster]);
        QuantCluster_ClearTraining(&Clusters[DstCluster]);
        for(n=0; n<nData; n++) if(DataClusters[n] == SrcCluster)
            {
                struct BGRAf_t SrcError = BGRAf_Sub(&Data[n], &Clusters[SrcCluster].Centroid);
                struct BGRAf_t DstError = BGRAf_Sub(&Data[n], &Clusters[DstCluster].Centroid);
                float DistSrc = QuantCluster_ErrorToDist(&SrcError);
                float DistDst = QuantCluster_ErrorToDist(&DstError);
                if(DistSrc < DistDst)
                {
                    QuantCluster_Train(&Clusters[SrcCluster], &Data[n], n);
                }
                else
                {
                    QuantCluster_Train(&Clusters[DstCluster], &Data[n], n);
                    DataClusters[n] = DstCluster;
                }
            }
        QuantCluster_Resolve(&Clusters[SrcCluster]);
        QuantCluster_Resolve(&Clusters[DstCluster]);
    }
}

/**************************************/

//! Place a cluster in a distortion linked list (Head = Most distorted)
static int QuantCluster_InsertToDistortionList(struct QuantCluster_t *Clusters, int Idx, int Head)
{
    int Next = Head;
    int Prev = -1;
    float Dist = Clusters[Idx].MaxDistVal;
    if(Dist != 0.0f)
    {
        while(Next != -1 && Dist < Clusters[Next].MaxDistVal)
        {
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
void QuantCluster_Quantize(struct QuantCluster_t *Clusters, int nCluster, const struct BGRAf_t *Data, int nData, int32_t *DataClusters, int nPasses)
{
    int i, j;
    if(!nData) return;

    //! Perform first pass from average of data
    Clusters[0].Centroid = (struct BGRAf_t)
    {
        0,0,0,0
    };
    for(i=0; i<nData; i++)
    {
        DataClusters[i] = 0;
        Clusters[0].Centroid = BGRAf_Add(&Clusters[0].Centroid, &Data[i]);
    }
    Clusters[0].Centroid = BGRAf_Divi(&Clusters[0].Centroid, nData);

    //! Second pass to properly train the distortion measures
    QuantCluster_ClearTraining(&Clusters[0]);
    for(i=0; i<nData; i++) QuantCluster_Train(&Clusters[0], &Data[i], i);
    if(Clusters[0].MaxDistVal == 0.0f) return; //! Global convergence already reached (ie. single item)
    Clusters[0].Next = -1;

    //! Begin splitting clusters to form the initial codebook
    int nClusterCur = 1;
    int MaxDistCluster = 0;
    int EmptyCluster = -1;
    float LastTotalError = INFINITY;
    while(nClusterCur < nCluster)
    {
        //! Split the most distorted cluster into a new one
        {
            //! Setting N=1 uses iterative splitting (slow)
            //! Setting N=nClusterCur uses binary splitting (faster)
            //! We use binary splitting, and just use more refinement passes,
            //! as this is much faster for the same convergence rate.
            int N = nClusterCur;
            do
            {
                //! If we've run out of pre-determined clusters, brutefroce a search now
                //! This can happen if we split clusters into empty ones in the last pass
                int SrcCluster = MaxDistCluster;
                if(SrcCluster != -1) MaxDistCluster = Clusters[SrcCluster].Next;
                else {
                    float MaxDist = 0.0f;
                    for(i=0;i<nClusterCur;i++) {
                        if(Clusters[i].MaxDistVal > MaxDist) {
                            SrcCluster = i;
                            MaxDist = Clusters[i].MaxDistVal;
			}
		    }
                }

                //! Find the target cluster index and update the EmptyCluster linked list
                int DstCluster;
                if(EmptyCluster != -1) DstCluster = EmptyCluster, EmptyCluster = Clusters[EmptyCluster].Next;
                else {
                    //! Generated as many clusters as possible?
                    if(nClusterCur == nCluster) break;
		    DstCluster = nClusterCur++, N--;
		}

                //! Split cluster
                QuantCluster_Split(Clusters, SrcCluster, DstCluster, Data, nData, DataClusters, 1);
            } while(N > 0);
        }

        //! Perform refinement passes
        int Pass;
        float ThisTotalError = 0.0f;
        float ClusterLastError = INFINITY;
        for(Pass=0; Pass<nPasses; Pass++)
        {
            ThisTotalError = 0.0f;
            for(i=0; i<nClusterCur; i++) QuantCluster_ClearTraining(&Clusters[i]);
            for(i=0; i<nData; i++)
            {
                int   BestIdx  = -1;
                float BestDist = INFINITY;
                for(j=0; j<nClusterCur; j++)
                {
                    float Dist = BGRAf_ColDistance(&Data[i], &Clusters[j].Centroid);
                    if(Dist < BestDist) BestIdx = j, BestDist = Dist;
                }
                ThisTotalError += BestDist;
                DataClusters[i] = BestIdx;
                QuantCluster_Train(&Clusters[BestIdx], &Data[i], i);
            }

            //! Resolve clusters
            MaxDistCluster = -1;
            EmptyCluster   = -1;
            for(i=0; i<nClusterCur; i++)
            {
                if(QuantCluster_Resolve(&Clusters[i]))
                {
                    //! If the cluster resolves, update the distortion linked list
                    MaxDistCluster = QuantCluster_InsertToDistortionList(Clusters, i, MaxDistCluster);
                }
                else
                {
                    //! No resolve - append to empty-cluster linked list
                    Clusters[i].Next = EmptyCluster, EmptyCluster = i;
                }
            }

            //! Split the most distorted clusters into any empty ones
            while(EmptyCluster != -1 && MaxDistCluster != -1)
            {
                int SrcCluster = MaxDistCluster;
                int DstCluster = EmptyCluster;
                QuantCluster_Split(Clusters, SrcCluster, DstCluster, Data, nData, DataClusters, 1);
                MaxDistCluster = Clusters[SrcCluster].Next;
                EmptyCluster   = Clusters[DstCluster].Next;
            }

            //! Stop when solution stops moving
            if(ThisTotalError == 0.0f || ThisTotalError == ClusterLastError) break;
            ClusterLastError = ThisTotalError;
        }

	//! If we've stopped converging, early exit
	if(ThisTotalError == 0.0f || ThisTotalError == LastTotalError) break;
	LastTotalError = ThisTotalError;
    }
}

/**************************************/
//! EOF
/**************************************/
