#include "OPF.h"
#include "deep.h"
#include "opt.h"

int main(int argc, char **argv){

    if(argc != 11){
        fprintf(stderr,"\nusage DBN <training set> <test set> <output results file name> <cross-validation iteration number> \
                <input parameters file> <n_epochs> <batch_size> <number of iterations for Constrastive Divergence> \
                <1 - CD | 2 - PCD | 3 - FPCD> <number of DBN layers>\n");
        exit(-1);
    }
    int iteration = atoi(argv[4]), i, j, n_epochs = atoi(argv[6]), batch_size = atoi(argv[7]), n_gibbs_sampling = atoi(argv[8]), op = atoi(argv[9]);
    gsl_vector *n_hidden_units = NULL, *eta = NULL, *lambda = NULL, *alpha = NULL, *eta_min = NULL, *eta_max = NULL, *p = NULL, *q = NULL;
    int n_layers = atoi(argv[10]); 
    double temp_eta, temp_lambda, temp_alpha, temp_eta_min, temp_eta_max, temp_p, temp_q, temp_hidden_units;
    double errorTRAIN, errorTEST;
    char *fileName = argv[5];
    FILE *fp = NULL;
    Dataset *DatasetTrain = NULL, *DatasetTest = NULL;
    DBN *d = NULL;
    
    Subgraph *Train = NULL, *Test = NULL;
    Train = ReadSubgraph(argv[1]);
    Test = ReadSubgraph(argv[2]);
    
    DatasetTrain = Subgraph2Dataset(Train);
    DatasetTest = Subgraph2Dataset(Test);
    

    n_hidden_units = gsl_vector_alloc(n_layers);
    eta = gsl_vector_alloc(n_layers);	
    lambda = gsl_vector_alloc(n_layers);
    alpha = gsl_vector_alloc(n_layers);
    eta_min = gsl_vector_alloc(n_layers);
    eta_max = gsl_vector_alloc(n_layers);
    p = gsl_vector_alloc(n_layers);
    q = gsl_vector_alloc(n_layers);

    fp = fopen(fileName, "r");
    if(!fp){
            fprintf(stderr,"\nUnable to open file %s.\n", fileName);
            exit(1);
    }

    j = 0;

    for(i = 0; i < n_layers; i++){
	fscanf(fp, "%lf %lf %lf %lf", &temp_hidden_units, &temp_eta, &temp_lambda, &temp_alpha);
	WaiveComment(fp);
	fscanf(fp, "%lf %lf", &temp_eta_min, &temp_eta_max);
	WaiveComment(fp);
	fscanf(fp, "%lf %lf", &temp_p, &temp_q);
	WaiveComment(fp);
        gsl_vector_set(n_hidden_units, i, temp_hidden_units);
	gsl_vector_set(eta, i, temp_eta);
	gsl_vector_set(lambda, i, temp_lambda);
	gsl_vector_set(alpha, i, temp_alpha);	
        gsl_vector_set(eta_min, i, temp_eta_min);
	gsl_vector_set(eta_max, i, temp_eta_max);
	gsl_vector_set(p, i, temp_p);
	gsl_vector_set(q, i, temp_q);
    }
    fclose(fp);

    fprintf(stderr,"\nCreating and initializing DBN ... ");
    
    d = CreateDBN(Train->nfeats, n_hidden_units, Train->nlabels, n_layers);    
    InitializeDBN(d);
    for(i = 0; i < d->n_layers; i++){
	d->m[i]->eta = gsl_vector_get(eta,i); 
        d->m[i]->lambda = gsl_vector_get(lambda,i); 
        d->m[i]->alpha = gsl_vector_get(alpha,i); 
        d->m[i]->eta_min = gsl_vector_get(eta_min,i); 
        d->m[i]->eta_max = gsl_vector_get(eta_max,i);
    }   
    
    fprintf(stderr,"\nOk\n");
    
    fprintf(stderr,"\nTraining DBN ...\n");
    
    switch (op){
        case 1:
            errorTRAIN = BernoulliDBNTrainingbyContrastiveDivergencewithDropout(DatasetTrain, d, n_epochs, n_gibbs_sampling, batch_size, p, q);
        break;
        case 2:
            errorTRAIN = BernoulliDBNTrainingbyPersistentContrastiveDivergencewithDropout(DatasetTrain, d, n_epochs, n_gibbs_sampling, batch_size, p, q);
        break;
        case 3:
            errorTRAIN = BernoulliDBNTrainingbyFastPersistentContrastiveDivergencewithDropout(DatasetTrain, d, n_epochs, n_gibbs_sampling, batch_size, p, q);
        break;
    }

    
    fprintf(stderr,"\nOK\n");
    
    fprintf(stderr,"\nRunning DBN for reconstruction ... ");
    errorTEST = BernoulliDBNReconstructionwithDropout(DatasetTest, d, p, q);
    fprintf(stderr,"\nOK\n");
        
    fp = fopen(argv[3], "a");
    fprintf(fp,"\n%d %lf %lf", iteration, errorTRAIN, errorTEST);
    fclose(fp);
    
    fprintf(stderr,"\nTraining Error: %lf \nTesting Error: %lf\n\n", errorTRAIN, errorTEST);
    
    DestroyDataset(&DatasetTrain);
    DestroyDataset(&DatasetTest);
    DestroySubgraph(&Train);
    DestroySubgraph(&Test);
    DestroyDBN(&d);
    gsl_vector_free(n_hidden_units);
    gsl_vector_free(lambda);
    gsl_vector_free(eta);
    gsl_vector_free(alpha);
    gsl_vector_free(eta_min);
    gsl_vector_free(eta_max);
    gsl_vector_free(p);
    gsl_vector_free(q);
    
    return 0;
}