// Header file for ParseCASW

#ifndef _INC_PARSECASW_H
#define _INC_PARSECASW_H

#define READ_LINESIZE 512

#define P_OK 0
#define P_ERROR 1

#ifdef WIN32
# define DIR_DELIMITER_CHAR '\\'
# define DIR_DELIMITER_STRING "\\"
// Hummingbird extra functions including lprintf
//   Needs to be included after Intrinsic.h for Exceed 5
# include <stdio.h>
# include <X11/Intrinsic.h>
# include <X11/XlibXtra.h>
// The following is done in Exceed 6 but not in Exceed 5
//   Need it to define printf as lprintf for Windows
//   (as opposed to Console) apps */
# ifdef _WINDOWS
#  ifndef printf
#   define printf lprintf
#  endif    
# endif    
#else // #ifdef WIN32
// WIN32 does not have unistd.h
# define DIR_DELIMITER_CHAR '/'
# define DIR_DELIMITER_STRING "/"
# include <stdio.h>
# include <unistd.h>
# include <limits.h>
#endif // #ifdef WIN32

#include <sys/stat.h>
#include <sys/types.h>
//#include <sys/utsname.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

// PATH_MAX
#ifdef WIN32
// Is in stdlib.h for WIN32
#define PATH_MAX _MAX_PATH
#endif

#include <epicsTime.h>
#include <epicsTimer.h>

class CParseTimer;

class CParseTimer : public epicsTimerNotify
{
  public:
    CParseTimer(epicsTimerQueue &queue, double intervalIn) : 
      interval(intervalIn), startTime(epicsTime::getCurrent()),
      timer(queue.createTimer()) {}
    virtual expireStatus expire(const epicsTime &curTime);
    void start() { timer.start(*this,interval); }
    void stop() { timer.cancel(); }
  protected:
    virtual ~CParseTimer() { timer.destroy(); }
  private:
    double interval;
    epicsTime startTime;
    epicsTimer &timer;
};


#endif // _INC_PARSECASW_H
