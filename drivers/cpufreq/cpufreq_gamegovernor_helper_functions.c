/*
 *  linux/drivers/cpufreq/cpufreq_gamegovernor_helper_functions.c
 *
 *  Copyright (C) 2015 - 2016 Dominik Füß <dominik.fuess@tum.de>
 *  Copyright (C) 2019 Philipp van Kempen <philipp.van-kempen@tum.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

// How many past workloads should be evaluated
#define WMA_length 14

#include <linux/sched.h>
#include <gamegovernor/cpufreq_gamegovernor_includes.h>

#ifdef UNIT_TEST
	#include <inttypes.h>
    // TODO: fix
	#include "/home/dominik/odroid_android2/kernel/samsung/exynos5422/include/dominiksgov/cpufreq_dominiksgov_includes.h"
	int64_t div64_s64(int64_t a, int64_t b){
		int64_t c;
		c=a/b;
		return c;
	}
#else
	#include <gamegovernor/cpufreq_gamegovernor_includes.h>
	#include <linux/math64.h>
#endif

	//function to compute the autocorrelation of the input data_in with the size length for tau lags
	//the output is written to outcorr_out which has to have have the size tau+1
/*
 * Compute Autocorrelation
 *
 * PARAMETERS:
 *  length: size of input data
 *  tau: number of lags
 *  data_in: workloads (pointer)
 *  outcorr_out: returned data (pointer)
 */
void autocorr( const int64_t *data_in, int64_t *outcorr_out, int tau, int length )
{
	int i;
	int64_t mean_data;
	int64_t buf;
	int64_t data_buf[length];

	mean_data=mean(data_in, length);

	for(i=0; i<length; i++){
		data_buf[i]=data_in[i]-mean_data;
	}

	for (i=0; i<=tau; i++){
		outcorr_out[i]=corr_own(data_buf, i, length);
	}

	buf=outcorr_out[0];

	if (buf!=0){
		for(i=0; i<=tau; i++){
			outcorr_out[i]=div64_s64(outcorr_out[i]*100, buf);
		}
	}
	else {
		for(i=0; i<=tau; i++){
					outcorr_out[i]=0;
				}
	}
}

//computes the autocorrelation for one given lag a
int64_t corr_own(const int64_t *data_in, int a, int length){
	int64_t out=0;
	int b;

	for (b=0; b<length-a; b++){
	   out=out+data_in[b]*data_in[b+a];
	}
	return out;
}

//computes the mean of the input data_in which has the size length
int64_t mean(const int64_t *data_in, int length){
	int i;
	int64_t sum=0;
	for(i=0; i<length; i++){
		sum=sum+data_in[i];
	}
	sum=div64_s64(sum, length);
	return sum;
}

int64_t WMA( const int64_t *cpu_time_history, int length ){
	int64_t Weight[WMA_length]={14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
	int64_t prediction=0;
	static int64_t BUF=0;
	int a;
	
	if (BUF==0){
		for (a=1; a<=length; a++){//hardcode??
			BUF=BUF+a;
		}
	}
	
	for (a=0; a<length; a++){
		prediction=prediction + (cpu_time_history[a]*Weight[a]);
	}
	
	prediction=div64_s64(prediction, BUF);
	return prediction;
}

void get_max(const int64_t *a, int length, int64_t *max, int *pos){
	int i;
	*max=a[0];
	*pos=0;
	
	for(i=1; i<length; i++){
		if(*max<a[i]){
			*max=a[i];
			*pos=i;
		}
	}	
}

int64_t WMA_hybrid_predictor(const int64_t *data, int length, int64_t autocorr_max, int pos){
	int64_t prediction;
	if (autocorr_max>34){
		prediction= data[pos];		
	}
	else{
		prediction=WMA(data, 14);
	}
	return prediction;
}
