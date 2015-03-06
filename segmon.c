#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "sdrclib/easyparse.h"
#include "sdrclib/daemonize.h"

#define CFGFILE "segmon.cfg"

const char * cfgfile=CFGFILE;

#define MAXPARAMS 20
#define SEGLSZ 20

typedef struct {
	int inuse;
	pid_t child;
	int argc ;
	const char * strm;
	const char * argv[MAXPARAMS+3];
	char segl[SEGLSZ];
} segmenter_t;

#define MAXSEGMETERS 200
segmenter_t segmenters [MAXSEGMETERS+1];
segmenter_t *csegm;

#define DEFSEGMENTER "./segmenter"
const char *segmenter;

int easyparse_cb (void*userdata,  char *name, int namel, char *value, int valuel){
	printf ("K=V: [%.*s]=[%.*s]\n",namel,name,valuel,value);
	if (value && valuel)value[valuel]=0;
	if (namel==9 && memcmp(name,"segmenter",9)==0) {
		segmenter=value;
	} else if (namel==6 && memcmp(name,"stream",6)==0) {
		if (!csegm) {
			csegm=segmenters;
		} else if(csegm-segmenters<MAXSEGMETERS) {
			csegm++;
		} else {
			printf ("too many segmenters! (hard limit is %d)\n",MAXSEGMETERS);
			return 1;
		}
		csegm->inuse=1;
		csegm->strm=value;
		csegm->argc=4;
		csegm->argv[0]=segmenter;
		csegm->argv[1]="-o";
		csegm->argv[2]=value;
		csegm->argv[3]="-p";
	} else if (namel==7 && memcmp(name,"plsfile",7)==0) {//fix -o value:
		csegm->argv[2]=value;
	} else if (csegm && namel==5 && memcmp(name,"input",5)==0) {
		if (csegm->argc<MAXPARAMS ){
			csegm->argv[csegm->argc++]="-i";
			csegm->argv[csegm->argc++]=value;
		} else {
			printf ("too many segmenter params! (hard limit is %d)\n",MAXPARAMS);
			return 1;
		}
	} else if (csegm && namel==7 && memcmp(name,"distdir",7)==0){
		if (csegm->argc<MAXPARAMS ){
			csegm->argv[csegm->argc++]="-d";
			csegm->argv[csegm->argc++]=value;
		} else {
			printf ("too many segmenter params! (hard limit is %d)\n",MAXPARAMS);
			return 1;
		}
	} else if (csegm && namel==7 && memcmp(name,"segname",7)==0){
		if (csegm->argc<MAXPARAMS ){
			csegm->argv[csegm->argc++]="-f";
			csegm->argv[csegm->argc++]=value;
		} else {
			printf ("too many segmenter params! (hard limit is %d)\n",MAXPARAMS);
			return 1;
		}
	} else if (csegm && namel==5 && memcmp(name,"listl",5)==0){
		if (csegm->argc<MAXPARAMS ){
			csegm->argv[csegm->argc++]="-m";
			csegm->argv[csegm->argc++]=value;
			if (valuel==1 && *value=='a') csegm->argv[1]="-f";
		} else {
			printf ("too many segmenter params! (hard limit is %d)\n",MAXPARAMS);
			return 1;
		}
	} else if (csegm && namel==10 && memcmp(name,"segmentlen",10)==0 && valuel>0){
		if (csegm->argc<MAXPARAMS ){
			int mult=1;
			if (value[valuel-1]=='m'){
				mult=60;value[--valuel]=0;
			} else if (value[valuel-1]=='h') {
				mult=60*60;value[--valuel]=0;
			}
			csegm->argv[csegm->argc++]="-l";
			if (mult==1){
				csegm->argv[csegm->argc++]=value;
			} else {
				snprintf(csegm->segl,SEGLSZ-1,"%d",atoi(value)*mult);
				csegm->argv[csegm->argc++]=csegm->segl;
			}
		} else {
			printf ("too many segmenter params! (hard limit is %d)\n",MAXPARAMS);
			return 1;
		}
	}
	
	return 0;
}

void cleanchilds(pid_t pid){
	
	int x;
	for(x=0;x<MAXSEGMETERS;x++){
		if (segmenters[x].inuse==0) break;
		if (segmenters[x].child==pid) segmenters[x].child=0;
	}
}

int volatile signaled=0;
static void handle_sigterm(int signum, siginfo_t *sinfo, void *unused){
	signaled=1;
}

static void handle_sigchld(int signum, siginfo_t *sinfo, void *unused){
    int sav_errno = errno;
	if (sinfo->si_code == CLD_EXITED) {
		int status;
		if  ( waitpid(sinfo->si_pid, &status, WNOHANG) != -1){
			if (WIFEXITED(status)||WIFSIGNALED(status))cleanchilds(sinfo->si_pid);
		}
	}
    errno = sav_errno;
}
#define usage(pname) { fprintf (stderr,"Usage: %s  [-c <config file>] [-p <pidfile>]\n   Default config is "CFGFILE" passing -p trigers deamon mode",pname); exit (1); }
int main (int argc,char * argv[] ){
	segmenter=DEFSEGMENTER;
	memset(segmenters,0,sizeof(segmenters));
	csegm=NULL;
	
	
	int c;
	char * pidfile=NULL;
	while ((c = getopt (argc, argv, "c:p:")) != -1) switch (c) {
		case 'c':
			cfgfile = optarg;
			break;
		case 'p':
			pidfile = optarg;
			break;
		case '?':
			usage (argv[0]);
			break;
		default:
			usage (argv[0]);
	}
	
	printf("using scfg file %s\n",cfgfile);
	struct stat st;
	char *cfg;
	if (stat(cfgfile, &st)!=0){
		printf("can't stat scfg file %s\n",cfgfile);
		exit (-1);
	} else {
		cfg=malloc(st.st_size+2);
		if (!cfg) {
			printf("failed to allocate memory for cfg?!\n");
			exit(-1);
		}
		cfg[st.st_size]=0;
	}
	FILE *cfgf=fopen(cfgfile,"r");
	if (!cfgf) {
		printf("failed to open %s for reading!\n",cfgfile);
		exit(-1);
	}
	size_t ret=fread(cfg,1,st.st_size,cfgf);
	if (ret!=st.st_size){
		printf("failed to read %ld bytes from %s!\n",st.st_size,cfgfile);
		exit(-1);
	}
	fclose(cfgf); 

	easyparse(cfg,st.st_size,easyparse_cb,NULL);
	
	if (pidfile) daemonize("/dev/null","/dev/null","/dev/null",pidfile);
		
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGABRT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    
	if (csegm==NULL) {
		printf ("no stream=.... in config! Nothing to monitor!\n");
		exit (1);
	}
	int x,y;
	for(x=0;x<MAXSEGMETERS;x++){
		if (segmenters[x].inuse==0) break;

		printf ("MONITOR: starting:");
		for (y=0; y<segmenters[x].argc;y++) printf("%s ",segmenters[x].argv[y]);
		printf ("\n");

		segmenters[x].child=fork();
		if (segmenters[x].child==0){
			sleep(1);
			execvp(segmenters[x].argv[0],(char * const *)segmenters[x].argv);
		} else {
			printf("child for strm %s started pid %d!\n",segmenters[x].strm,segmenters[x].child);
		}

	}
	printf("waiting...\n");
	while (!signaled) {
		int status;
		pid_t pid=waitpid((pid_t)0,&status,0);
		if (pid>0 && (WIFEXITED(status)||WIFSIGNALED(status))) cleanchilds(pid);
		for(x=0;x<MAXSEGMETERS;x++){
			if (segmenters[x].inuse==0) break;
			if (segmenters[x].child==0) {
				segmenters[x].child=fork();
				if (segmenters[x].child==0){
					sleep(1);
					execvp(segmenters[x].argv[0],(char * const *)segmenters[x].argv);
				} else {
					printf("child for strm %s REstarted pid %d!\n",segmenters[x].strm,segmenters[x].child);
				}
			}
		}
	}
	printf("waiting over!\n");
	
	return 0;
}
