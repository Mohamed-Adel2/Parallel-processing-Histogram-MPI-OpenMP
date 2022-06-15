#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "mpi.h"
#define MIN -214748364

int getMax(int* arr,int n){
    int max=arr[0],i=0;
    for(i=1;i<n;i++){
       if(arr[i]>max){
        max=arr[i];
       }
    }
    return max;
}

int main( int argc, char **argv )
{
    int my_rank;
	int p;
	int tag=0;
	char line[100];
	MPI_Status status;
    FILE* file;
    int number_of_data,number_of_bars,Data_per_process;
    int tmp,i,j;
    int *Data=NULL;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &p);

    if(my_rank == 0){
        printf("Enter the Number of Bars: ");
        scanf("%d",&number_of_bars);
        printf("Enter the Number of points: ");
        scanf("%d",&number_of_data);
        //number of Data that will be send to processes
        Data_per_process=(number_of_data)/p;
        if(number_of_data%p!=0)Data_per_process++;
        //create the Array that will hold the data and Read the data from the file
        Data =(int*) malloc((Data_per_process*p) * sizeof(int));
        file = fopen("/shared/dataset2.txt", "r");
        #pragma omp parallel shared(Data,number_of_data) private(i)
        {
            #pragma omp for schedule(dynamic)
            for(i=0;i<number_of_data;i++){
                fgets (line, 100, file);
                tmp=atoi(line);
                Data[i]=tmp;
            }
        }
        fclose(file);
        //if Data is not divisible by processes fill some index with specified value till the number become divisible by processes
        for(i=number_of_data;i<Data_per_process*p;i++)Data[i]=MIN;
    }
    //BroadCast some values and declare some Data that will be used
    MPI_Bcast(&number_of_data,1,MPI_INT,0,MPI_COMM_WORLD);
    MPI_Bcast(&number_of_bars,1,MPI_INT,0,MPI_COMM_WORLD);
    MPI_Bcast(&Data_per_process,1,MPI_INT,0,MPI_COMM_WORLD);
    int recievedData[Data_per_process],BarCount[number_of_bars],Range[number_of_bars+1];
    #pragma omp parallel shared(Data,number_of_data) private(i)
    {
            #pragma omp for schedule(dynamic)
            for(i=0;i<number_of_bars;i++) BarCount[i]=0;
    }
    //setting the ranges for bars
    if(my_rank==0){
        Range[0]=0;
        tmp=getMax(Data,number_of_data);
        #pragma omp parallel shared(Range,number_of_bars,tmp) private(i)
        {
            #pragma omp for schedule(dynamic)
            for(i=0;i<number_of_bars;i++){
            Range[i+1]=(tmp/number_of_bars)*(i+1);
            }
        }
    }
    MPI_Bcast(&Range,(number_of_bars+1),MPI_INT,0,MPI_COMM_WORLD);
    //send data to each process
    MPI_Scatter(Data,Data_per_process, MPI_INT, &recievedData,Data_per_process,MPI_INT,0,MPI_COMM_WORLD);
    //add each point to its right bar
    #pragma omp parallel shared(Range,Data_per_process,number_of_bars,recievedData,BarCount) private(i,j)
    {
            #pragma omp for schedule(dynamic)
            for(i=0;i<Data_per_process;i++){
                for(j=1;j<=number_of_bars;j++){
                    if(recievedData[i]==MIN) break;
                    if(recievedData[i]<=Range[j]){
                            #pragma omp critical(BarCount)
                            BarCount[j-1]++;
                            break;
                    }
                }
            }
    }
    int gatheredData[number_of_bars*p];
    //collect result from each process
    MPI_Gather(&BarCount,number_of_bars,MPI_INT,&gatheredData,number_of_bars,MPI_INT,0,MPI_COMM_WORLD);
    //add the received data and calculate the final bars count then print them
    if(my_rank==0){
        #pragma omp parallel shared(BarCount,number_of_bars) private(i)
        {
            #pragma omp for schedule(dynamic)
            for(i =0;i<number_of_bars;i++)BarCount[i]=0;
        }
        #pragma omp parallel shared(gatheredData,number_of_bars,BarCount) private(i)
        {
            #pragma omp for schedule(dynamic)
            for(i=0;i<number_of_bars*p;i++)
                #pragma omp critical(BarCount)
                BarCount[i%number_of_bars]+=gatheredData[i];
        }
        #pragma omp parallel shared(Range,number_of_bars,BarCount) private(i)
        {
            #pragma omp for schedule(dynamic)
            for(i=0;i<number_of_bars;i++){
            printf("The range start with %d, end with %d with count %d\n",Range[i],Range[i+1],BarCount[i]);
            }
        }
    }
	/* shutdown MPI */
    MPI_Finalize();

    return 0;
}
