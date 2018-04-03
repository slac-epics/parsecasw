// Implementation for utils

// The following must be 1024 or less for WIN32
#define FIXED_MSG_SIZE 1024

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int errMsg(const char *fmt, ...)
{
    va_list vargs;
    static char lstring[FIXED_MSG_SIZE];

    va_start(vargs,fmt);
    (void)vsprintf(lstring,fmt,vargs);
    va_end(vargs);
    
    if(lstring[0] != '\0') {
	fprintf(stdout,lstring);
    }

    return 0;
}

// Heap sort routine: Sorts an array of length n into ascending order
// and puts the sorted indices in indx.
void hsort(double array[], int indx[], int n)
{
    int l,j,ir,indxt,i;
    double q;

  /* All done if none or one element */
    if(n == 0) return;
    if(n == 1) {
	indx[0]=0;
	return;
    }
    
  /* Initialize indx array */
    for(j=0; j < n; j++) indx[j]=j;
  /* Loop over elements */
    l=(n>>1);
    ir=n-1;
    for(;;) {
	if(l > 0) q=array[(indxt=indx[--l])];
	else {
	    q=array[(indxt=indx[ir])];
	    indx[ir]=indx[0];
	    if(--ir == 0) {
		indx[0]=indxt;
		return;
	    }
	}
	i=l;
	j=(l<<1)+1;
	while(j <= ir) {
	    if(j < ir && array[indx[j]] < array[indx[j+1]]) j++;
	    if(q < array[indx[j]]) {
		indx[i]=indx[j];
		j+=((i=j)+1);
		
	    }
	    else break;
	}
	indx[i]=indxt;
    }
}
