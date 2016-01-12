#include "dbm.h"

/* Allocation and deallocation */

/* It allocates a DBM */
DBM *CreateDBM(int n_visible_layer_neurons,  gsl_vector *n_hidden_units, int n_labels){
    DBM *d = NULL;
    int i;
    
    d = (DBM *)malloc(sizeof(DBM));
    d->n_layers = n_hidden_units->size;
    d->m = (RBM **)malloc(d->n_layers*sizeof(RBM *));
    
    //only the first layer has the number of visible inputs equals to the number of features
    d->m[0] = CreateRBM(n_visible_layer_neurons, (int)gsl_vector_get(n_hidden_units, 0), n_labels);
    for(i = 1; i < d->n_layers; i++)
		d->m[i] = CreateRBM((int)gsl_vector_get(n_hidden_units, i-1), (int)gsl_vector_get(n_hidden_units, i), n_labels);
    
    return d;
}

/* It deallocates an DBM */
void DestroyDBM(DBM **d){
    int i;
    
    if(*d){
        for(i = 0; i < (*d)->n_layers; i++)
          if((*d)->m[i]) DestroyRBM(&(*d)->m[i]);
        free((*d)->m);
        free(*d);
    }
}
/**************************/

/* DBM initialization */

/* It initializes a DBM */
void InitializeDBM(DBM *d){
    int i;
    srand(time(NULL));
    
    for(i = 0; i < d->n_layers; i++){
        InitializeBias4VisibleUnitsWithRandomValues(d->m[i]);
        InitializeBias4HiddenUnits(d->m[i]);
        InitializeBias4LabelUnits(d->m[i]);
        InitializeWeights(d->m[i]);
        InitializeLabelWeights(d->m[i]);
    }
}


/* It performs DBM greedy pre-training step
Parameters: [D, d, n_epochs, n_samplings, batch_size, LearningType]
D: dataset
d: DBM
n_epochs: number of epochs
n_samplings: number of samplings
batch_size: mini-batch size
LearningType: type of learning algorithm [1 - CD|2 - PCD|3 - FPCD] */
double GreedyPreTrainingDBM(Dataset *D, DBM *d, int n_epochs, int n_samplings, int batch_size, int LearningType){
    double error = 0.0,aux = 0.0;
    int i, j, k, l;
    Dataset *tmp1 = NULL, *tmp2 = NULL;   
 
    
    tmp1 = CopyDataset(D);
    
    for (i = 0; i < d->n_layers;i++){
        
        if(i == 0){
            fprintf(stderr,"\n Training bottom layer ... ");
            error = Bernoulli_TrainingRBMbyCD4DBM_BottomLayer(tmp1, d->m[0], n_epochs, n_samplings, batch_size);
        }else if(i == d->n_layers - 1){
            fprintf(stderr,"\n Training top layer ... ");
            error += Bernoulli_TrainingRBMbyCD4DBM_TopLayer(tmp1, d->m[d->n_layers-1], n_epochs, n_samplings, batch_size); 
        }else{
	    fprintf(stderr,"\n Training layer %i ... ",i+1);
            error += Bernoulli_TrainingRBMbyCD4DBM_IntermediateLayers(tmp1, d->m[i], n_epochs, n_samplings, batch_size);
        }
        fprintf(stderr,"OK");
	
		/* making the hidden layer of RBM i to be the visible layer of RBM i+1 */
		
		if(i < d->n_layers-1){
			tmp2 = CopyDataset(tmp1);
			DestroyDataset(&tmp1);
			tmp1 = CreateDataset(D->size, d->m[i]->n_hidden_layer_neurons);
			tmp1->nlabels = D->nlabels;
			for(j = 0; j < tmp1->size; j++){
				//----alterei
		        for(k = 0; k < tmp1->nfeatures; k++){
		            aux = 0.0;
					for(l = 0; l < tmp2->nfeatures; l++)
						aux+=(gsl_vector_get(tmp2->sample[j].feature, l)*gsl_matrix_get(d->m[i]->W, l, k)+gsl_vector_get(tmp2->sample[j].feature, l)*gsl_matrix_get(d->m[i]->W, l, k));
					
					aux+=gsl_vector_get(d->m[i]->b, k);
					gsl_vector_set(tmp1->sample[j].feature, k, SigmoidLogistic(aux));
				}
				//fim alteracao
			    //gsl_vector_memcpy(tmp1->sample[j].feature,getProbabilityTurningOnHiddenUnit4DBM(d->m[i], tmp2->sample[j].feature));
			}
			DestroyDataset(&tmp2);
		}
    }
	DestroyDataset(&tmp1);
    
    return error;
}

/* Bernoulli DBM reconstruction */

/* It reconstructs an input dataset given a trained DBM */
double BernoulliDBMReconstruction(Dataset *D, DBM *d){
    double error = 0.0;
    int l, i;	
    for(i = 0; i < D->size; i++){        
        // going up
		gsl_vector_free(d->m[0]->v);
		d->m[0]->v = gsl_vector_alloc(d->m[0]->n_visible_layer_neurons);
		gsl_vector_memcpy(d->m[0]->v, D->sample[i].feature);

        for(l = 0; l < d->n_layers; l++){	
			gsl_vector_free(d->m[l]->h);
			d->m[l]->h = getProbabilityTurningOnHiddenUnit(d->m[l], d->m[l]->v);
                     
            if(l < d->n_layers-1){
				gsl_vector_free(d->m[l+1]->v);
				d->m[l+1]->v = gsl_vector_alloc(d->m[l+1]->n_visible_layer_neurons);
				gsl_vector_memcpy(d->m[l+1]->v,d->m[l]->h);
            }      
        }
        
        //going down
        for(l = d->n_layers-1; l > 0; l--){
			gsl_vector_free(d->m[l]->v);
            d->m[l]->v = getProbabilityTurningOnDBMIntermediateLayersOnDownPass(d->m[l], d->m[l]->h, d->m[l-1]);
            if(l >0){
				gsl_vector_free(d->m[l-1]->h);
				d->m[l-1]->h = gsl_vector_alloc(d->m[l-1]->n_hidden_layer_neurons);
				gsl_vector_memcpy(d->m[l-1]->h,d->m[l]->v);
            }  
        }

		//reconstruction of the visible layer
		gsl_vector_free(d->m[0]->v);
        d->m[0]->v= getProbabilityTurningOnVisibleUnit(d->m[0],d->m[0]->h);

        error+=getReconstructionError(D->sample[i].feature, d->m[0]->v);
        //gsl_vector_free(v_prime);
    }

    error/=D->size;
	fprintf(stderr,"Reconstruction error: %lf OK", error);
    
    return error;
}

/* It computes the probability of turning on an intermediate layer of a DBM, as show in Eq. 28 and 29  */
gsl_vector *getProbabilityTurningOnDBMIntermediateLayersOnDownPass(RBM *m, gsl_vector *h, RBM *beneath_layer){
    int i,j;
    gsl_vector *inter = NULL;
    double tmp;
    
    inter = gsl_vector_calloc(m->n_visible_layer_neurons);
    
    for(j = 0; j < m->n_visible_layer_neurons; j++){
        tmp = 0.0;
        for(i = 0; i < m->n_hidden_layer_neurons; i++)
            tmp+=(gsl_vector_get(h, i)*gsl_matrix_get(m->W, j, i));
        tmp+=gsl_vector_get(m->a, j);

        for(i = 0; i < beneath_layer->n_visible_layer_neurons; i++)
            tmp+=(gsl_vector_get(beneath_layer->v, i)*gsl_matrix_get(beneath_layer->W, i, j));
        tmp+=gsl_vector_get(m->a, j);

	
        tmp = SigmoidLogistic(tmp);
        gsl_vector_set(inter, j, tmp);
    }
    
    return inter;
}


/*double DBMDiscriminativeFineTunning(Dataset *D, DBM *d){
    gsl_vector *h2 = NULL *h1 = NULL;
    double error;
    int id, i;


    for(i = 0; i < D->size; i++){      
        h2 = ForwardPass(D->sample[i].feature, d);

		h1 = gsl_vector_calloc(d->m[1]->n_visible_layer_neurons);
        h1 = getProbabilityTurningOnVisibleUnit(d->m[1], h2);
    }
	gsl_vector_free(v);
	gsl_vector_free(h2);
}*/

/*gsl_vector *ForwardPass(gsl_vector *s, DBN *d){
    int l;
    gsl_vector *h = NULL, *v = NULL;
    
    if(d){
        v = gsl_vector_calloc(d->m[0]->n_visible_layer_neurons);
        setVisibleLayer(d->m[0], s);
        
         for each layer */
        /*for(l = 0; l < d->n_layers;  l++){
            h = gsl_vector_calloc(d->m[l]->n_hidden_layer_neurons);
            h = getProbabilityTurningOnHiddenUnit(d->m[l], v);
        
            gsl_vector_free(v);
            v = gsl_vector_calloc(d->m[l]->n_hidden_layer_neurons);
            gsl_vector_memcpy(v, h);
            gsl_vector_free(h);
        }
        return v;
    }else{
        fprintf(stderr,"\nThere is no DBN allocated @ForwardPass.\n");
        return NULL;
    }
}*/
/*
double DBMReconstruction(Dataset *D, DBM *d){
    gsl_vector *h_prime = NULL,*h_prime0 = NULL, *v_prime = NULL, *aux = NULL;
    double error, beta;
    int i,j;
    error = 0.0;

    for(j = 0; j < D->size;j++){      
		beta = 1 - ((D->size-(float)j)/D->size);

        aux = gsl_vector_calloc(d->m[0]->n_visible_layer_neurons);
        gsl_vector_memcpy(aux, D->sample[j].feature);
		for(i = 0; i < d->n_layers-1; i++){            

			h_prime = getProbabilityTurningOnHiddenUnitDBM(d->m[i],d->m[i+1], aux, beta);
			if (i==0){
				h_prime0 = gsl_vector_calloc(d->m[0]->n_hidden_layer_neurons);
                gsl_vector_memcpy(h_prime0, h_prime);
			}
            gsl_vector_free(aux);
            
            aux = gsl_vector_calloc(d->m[i+1]->n_visible_layer_neurons);
            gsl_vector_memcpy(aux, h_prime);
           
            gsl_vector_free(h_prime);
		}
		//last layer
		h_prime = getProbabilityTurningOnHiddenUnitDBMLastLayer(d->m[d->n_layers-1], aux, beta);
		gsl_vector_free(aux);
		gsl_vector_free(h_prime);

        v_prime = getProbabilityTurningOnVisibleUnitDBMFirstLayer(d->m[0], h_prime0,beta);
        gsl_vector_free(h_prime0);

        error+=getReconstructionError(D->sample[j].feature, v_prime);
        gsl_vector_free(v_prime);
    }
    error/=D->size;
    fprintf(stdout,"Reconstruction error = %lf\n", error);
    return error;
}*/

/* It computes the probability of turning on a hidden unit j, as described by Equation 38 - An Efficient Learning Procedure for Deep
Boltzmann Machines */
/*gsl_vector *getProbabilityTurningOnHiddenUnitDBM(RBM *rbm,RBM *next, gsl_vector *v, double beta){
    int i, j, m;
    gsl_vector *h = NULL;
    double tmp, tmpPre, tmpPos;
    
	//ex: h1
    h = gsl_vector_calloc(rbm->n_hidden_layer_neurons);
    for(j = 0; j < rbm->n_hidden_layer_neurons; j++){
        tmp = 0.0;
		tmpPre = 0.0;
		tmpPos = 0.0;
		//ex: v
        for(i = 0; i < rbm->n_visible_layer_neurons; i++)
            tmpPre+=(gsl_vector_get(v, i)*gsl_matrix_get(rbm->W, i, j));

		//ex: h2
        for(m = 0; m < next->n_hidden_layer_neurons; m++)
            tmpPos+=(gsl_vector_get(rbm->h, j)*gsl_matrix_get(next->W, j, m));

		tmp = beta*(tmpPre+tmpPos + gsl_vector_get(rbm->b, j));		
        
		tmp = SigmoidLogistic(tmp);
        gsl_vector_set(h, j, tmp);
    }
    
    return h;
}*/

/* It computes the probability of turning on a hidden unit j, as described by Equation 40 - An Efficient Learning Procedure for Deep
Boltzmann Machines */
/*
gsl_vector *getProbabilityTurningOnHiddenUnitDBMLastLayer(RBM *m, gsl_vector *v, double beta){
    int i, j;
    gsl_vector *h = NULL;
    double tmp;
    
    h = gsl_vector_calloc(m->n_hidden_layer_neurons);
    for(j = 0; j < m->n_hidden_layer_neurons; j++){
        tmp = 0.0;
        for(i = 0; i < m->n_visible_layer_neurons; i++)
            tmp+=(beta*(gsl_vector_get(v, i)*gsl_matrix_get(m->W, i, j)));
        tmp+=gsl_vector_get(m->b, j);
		tmp = SigmoidLogistic(tmp);
        gsl_vector_set(h, j, tmp);
    }
    
    return h;
}*/

/* It computes the probability of turning on a visible unit j, as described by Equation 41 */
/*gsl_vector *getProbabilityTurningOnVisibleUnitDBMFirstLayer(RBM *m, gsl_vector *h, double beta){
    int i,j;
    gsl_vector *v = NULL;
    double tmp;
    
    v = gsl_vector_calloc(m->n_visible_layer_neurons);
    
    for(j = 0; j < m->n_visible_layer_neurons; j++){
        tmp = 0.0;
        for(i = 0; i < m->n_hidden_layer_neurons; i++)
            tmp+=(beta*(gsl_vector_get(h, i)*gsl_matrix_get(m->W, j, i)));
        tmp+=gsl_vector_get(m->a, j);
	
        tmp = SigmoidLogistic(tmp);
        gsl_vector_set(v, j, tmp);
    }
    
    return v;
}*/




