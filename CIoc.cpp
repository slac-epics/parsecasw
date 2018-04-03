// Implementation of classes for ParseCASW

#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#include "CIoc.h"
#include "utils.h"

// Class CIoc implementations

CIoc::CIoc(const char *name, epicsTime &time) :
    stringId(name),
    firstTime(time),
    lastTime(firstTime),
    curGroup(NULL)
{
    curGroup=new CGroup(*this,time);
    if(!curGroup) {
	errMsg("Failed to create a group for %s\n",name);
	exit(1);
    }
    groupList.add(*curGroup);
}

CIoc::~CIoc(void)
{
    CGroup *pGroup;

  // Remove all the groups from the list and delete them
    while(groupList.count()) {
	pGroup=groupList.last();
      // Deleting the group should remove it from the list
	delete pGroup;
    }
}

void CIoc::update(epicsTime &time, double newGroupTime)
{
  // Update the last Time
    lastTime=time;

  // If there is no current group make one
    if(!curGroup) {
	curGroup=new CGroup(*this,time);
	if(!curGroup) {
	    errMsg("Failed to create a group for %s\n",
	      resourceName());
	    exit(1);
	}
	return;
    }

  // If there is a current group, check if it needs to be ended
  // because the time since the last time has exceeded newGroupTime
    double delTime=time-curGroup->getLastTime();
    if(delTime > newGroupTime) {
	curGroup->setFinished(1);
	curGroup=new CGroup(*this,time);
	if(!curGroup) {
	    errMsg("Failed to create a group for %s\n",
	      resourceName());
	    exit(1);
	}
	groupList.add(*curGroup);
	return;
    }

  // Else update the current group
    curGroup->update(time);
}

// Class CGroup implementations

CGroup::CGroup(CIoc &iocIn,epicsTime &time) :
    ioc(iocIn),
    firstTime(time),
    lastTime(firstTime),
    nIntervals(0),
    sum(0.0),
    sum2(0.0),
    max(DBL_MIN),
    min(DBL_MAX),
    lastInterval(0.0),
    increasing(0),
    intervalType(NoIntervals),
    finished(0),
    outOfOrder(0)
{
}

CGroup::~CGroup(void)
{
  // Remove it from the list
    tsDLList<CGroup> *pGroupList=ioc.getGroupList();
    pGroupList->remove(*this);
}

double CGroup::getMean(void) const
{
    if(nIntervals > 0) {
	double avg=sum/(double)nIntervals;
	return avg;
    } else {
	return 0;
    }
}

double CGroup::getSigma(void) const
{
  // Use sigma=sqrt(sum(x-xbar)^2/n), not n-1 version
    if(nIntervals > 1) {
	double avg=sum/(double)nIntervals;
	double arg=sum2/(double)nIntervals-avg*avg;
	if(arg > 0) return sqrt(arg);
	else return 0.0;
    } else {
	return 0;
    }
}

void CGroup::update(epicsTime &time)
{
    nIntervals++;
    double delTime=time-lastTime;
    lastTime=time;
    sum+=delTime;
    sum2+=delTime*delTime;

    if(delTime > lastInterval) increasing++;
    if(intervalType == NoIntervals) {
	if(delTime > 0.0) intervalType=MonotonicIncreasing;
	else if(delTime < 0.0) intervalType=MonotonicDecreasing;
    } else if(intervalType == MonotonicIncreasing) {
	if(delTime < max) intervalType=IncreasingDecreasing;
    } else if(intervalType == MonotonicDecreasing) {
	if(delTime > min) intervalType=IncreasingDecreasing;
    }

    if(delTime > max) max=delTime;
    if(delTime < min) min=delTime;
    if(delTime < 0) outOfOrder++;
    lastInterval=delTime;
}

void CGroup::checkFinished(epicsTime &time, double newGroupTime)
{
    double delTime=time-getLastTime();
    if(delTime > newGroupTime) {
	setFinished(1);
	getIoc().setCurGroup(NULL);
    }
}
