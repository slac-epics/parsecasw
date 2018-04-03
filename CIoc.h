// Classes for ParseCASW

#ifndef _INC_CIOC_H
#define _INC_CIOC_H

#include <epicsTime.h>
#include <resourceLib.h>
#include "tsDLList.h"

#endif // _INC_CIOC_H

typedef enum _IntervalType {
    NoIntervals,
    IncreasingDecreasing,
    MonotonicIncreasing,
    MonotonicDecreasing
} IntervalType;

class CIoc;
class CGroup;

class CIoc : public tsSLNode <CIoc>, public stringId
{
  public:
    CIoc(const char *name, epicsTime &time);
    ~CIoc(void);
    tsDLList<CGroup> *getGroupList(void) { return &groupList; }
    epicsTime getFirstTime(void) const { return firstTime; }
    epicsTime getLastTime(void) const { return lastTime; }
    
    unsigned getGroupCount(void) const { return groupList.count(); }
    void update(epicsTime &time, double newGroupTime);
    CGroup *getCurGroup(void) const { return curGroup; }
    void setCurGroup(CGroup *curGroupIn) { curGroup=curGroupIn; }

  private:
    tsDLList<CGroup> groupList;
    epicsTime firstTime;
    epicsTime lastTime;
    CGroup *curGroup;
};

class CGroup : public tsDLNode<CGroup>
{
  public:
    CGroup(CIoc &ioc, epicsTime &time);
    ~CGroup(void);

    epicsTime getFirstTime(void) const { return firstTime; }
    epicsTime getLastTime(void) const { return lastTime; }
    
    void update(epicsTime &time);
    void checkFinished(epicsTime &time, double newGroupTime);
    int getNPoints(void) const { return nIntervals+1; }
    int getNIntervals(void) const { return nIntervals; }
    double getMean(void) const;
    double getSigma(void) const;
    double getMin(void) const { return min; }
    double getMax(void) const { return max; }
    int isFinished(void) const { return finished; }
    void setFinished(int val) { finished=val; }
    IntervalType getIntervalType(void) const { return intervalType; }
    int getOutOfOrder(void) const { return outOfOrder; }
    int getIncreasing(void) const { return increasing; }
    CIoc &getIoc(void) const { return ioc; }

  private:
    CIoc &ioc;
    epicsTime firstTime;
    epicsTime lastTime;
    int nIntervals;
    double sum;
    double sum2;
    double max;
    double min;
    double lastInterval;
    int increasing;
    IntervalType intervalType;
    int finished;
    int outOfOrder;
};
