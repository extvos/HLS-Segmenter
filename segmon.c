#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include "sdrclib/easyparse.h"

#define CFGFILE "segmon.cfg"
const char * cfgfile=CFGFILE;

int main (int argc,char * argv[] ){
	if (argc>1){
		cfgfile=argv[1];
	}
	
	printf("using scfg file %s\n",cfgfile);
	
	struct stat st;
	char *cfg;
	if (stat(cfgfile, &st)!=0){
		printf("can't stat scfg file %s\n",cfgfile);
		exit (-1);
	} else {
		char *cfg=malloc(st.st_size+1);

	}
	

	return 0;
}