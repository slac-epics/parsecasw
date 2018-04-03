// Main implementation for parsecasw

// Note that IOC is used in this code to denote a server in general
// even though not all servers are IOCs

#define DEBUG_PARSE 0
#define DEBUG_REALTIME 0

// Use to limit output while testing
#define DEBUG_LIMIT 0
#define LINE_LIMIT 10

// Time interval in sec for a new group to be declared
#define NEW_GROUP_TIME 60.0

// Timer interval in sec
#define TIMER_INTERVAL 60u

// See characterize() for the logic used to separate the groups into
// categories using the following parameters

// Number of points in a group to be declared short
#define SHORT_POINTS 5
// Number of points in a group to be declared mwdium long
#define MEDIUM_POINTS 15
// Number of points in a group to be declared long
#define LONG_POINTS 50
// Time in sec that max and min must agree to be declared
// regular
#define REGULAR_TOLERANCE .25
// Ratio of Min to Max for server coming up
#define MAX_MIN_RATIO 25.0
// Maximum non-increasing intervals for probable IOC coming up
#define MAX_NONINCREASING_INTERVALS 2

#include "parsecasw.h"
#include "utils.h"
#include "CIoc.h"

// Include array with extra help lines
#include "help.txt"

typedef enum _SortMode
{
    SORT_GROUP,
    SORT_IOC,
    SORT_FINISHED
} SortMode;

// Make chnString consistent with this
typedef enum _Characterization
{
    CHN_SINGLE=0,
    CHN_SERVER,
    CHN_PROBABLESERVER,
    CHN_REGULAR,
    CHN_SHORT,
    CHN_MEDIUM,
    CHN_LONG,
    CHN_VERYLONG,
    CHN_ORDER
} Characterization;

const char *chnString[CHN_ORDER+1]={
    "Single anomaly",
    "Server coming up",
    "Probably server coming up",
    "Regular beacons, network coming back",
    "Short sequence",
    "Medium long sequence",
    "Long sequence",
    "Very long sequence",
    "Anomalies out of order"
};

typedef enum _CaswFileType
{
    FT_CASW,
    FT_OAG
} CaswFileType;

// Function prototypes
int main(int argc, char **argv);
static int parseCommand(int argc, char **argv);
static void usage(void);
static void report(SortMode sortMode);
static void sortByIoc(void);
static void reportByIoc();
static void printIoc(CIoc *pIoc);
static void sortByGroup(SortMode sortMode);
static void reportByGroup();
static void printGroup(CGroup *pGroup);
static Characterization characterize(CGroup *pGroup);
void removeFinished(void);

// Global variables

//ioclic1:5064                             2004-05-18 12:17:02.418826640
const char caswFormat[]="%s %d-%d-%d %d:%d:%lf";
//                       iocs3vp:5064  2004/05/12 00:08:08.0134  2004/05/12 00:08:08.0000 
const char oagFormat[]="%s %d/%d/%d %d:%d:%lf";

epicsMutexId lock=NULL;
resTable<CIoc,stringId> iocTable;
CIoc **iocs=NULL;
CGroup **groups=NULL;
double *timeDiffs=NULL;
int *indices=NULL;
int nArray;
SortMode defaultSortMode=SORT_GROUP;
int verbose=0;
int terse=0;
int realTime=0;
int echo=0;
int doUsage=0;
CaswFileType fileType=FT_CASW;
int caswFileSpecified=0;
char caswFileName[PATH_MAX];
int linesSkipped=0;
unsigned timerInterval=TIMER_INTERVAL;

// CParseTimer implementation

epicsTimerNotify:: expireStatus
CParseTimer::expire(const epicsTime &curTime)
{
    static int first=1;

    if(first) {
	first=0;
	return epicsTimerNotify::expireStatus(restart,interval);
    }

    epicsMutexLock(lock);
#if DEBUG_REALTIME && 0
    printf("Starting report\n");
#endif
    report(SORT_FINISHED);
#if DEBUG_REALTIME
    if(nArray) {
	printf("Ending report: %d items\n",nArray);
	fflush(stdout);
    }
#endif
    epicsMutexUnlock(lock);

  // Set to continue
    return epicsTimerNotify::expireStatus(restart,interval);
}


int main(int argc, char **argv)
{
    epicsTimerQueueActive *timerQueue=NULL;
    CParseTimer *parseTimer=NULL;
    FILE *caswFp=NULL;
    int retVal=0;
    char *bytes;
    int lineNum=0;
    char line[READ_LINESIZE];
    char name[READ_LINESIZE];
    int year=0,month=0,day=0,hour=0,min=0,sec=0;
    double dsec=0.0,fsec=0.0;
    local_tm_nano_sec tmnanotime;
    epicsTime time;
    int items=0;
    CIoc *pIoc;

  // Parse the command line
    int status=parseCommand(argc,argv);
    if(doUsage) {
	usage();
	if(status == P_OK) exit(0);
    }
    if(status != P_OK) exit(1);

    if(!caswFileSpecified) realTime=1;

  // Setup real time
    if(realTime) {
      // Do overrides
	defaultSortMode=SORT_GROUP;
	caswFileSpecified=0;

      // Make a mutex
	lock=epicsMutexCreate();
	if(!lock) {
	    errMsg("Cannot create mutex");
	    goto ERROR;
	}

      // Start a default timer queue (true to use shared queue, false to
      // have a private one)
	timerQueue=&epicsTimerQueueActive::allocate(true);

      // Start a timer
	parseTimer = new CParseTimer(*timerQueue,timerInterval);
	if(parseTimer) {
	  // Call the expire routine to initialize it
	    parseTimer->expire(epicsTime::getCurrent());
	  // Then start the timer
	    parseTimer->start();
	} else {
	    errMsg("Could not start timer\n");
	    goto ERROR;
	}
    }

  // Open the file
    if(caswFileSpecified) {
	caswFp=fopen(caswFileName,"r");
	if(!caswFp) {
	    errMsg("Cannot read file:\n%s",caswFileName);
	    goto ERROR;
	}
    } else {
	caswFp=stdin;
    }

  // Read the lines
    while(1) {
	bytes=fgets(line,READ_LINESIZE,caswFp);
	lineNum++;
	if(!bytes) break;
	if(ferror(caswFp)) {
	    errMsg("Error reading line %d of %s",lineNum,caswFileName);
	    goto ERROR;
	}
	if(fileType == FT_CASW) {
	    items=sscanf(line,caswFormat, name,
	      &year,&month,&day,&hour,&min,&dsec);
	} else {
	    items=sscanf(line,oagFormat, name,
	      &year,&month,&day,&hour,&min,&dsec);
	}

      // Only use lines that have all expected items
	if(items != 7) {
	    linesSkipped++;
	    continue;
	}

      // Put the information in a local_tm_nano_sec, which contains a
      // struct tm
	sec=(int)dsec;
	fsec=dsec-(double)sec;
	memset(&tmnanotime,0,sizeof(tmnanotime));
	tmnanotime.ansi_tm.tm_sec=sec;
	tmnanotime.ansi_tm.tm_min=min;
	tmnanotime.ansi_tm.tm_hour=hour;
	tmnanotime.ansi_tm.tm_mday=day;
	tmnanotime.ansi_tm.tm_mon=month-1;
	tmnanotime.ansi_tm.tm_year=year-1900;
      // Say we don't know about DST
	tmnanotime.ansi_tm.tm_isdst=-1;
      // Define the nanosec part
	tmnanotime.nSec=(int)(1000000000.0*fsec+.5);
      // Convert it to a epicsTime
	time=tmnanotime;

#if DEBUG_PARSE
	printf(line);
	printf("name=%s\n"
	  "year=%d month=%d day=%d hour=%d min=%d dsec=%.5f sec=%d fsec=%.5f\n",
	  name,
	  year,month,day,hour,min,dsec,sec,fsec);
	static char timeStampStr[512];
	time.strftime(timeStampStr,20,"%b %d %H:%M:%S");
	
	printf("%s\n",timeStampStr);
#endif

      // Echo the input lines
	if(echo) printf("%s",line);

      // Lock
	if(realTime) epicsMutexLock(lock);

      // See if we have it
	stringId *id=new stringId(name);
	if(!id) {
	    errMsg("Failed to create ID for line %d: %s",
	      lineNum,name);
	    exit(1);
	}
	pIoc=iocTable.lookup(*id);
	delete id;
	id=NULL;
	if(pIoc) {
	  // We have it already
#if DEBUG_PARSE
	    printf("IOC Found: %s\n",pIoc->resourceName());
#endif
	    pIoc->update(time,NEW_GROUP_TIME);
	} else {
	  // Create a new one
#if DEBUG_PARSE
	    printf("New IOC\n");
#endif
#if DEBUG_REALTIME
	    printf(" Creating ioc: %s\n",name);
#endif
	    pIoc=new CIoc(name,time);
	    if(!pIoc) {
		errMsg("Failed to create IOC entry for line %d: %s",
		  lineNum,name);
		exit(1);
	    }
	    iocTable.add(*pIoc);
	}

      // Unlock
	if(realTime) epicsMutexUnlock(lock);

#if DEBUG_LIMIT
	if(lineNum >= LINE_LIMIT) break;
#endif
    }

  // Print report
    report(defaultSortMode);

  // Print how many lines were skipped.  This needs to be done because
  // if the format is wrong, for example, there is no user
  // notification otherwise.
    if(linesSkipped > 0) printf("\n\nLines skipped: %d\n",linesSkipped);

    goto FINISH;

  ERROR:
    retVal=1;
    
  FINISH:
  // Close the file
    fclose(caswFp);

  // Free any existing arrays
    if(iocs) {
	delete [] iocs;
	iocs=NULL;
    }
    if(groups) {
	delete [] groups;
	groups=NULL;
    }
    if(timeDiffs) {
	delete [] timeDiffs;
	timeDiffs=NULL;
    }
    if(indices) {
	delete [] indices;
	indices=NULL;
    }
    nArray=0;

  // Empty the ioc list
    resTableIter<CIoc,stringId> iter1(iocTable.firstIter());
    while((pIoc=iter1.pointer())) {
      // Increment first before deleting so we don't delete what the
      // iter is pointing at
        iter1++;
      // Delete the CIoc which should remove all the groups from its
      // list and delete them
	delete pIoc;
    }

    return retVal;
}

static int parseCommand(int argc, char **argv)
{
    int intVal;

    for(int i=1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    switch(argv[i][1]) {
	    case 'h':
		doUsage=1;
		break;
	    case 'e':
		echo=1;
		break;
	    case 'i':
		i++;
		if(i >= argc) {
		    errMsg("\nNo value specified for interval");
		    doUsage=1;
		    return P_ERROR;
		}
		intVal=(unsigned)atoi(argv[i]);
		if(intVal <= 0) {
		    errMsg("\nInvalid timerInterval: %s",argv[i]);
		    doUsage=1;
		    return P_ERROR;
		}
		timerInterval=(unsigned)intVal;
		break;
	    case 'o':
		fileType=FT_OAG;
		break;
#if 0
	    case 'r':
		realTime=1;
		break;
#endif
	    case 's':
		defaultSortMode=SORT_IOC;
		break;
	    case 'v':
		verbose=1;
		break;
	    case 't':
		terse=1;
		break;
	    default:
		errMsg("\nInvalid option: %s",argv[i]);
		usage();
		return P_ERROR;
	    }
	} else {
	    if(!caswFileSpecified) {
		strcpy(caswFileName,argv[i]);
		caswFileSpecified=1;
	    } else {
		errMsg("\nInvalid option: %s",argv[i]);
		doUsage=1;
		return P_ERROR;
	    }
	}
    }
#if 0
    if(!caswFileSpecified) {
	errMsg("Filename not specified");
	doUsage=1;
	return P_ERROR;
    }
#endif

    return P_OK;
}

static void usage(void)
{
    printf(
      "\nParseCASW\n\n"
      "Usage: parsecasw [Options] [filename]\n"
      "       casw | parsecasw [Options]\n"
      "  Parses CASW output and divides it into groups of beacon anomalies.\n"
      "  Reads from stdin if no filename is specified.\n"
      "\n"
      "  Options (First character is sufficient):\n"
      "    -help        This message.  Use with -v for more information.\n"
      "    -echo        Echo input lines\n"
      "    -int <int>   Do checking and output at this interval when reading\n"
      "                 from stdin. (Default is %u sec)\n"
      "    -oag         Use OAG data logger format (Default is CASW output)\n"
#if 0
      "    -real        Write blocks in real time (Use stdin, ignore -server)\n"
#endif
      "    -server      Sort by server (Default is by group)\n"
      "    -terse       Terse output (Default is between terse and verbose)\n"
      "    -verbose     Verbose output.  When used with -h produces more\n"
      "                 extensive help information.\n"
	,TIMER_INTERVAL);

    if(verbose) {
	int nLines=sizeof(helpTxt)/sizeof(char *);
	for(int i=0; i < nLines; i++) printf("%s\n",helpTxt[i]);
    }
}

static void report(SortMode sortMode)
{
    if(sortMode == SORT_FINISHED) {
	sortByGroup(sortMode);
	if(nArray > 0) {
	    reportByGroup();
	    removeFinished();
	}
    } else if(sortMode == SORT_GROUP) {
	sortByGroup(sortMode);
	if(nArray > 0) reportByGroup();
    } else {
	sortByIoc();
	if(nArray > 0) reportByIoc();
    }
}


static void sortByIoc(void)
{
  // Get a time to difference against (epicsTime only understands
  // differences)
    epicsTime curTime=epicsTime::getCurrent();

  // Free any existing arrays
    if(iocs) {
	delete [] iocs;
	iocs=NULL;
    }
    if(groups) {
	delete [] groups;
	groups=NULL;
    }
    if(timeDiffs) {
	delete [] timeDiffs;
	timeDiffs=NULL;
    }
    if(indices) {
	delete [] indices;
	indices=NULL;
    }
    nArray=0;

  // Get the number of entries
    CIoc *pIoc;
    int i=0;
    resTableIter<CIoc,stringId> iter1(iocTable.firstIter());
    while((pIoc=iter1.pointer())) {
	i++;
        iter1++;
    }
    nArray=i;

  // Allocate arrays
    if(!nArray) return;
    iocs=new CIoc *[nArray];
    if(!iocs) {
	errMsg("Cannot allocate space for IOC array");
	exit(1);
    }
    timeDiffs=new double[nArray];
    if(!timeDiffs) {
	errMsg("Cannot allocate space for timeDiffs array");
	exit(1);
    }
    indices=new int[nArray];
    if(!indices) {
	errMsg("Cannot allocate space for indices array");
	exit(1);
    }

  // Fill in arrays
  // Loop over the iocTable
    iter1=iocTable.firstIter();
    i=0;
    while((pIoc=iter1.pointer())) {
	epicsTime time=pIoc->getFirstTime();
	timeDiffs[i]=time-curTime;
	indices[i]=i;
	iocs[i]=pIoc;
	i++;
        iter1++;
    }

  // Sort
    hsort(timeDiffs,indices,nArray);

    return;
}

static void reportByIoc()
{
    CIoc *pIoc;
    int i,index;

    for(i=0; i < nArray; i++) {
	index=indices[i];
	pIoc=iocs[index];
	printIoc(pIoc);
    }
}

static void printIoc(CIoc *pIoc)
{
    char timeStampStr1[16];
    char timeStampStr2[16];
    
    printf("\n%s\n",pIoc->resourceName());
    if(verbose) {
	double delTime1=pIoc->getLastTime()-pIoc->getFirstTime();
	pIoc->getFirstTime().strftime(timeStampStr1,20,"%b %d %H:%M:%S");
	pIoc->getLastTime().strftime(timeStampStr2,20,"%b %d %H:%M:%S");
	printf(" %s to %s (%.2f sec = %.2f min = %.2f hours)\n",
	  timeStampStr1,timeStampStr2,delTime1,delTime1/60.,delTime1/3600.);
    }
    
    const tsDLList<CGroup> *pGroupList=pIoc->getGroupList();
    printf(" %u group(s) of beacon anomalies\n",pGroupList->count());
    
    tsDLIterBD<CGroup> iter2(pGroupList->first());
    tsDLIterBD<CGroup> eol;
    int group=1;
    while(iter2 != eol) {
	CGroup *pGroup=iter2;
	int nPoints=pGroup->getNPoints();
	printf(" Group %d: %d event(s)",group++,pGroup->getNPoints());
	int outOfOrder=pGroup->getOutOfOrder();
	if(outOfOrder) {
	    printf(" (%d event(s) out of order)\n",outOfOrder);
	} else {
	    printf("\n");
	}
	if(terse) {
	    Characterization chn=characterize(pGroup);
	    pGroup->getFirstTime().strftime(timeStampStr1,20,
	      "%b %d %H:%M:%S");
	    printf("  %s %s\n",timeStampStr1,chnString[chn]);
	} else if(!verbose) {
	    Characterization chn=characterize(pGroup);
	    pGroup->getFirstTime().strftime(timeStampStr1,20,
	      "%b %d %H:%M:%S");
	    printf("  %s %s\n",timeStampStr1,chnString[chn]);
	} else {
	    if(nPoints == 1) {
		Characterization chn=characterize(pGroup);
		printf("  %s\n",chnString[chn]);
		pGroup->getFirstTime().strftime(timeStampStr1,20,
		  "%b %d %H:%M:%S");
		printf("  %s\n",timeStampStr1);
	    } else if(nPoints > 1) {
		Characterization chn=characterize(pGroup);
		printf("  %s\n",chnString[chn]);
		double delTime2=pGroup->getLastTime()-pGroup->getFirstTime();
		pGroup->getFirstTime().strftime(timeStampStr1,20,
		  "%b %d %H:%M:%S");
		pGroup->getLastTime().strftime(timeStampStr2,20,
		  "%b %d %H:%M:%S");
		printf("  %s to %s (%.2f sec = %.2f min = %.2f hours)\n",
		  timeStampStr1,timeStampStr2,
		  delTime2,delTime2/60.,delTime2/3600.);
		
		printf("  Mean=%.2f Sigma=%.2f Min=%.2f Max=%.2f Increasing=%d",
		  pGroup->getMean(),pGroup->getSigma(),
		  pGroup->getMin(),pGroup->getMax(),pGroup->getIncreasing());
		if(pGroup->getIntervalType() == MonotonicIncreasing) {
		    printf(" Monotonically increasing\n");
		} else if(pGroup->getIntervalType() == MonotonicIncreasing) {
		    printf(" Monotonically decreasing\n");
		} else {
		    printf("\n");
		}
	    }
	}
	iter2++;
    }
}

static void sortByGroup(SortMode sortMode)
{
  // Get a time to difference against (epicsTime only understands
  // differences)
    epicsTime curTime=epicsTime::getCurrent();

  // Free any existing arrays
    if(iocs) {
	delete [] iocs;
	iocs=NULL;
    }
    if(groups) {
	delete [] groups;
	groups=NULL;
    }
    if(timeDiffs) {
	delete [] timeDiffs;
	timeDiffs=NULL;
    }
    if(indices) {
	delete [] indices;
	indices=NULL;
    }
    nArray=0;

  // Get the number of entries
    CIoc *pIoc;
    CGroup *pGroup;
    int i=0;
    resTableIter<CIoc,stringId> iter1(iocTable.firstIter());
    while((pIoc=iter1.pointer())) {
	const tsDLList<CGroup> *pGroupList=pIoc->getGroupList();
	tsDLIterBD<CGroup> iter2(pGroupList->first());
	tsDLIterBD<CGroup> eol;
	while(iter2 != eol) {
	    pGroup=iter2;
	    if(sortMode == SORT_FINISHED) {
	      // Set group to be finished if appropriate
		pGroup->checkFinished(curTime,NEW_GROUP_TIME);
	      // Only do finished groups
		if(!pGroup->isFinished()) {
		    iter2++;
		    continue;
		}
	    }
	    i++;
	    iter2++;
	}
	iter1++;
    }
    nArray=i;

  // Allocate arrays
    if(!nArray) return;
    groups=new CGroup *[nArray];
    if(!groups) {
	errMsg("Cannot allocate space for groups array");
	exit(1);
    }
    timeDiffs=new double[nArray];
    if(!timeDiffs) {
	errMsg("Cannot allocate space for timeDiffs array");
	exit(1);
    }
    indices=new int[nArray];
    if(!indices) {
	errMsg("Cannot allocate space for indices array");
	exit(1);
    }

  // Fill in arrays
  // Loop over the iocTable
    iter1=iocTable.firstIter();
    i=0;
    while((pIoc=iter1.pointer())) {
	const tsDLList<CGroup> *pGroupList=pIoc->getGroupList();
	tsDLIterBD<CGroup> iter2(pGroupList->first());
	tsDLIterBD<CGroup> eol;
	while(iter2 != eol) {
	    pGroup=iter2;
	    epicsTime time;
	    if(sortMode == SORT_FINISHED) {
	      // Only do finished groups
		if(!pGroup->isFinished()) {
		    iter2++;
		    continue;
		}
	      // Sort on last time in this case
		time=pGroup->getLastTime();
	    } else {
	      // Sort on first time
		time=pGroup->getFirstTime();
	    }
	    timeDiffs[i]=time-curTime;
	    indices[i]=i;
	    groups[i]=pGroup;
	    i++;
	    iter2++;
	}
        iter1++;
    }

  // Sort
    hsort(timeDiffs,indices,nArray);

    return;
}

static void reportByGroup()
{
    CGroup *pGroup;
    int i,index;

    for(i=0; i < nArray; i++) {
	index=indices[i];
	pGroup=groups[index];
	printGroup(pGroup);
    }
}

void removeFinished(void)
{
    CIoc *pIoc;
    CGroup *pGroup;
    int i,index;

    for(i=0; i < nArray; i++) {
	index=indices[i];
	pGroup=groups[index];
	pIoc=&pGroup->getIoc();
#if DEBUG_REALTIME
	printf(" Removing group: %s groupCount=%d\n",pIoc->resourceName(),
	  pIoc->getGroupList()->count());
#endif
      // Deleting the group should remove it from the groupList
	delete pGroup;
      // Set the current group in the ioc to NULL
	pIoc->setCurGroup(NULL);
      // If the group list in the ioc is empty, remove the ioc
	int count=pIoc->getGroupList()->count();
	if(count <= 0) {
#if DEBUG_REALTIME
	    printf(" Removing ioc: %s groupCount=%d\n",pIoc->resourceName(),
	      pIoc->getGroupList()->count());
#endif
	    iocTable.remove(*pIoc);
	    delete pIoc;
	}
    }
}

static void printGroup(CGroup *pGroup)
{
    char timeStampStr1[16];
    char timeStampStr2[16];
    
    pGroup->getFirstTime().strftime(timeStampStr1,20,"%b %d %H:%M:%S");
    pGroup->getLastTime().strftime(timeStampStr2,20,"%b %d %H:%M:%S");
    double delTime1=pGroup->getLastTime()-pGroup->getFirstTime();
    Characterization chn=characterize(pGroup);
    int nPoints=pGroup->getNPoints();
    if(terse) {
	printf("%s %s %s\n",pGroup->getIoc().resourceName(),
	  timeStampStr1,chnString[chn]);
    } else if(!verbose) {
	printf("\n%s\n",pGroup->getIoc().resourceName());
	printf(" %s\n",chnString[chn]);
	if(nPoints == 1) {
	    printf(" %s %d event(s)\n",
	      timeStampStr1,pGroup->getNPoints());
	} else if (nPoints > 1) {
	    printf(" %s %d event(s) for %.2f sec = %.2f min = %.2f hours\n",
	      timeStampStr1,pGroup->getNPoints(),
	      delTime1,delTime1/60.,delTime1/3600.);
	}
    } else {
	printf("\n%s\n",pGroup->getIoc().resourceName());
	printf(" %s\n",chnString[chn]);
	printf(" %d event(s)",pGroup->getNPoints());
	int outOfOrder=pGroup->getOutOfOrder();
	if(outOfOrder) {
	    printf(" (%d event(s) out of order)\n",outOfOrder);
	} else {
	    printf("\n");
	}
	pGroup->getLastTime().strftime(timeStampStr2,20,"%b %d %H:%M:%S");
	printf(" %s to %s (%.2f sec = %.2f min = %.2f hours)\n",
	  timeStampStr1,timeStampStr2,delTime1,delTime1/60.,delTime1/3600.);
	if(nPoints == 1) {
	    pGroup->getFirstTime().strftime(timeStampStr1,20,
	      "%b %d %H:%M:%S");
	} else if(nPoints > 1) {
	    printf(" Mean=%.2f Sigma=%.2f Min=%.2f Max=%.2f Increasing=%d",
	      pGroup->getMean(),pGroup->getSigma(),
	      pGroup->getMin(),pGroup->getMax(),pGroup->getIncreasing());
	    if(pGroup->getIntervalType() == MonotonicIncreasing) {
		printf(" Monotonically increasing\n");
	    } else if(pGroup->getIntervalType() == MonotonicIncreasing) {
		printf(" Monotonically decreasing\n");
	    } else {
		printf("\n");
	    }
	}
    }
}

static Characterization characterize(CGroup *pGroup)
{
    double max=pGroup->getMax();
    double min=pGroup->getMin();
    int nPoints=pGroup->getNPoints();
    int nIntervals=pGroup->getNIntervals();
    int outOfOrder=pGroup->getOutOfOrder();
    int increasing=pGroup->getIncreasing();
    int nonIncreasing=nIntervals-increasing;
    IntervalType type=pGroup->getIntervalType();

    if(nPoints == 1) {
	return CHN_SINGLE;
    }
    if(outOfOrder) {
	return CHN_ORDER;
    }
    if(nPoints <= SHORT_POINTS) {
	return CHN_SHORT;
    }
    if(max > 0.0 && min > 0.0 && (double)max/(double)min > MAX_MIN_RATIO) {
	if(type == MonotonicIncreasing) {
	    return CHN_SERVER;
	} else if (nonIncreasing <= MAX_NONINCREASING_INTERVALS ) {
	    return CHN_PROBABLESERVER;
	}
    }
    if(max-min < REGULAR_TOLERANCE) {
	return CHN_REGULAR;
    }
    if(nPoints < MEDIUM_POINTS) {
	return CHN_MEDIUM;
    }
    if(nPoints < LONG_POINTS) {
	return CHN_LONG;
    }
    return CHN_VERYLONG;
}

    epicsTimerQueueActive *timerQueue=NULL;
