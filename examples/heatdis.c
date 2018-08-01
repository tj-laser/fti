/**
 *  @file   heatdis.c
 *  @author Leonardo A. Bautista Gomez
 *  @date   May, 2014
 *  @brief  Heat distribution code to test FTI.
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fti.h>
#include <unistd.h>
#include <string.h>

char * status_string( int val ) {
    
    static char pend[] = "PENDING"; 
    static char actv[] = "ACTIVE"; 
    static char sces[] = "SUCCESS"; 
    static char fail[] = "FAILED"; 
    static char nini[] = "NOT INITIALIZED"; 
    
    if ( val == FTI_SI_PEND ) {
        return pend;
    }
    if ( val == FTI_SI_ACTV ) {
        return actv;
    }
    if ( val == FTI_SI_SCES ) {
        return sces;
    }
    if ( val == FTI_SI_FAIL ) {
        return fail;
    }
    if ( val == FTI_SI_NINI ) {
        return nini;
    }
}
        


#define PRECISION   0.005
#define ITER_TIMES  5000
#define ITER_OUT    500
#define WORKTAG     50
#define REDUCE      5


void initData(int nbLines, int M, int rank, double *h)
{
    int i, j;
    for (i = 0; i < nbLines; i++) {
        for (j = 0; j < M; j++) {
            h[(i*M)+j] = 0;
        }
    }
    if (rank == 0) {
        for (j = (M*0.1); j < (M*0.9); j++) {
            h[j] = 100;
        }
    }
}


double doWork(int numprocs, int rank, int M, int nbLines, double *g, double *h)
{
    int i,j;
    MPI_Request req1[2], req2[2];
    MPI_Status status1[2], status2[2];
    double localerror;
    localerror = 0;
    for(i = 0; i < nbLines; i++) {
        for(j = 0; j < M; j++) {
            h[(i*M)+j] = g[(i*M)+j];
        }
    }
    if (rank > 0) {
        MPI_Isend(g+M, M, MPI_DOUBLE, rank-1, WORKTAG, FTI_COMM_WORLD, &req1[0]);
        MPI_Irecv(h,   M, MPI_DOUBLE, rank-1, WORKTAG, FTI_COMM_WORLD, &req1[1]);
    }
    if (rank < numprocs-1) {
        MPI_Isend(g+((nbLines-2)*M), M, MPI_DOUBLE, rank+1, WORKTAG, FTI_COMM_WORLD, &req2[0]);
        MPI_Irecv(h+((nbLines-1)*M), M, MPI_DOUBLE, rank+1, WORKTAG, FTI_COMM_WORLD, &req2[1]);
    }
    if (rank > 0) {
        MPI_Waitall(2,req1,status1);
    }
    if (rank < numprocs-1) {
        MPI_Waitall(2,req2,status2);
    }
    for(i = 1; i < (nbLines-1); i++) {
        for(j = 0; j < M; j++) {
            g[(i*M)+j] = 0.25*(h[((i-1)*M)+j]+h[((i+1)*M)+j]+h[(i*M)+j-1]+h[(i*M)+j+1]);
            if(localerror < fabs(g[(i*M)+j] - h[(i*M)+j])) {
                localerror = fabs(g[(i*M)+j] - h[(i*M)+j]);
            }
        }
    }
    if (rank == (numprocs-1)) {
        for(j = 0; j < M; j++) {
            g[((nbLines-1)*M)+j] = g[((nbLines-2)*M)+j];
        }
    }
    return localerror;
}


int main(int argc, char *argv[])
{
    int rank, nbProcs, nbLines, i, M, arg;
    double wtime, *h, *g, memSize, localerror, globalerror = 1;

    MPI_Init(&argc, &argv);
    FTI_Init(argv[2], MPI_COMM_WORLD);

    MPI_Comm_size(FTI_COMM_WORLD, &nbProcs);
    MPI_Comm_rank(FTI_COMM_WORLD, &rank);

    arg = atoi(argv[1]);
    M = (int)sqrt((double)(arg * 1024.0 * 512.0 * nbProcs)/sizeof(double));
    nbLines = (M / nbProcs)+3;
    h = (double *) malloc(sizeof(double *) * M * nbLines);
    g = (double *) malloc(sizeof(double *) * M * nbLines);
    initData(nbLines, M, rank, g);
    memSize = M * nbLines * 2 * sizeof(double) / (1024 * 1024);

    int fti_req[1024L*512L];
    int reqcnt = 0;

    char s_dir[FTI_BUFS];
    int serr = FTI_GetStageDir( s_dir, FTI_BUFS );
    
    int jj;
    const int NUM_FILES = 1000;
    char **fn_local = (char**) malloc( sizeof(char*) * NUM_FILES );
    char **fn_global = (char**) malloc( sizeof(char*) * NUM_FILES );
    if ( serr == FTI_SCES ) {
        if ( rank%1==0 ) {
            for( jj=0; jj<NUM_FILES; ++jj ) {
                fn_local[jj] = (char*)malloc( FTI_BUFS );
                fn_global[jj] = (char*)malloc( FTI_BUFS );
                snprintf( fn_local[jj], FTI_BUFS, "%s/file-0-%d-%d", s_dir, jj, rank );
                snprintf( fn_global[jj], FTI_BUFS, "./file-0-%d-%d", jj, rank );
                fn_local[jj][FTI_BUFS-1]='\0';
                fn_global[jj][FTI_BUFS-1]='\0';
                FILE *fd = fopen( fn_local[jj], "w+" );
                fsync(fileno(fd));
                fclose( fd );
                truncate( fn_local[jj], 1024L );
//                usleep(200000);
                //char tmp = *(fn_local[jj]+strlen(s_dir)); 
                //if( (rank == 0) && (jj == 0) ) {
                //    *(fn_local[jj]+strlen(s_dir)) = 'G';
                //}
                //fti_req[reqcnt++] = FTI_SendFile( fn_local[jj], fn_global[jj] );
                //*(fn_local[jj]+strlen(s_dir)) = tmp;
            }
            for( jj=0; jj<NUM_FILES; ++jj ) {
                fti_req[reqcnt++] = FTI_SendFile( fn_local[jj], fn_global[jj] );
                //usleep(30000);
                //int kk;
                //printf("last ID: %d\n", fti_req[reqcnt-1]);
                //for( kk=0; kk<reqcnt; ++kk ){
                //    FTI_GetStageStatus( fti_req[kk] );
                //}
            }
        }
    }
    if (rank == 0) {
        printf("Local data size is %d x %d = %f MB (%d).\n", M, nbLines, memSize, arg);
        printf("Target precision : %f \n", PRECISION);
        printf("Maximum number of iterations : %d \n", ITER_TIMES);
        sleep(1);
    }
    
    FTI_Protect(0, &i, 1, FTI_INTG);
    FTI_Protect(1, h, M*nbLines, FTI_DBLE);
    FTI_Protect(2, g, M*nbLines, FTI_DBLE);

    bool printed = false;
    wtime = MPI_Wtime();
    for (i = 0; i < ITER_TIMES; i++) {
        if ( (rank%1==0) && (i%1==0) && !(i==0) && serr==FTI_SCES ) {
            bool all_finished = true;
            for ( jj=0; jj<reqcnt; ++jj ) {
                int res = FTI_GetStageStatus( fti_req[jj] );
                if ( res != FTI_SI_NINI ) {
                    //printf( "| [rank:%02d|iter:%d] STAGING STATUS -> %s\t\t|\n", rank, i, status_string(res) );
                }
                if ( (res == FTI_SI_ACTV) || (res == FTI_SI_PEND) ) {
                    all_finished = false;
                }
            }
            if ( all_finished && !printed ) {
                printf("| [rank:%02d|iter:%d] ALL STAGING REQUEST COMPLETED\t|\n", rank, i);
                printed = true;
            }
            if ( i%1000 == 0 ) {
                for( jj=0; jj<NUM_FILES; ++jj ) {
                    snprintf( fn_local[jj], FTI_BUFS, "%s/file-%d-%d-%d", s_dir, i, jj, rank );
                    snprintf( fn_global[jj], FTI_BUFS, "./file-%d-%d-%d", i, jj, rank );
                    fn_local[jj][FTI_BUFS-1]='\0';
                    fn_global[jj][FTI_BUFS-1]='\0';
                    FILE *fd = fopen( fn_local[jj], "w+" );
                    fclose( fd );
                    truncate( fn_local[jj], 1024L );
                }
                for ( jj=0; jj<NUM_FILES; ++jj ) {
                    fti_req[reqcnt++] = FTI_SendFile( fn_local[jj], fn_global[jj]);
                    //usleep(30000);
                    //int kk;
                    //for( kk=0; kk<reqcnt; ++kk ){
                    //    FTI_GetStageStatus( fti_req[kk] );
                    //}
                }
            }

        }
        int checkpointed = FTI_Snapshot();
        localerror = doWork(nbProcs, rank, M, nbLines, g, h);
        if (((i%ITER_OUT) == 0) && (rank == 0)) {
            printf("Step : %d, error = %f\n", i, globalerror);
        }
        if ((i%REDUCE) == 0) {
            MPI_Allreduce(&localerror, &globalerror, 1, MPI_DOUBLE, MPI_MAX, FTI_COMM_WORLD);
        }
        if(globalerror < PRECISION) {
            break;
        }
    }
    if (rank == 0) {
        printf("Execution finished in %lf seconds.\n", MPI_Wtime() - wtime);
    }

    free(h);
    free(g);

    FTI_Finalize();
    MPI_Finalize();
    return 0;
}
