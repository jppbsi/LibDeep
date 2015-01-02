#include "dbn.h"

/* Allocation and deallocation */

/* It allocates an DBN */
DBN *CreateDBN(int n_visible_layers, int n_hidden_layers, int n_labels, int n_layers){
    DBN *d = NULL;
    int i;
    
    d = (DBN *)malloc(sizeof(DBN));
    d->n_layers = n_layers;
    d->m = (RBM **)malloc(d->n_layers*sizeof(RBM *));
    
    //only the first layer has the number of visible inputs equals to the number of features
    d->m[0] = CreateRBM(n_visible_layers, n_hidden_layers, n_labels);
    for(i = 1; i < d->n_layers; i++)
        d->m[i] = CreateRBM(n_hidden_layers, n_hidden_layers, n_labels);
    
    return d;
}

/* It deallocates an DBN */
void DestroyDBN(DBN **d){
    int i;
    
    if(*d){
        for(i = 0; i < (*d)->n_layers; i++)
            if((*d)->m[i]) DestroyRBM(&(*d)->m[i]);
        free((*d)->m);
        free(*d);
    }
}
/**************************/

/* DBN initialization */

/* It initializes an DBN */
void InitializeDBN(DBN *d){
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
/**************************/

/* DBN information */

/* It writes the weight matrix as PGM images */
/* Notice: to visualize the weights, we need squared images and a number of hidden layers that has an integer square root (n=16, for instance) */
/*void DBNSaveWeights(DBN *d, char *path){
    int i, j, z, w, l, width, height;
    double min, max;
    IplImage *img = NULL;
    CvScalar s;
    gsl_vector_view v;
    char filename[256];
    
    for(l = 0; l < d->n_layers; l++){
        fprintf(stderr,"\nd->m[%d]->n_visible_layer_neurons: %d", l, (int)sqrt(d->m[l]->n_visible_layer_neurons));
    
        width = (int)sqrt(d->m[l]->n_visible_layer_neurons);
        height = (int)sqrt(d->m[l]->n_visible_layer_neurons);
        for(z = 0; z < d->m[l]->W->size2; z++){
            img = cvCreateImage(cvSize(width,height),IPL_DEPTH_8U,1);
        
            // It normalizes weights within the interval [0,1]
            v = gsl_matrix_column(d->m[l]->W, z);
            min = gsl_vector_min(&v.vector);
            gsl_vector_add_constant(&v.vector, -min);
            max = gsl_vector_max(&v.vector);
            gsl_vector_scale(&v.vector, 1.0/max);
        
            // It transforms the weights in a 8bits/pixel image
            gsl_vector_scale(&v.vector, 255);
        
            w = 0;
            for(i = 0; i < img->height; i++){
                for(j = 0; j < img->width; j++){
                    s.val[0] = (double)round(gsl_vector_get(&v.vector, w++));
                    cvSet2D(img,i,j,s);
                }
            }
            sprintf(filename, "%s/layer_%d_weight_image_%d.pgm", path, l+1, z);
            if(!cvSaveImage(filename,img, 0)) fprintf(stderr,"\nCould not save %s", filename);
            cvReleaseImage(&img);
        }

    }
}*/
/**************************/

/* Bernoulli DBN training */

/* It trains an DBN for image reconstruction */
double BernoulliDBNTrainingbyContrastiveDivergence(Dataset *D, DBN *d, int n_epochs, int n_CD_iterations, int batch_size){
    double error, aux;
    Dataset *tmp1 = NULL, *tmp2 = NULL;
    int i, j, z, id;
    
    tmp1 = CopyDataset(D);
    
    for(id = 0; id < d->n_layers; id++){
        fprintf(stderr,"\nTraining layer %d ... ", id+1);
        error = BernoulliRBMTrainingbyContrastiveDivergence(tmp1, d->m[id], n_epochs, n_CD_iterations, batch_size);
        
        /* it updates the last layer to be the input to the next RBM */
        tmp2 = CopyDataset(tmp1);
        DestroyDataset(&tmp1);
        tmp1 = CreateDataset(D->size, d->m[id]->n_hidden_layer_neurons);
        for(z = 0; z < tmp1->size; z++){
            for(j = 0; j < tmp1->nfeatures; j++){
                aux = 0.0;
                for(i = 0; i < tmp2->nfeatures; i++)
                    aux+=(gsl_vector_get(tmp2->sample[z].feature, i)*gsl_matrix_get(d->m[id]->W, i, j));
                aux+=gsl_vector_get(d->m[id]->b, j);
                
                gsl_vector_set(tmp1->sample[z].feature, j, SigmoidLogistic(aux));
            }
        }
        DestroyDataset(&tmp2);
        fprintf(stderr,"\nOK");
    }
    DestroyDataset(&tmp1);
    
    error = BernoulliDBNReconstruction(D, d);
    
    return error;
}
/**************************/

/* Bernoulli DBN reconstruction */

/* It reconstructs an input dataset given a trained DBN */
double BernoulliDBNReconstruction(Dataset *D, DBN *d){
    gsl_vector *h_prime = NULL, *v_prime = NULL, *aux = NULL;
    double error = 0.0;
    int l, i;
    
    for(i = 0; i < D->size; i++){        
        // going up
        aux = gsl_vector_calloc(d->m[0]->n_visible_layer_neurons);
        gsl_vector_memcpy(aux, D->sample[i].feature);
        
        for(l = 0; l < d->n_layers; l++){
            h_prime = getProbabilityTurningOnHiddenUnit(d->m[l], aux);
            gsl_vector_free(aux);
            
            if(l < d->n_layers-1){
                aux = gsl_vector_calloc(d->m[l+1]->n_visible_layer_neurons);
                gsl_vector_memcpy(aux, h_prime);
            }
            
            gsl_vector_free(h_prime);
        }
        
        //going down
        aux = gsl_vector_calloc(d->m[l-1]->n_hidden_layer_neurons);
        gsl_vector_memcpy(aux, d->m[l-1]->h);
        for(l = d->n_layers-1; l >= 0; l--){
            v_prime = getProbabilityTurningOnVisibleUnit(d->m[l], aux);
            gsl_vector_free(aux);
            
            if(l > 0){
                aux = gsl_vector_calloc(d->m[l-1]->n_hidden_layer_neurons);
                gsl_vector_memcpy(aux, v_prime);
                gsl_vector_free(v_prime);
            }
        }
        
        error+=getReconstructionError(D->sample[i].feature, v_prime);
        gsl_vector_free(v_prime);
    }

    error/=D->size;
    
    return error;
}

/* Backpropagation fine-tuning ****************/

/* It executes the forward pass for a given sample s, and outputs the net's response for that sample */
gsl_vector *ForwardPass(gsl_vector *s, DBN *d){
    int l;
    gsl_vector *h = NULL, *v = NULL;
    
    if(d){
        v = gsl_vector_calloc(d->m[0]->n_visible_layer_neurons);
        setVisibleLayer(d->m[0], s);
        
        /* for each layer */
        for(l = 0; l < d->n_layers;  l++){
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
}

void ComputeBackPropagateError(gsl_vector *s, gsl_vector *output, DBN *d){
    
}

/**********************************************/


/* Image reconstruction */

// It reconstructs an input image given a trained DBN
/*IplImage *DBNReconstructImage(DBN *d, IplImage *img){
    int i, j, w;
    CvScalar s;
    gsl_vector *input = NULL, *h_prime = NULL, *v_prime = NULL, *aux = NULL;
    IplImage *output = NULL;
    
    output = cvCreateImage(cvSize(img->width,img->height),IPL_DEPTH_8U,1);
    input = gsl_vector_calloc(d->m[0]->n_visible_layer_neurons);
    
    w = 0;
    for(i = 0; i < img->height; i++){
        for(j = 0; j < img->width; j++){
            s = cvGet2D(img,i,j);
            gsl_vector_set(input, w, s.val[0]);
            if(gsl_vector_get(input, w) == 255) gsl_vector_set(input, w, 1.0); 
            w++;
        }
    }
    
    // going up
    aux = gsl_vector_calloc(d->m[0]->n_visible_layer_neurons);
    gsl_vector_memcpy(aux, input);
    gsl_vector_free(input);
    
    for(j = 0; j < d->n_layers; j++){
        h_prime = gsl_vector_calloc(d->m[j]->n_hidden_layer_neurons);
        
        h_prime = getProbabilityTurningOnHiddenUnit(d->m[j], aux);
        gsl_vector_free(aux);
        
        if(j < d->n_layers-1){
            aux = gsl_vector_calloc(d->m[j+1]->n_visible_layer_neurons);
            gsl_vector_memcpy(aux, h_prime);
        }
        
        gsl_vector_free(h_prime);
    }
    
    //going down
    aux = gsl_vector_calloc(d->m[j-1]->n_hidden_layer_neurons);
    gsl_vector_memcpy(aux, d->m[j-1]->h);
    for(j = d->n_layers-1; j >= 0; j--){
        v_prime = gsl_vector_calloc(d->m[j]->n_visible_layer_neurons);
        
        v_prime = getProbabilityTurningOnVisibleUnit(d->m[j], aux);
        gsl_vector_free(aux);
        
        if(j > 0){
            aux = gsl_vector_calloc(d->m[j-1]->n_hidden_layer_neurons);
            gsl_vector_memcpy(aux, v_prime);
            gsl_vector_free(v_prime);
        }
    }
    
    w = 0;
    for(i = 0; i < output->height; i++){
        for(j = 0; j < output->width; j++){
            s.val[0] = (double)round(gsl_vector_get(v_prime, w++));
            if(s.val[0]) s.val[0] = 255;
            cvSet2D(output,i,j,s);
        }
    }
    
    gsl_vector_free(v_prime);
    
    return output;
}*/