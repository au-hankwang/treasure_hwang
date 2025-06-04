#ifndef __PCH_H_INCLUDED__
#define __PCH_H_INCLUDED__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/timeb.h>
#include <time.h>
#include <math.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <functional>

#include "windefinitions.h"
#include "unixdefinitions.h"
#include "debug.h"
#include "common.h"
#include "multithread.h"
#include "gzip.h"
//#include "template.h"

using namespace std;

typedef unsigned int   uint32;
typedef unsigned short uint16;
typedef unsigned char  uint8;

#define MINIMUM(a,b) ( ((a)<(b)) ? (a) : (b))
#define MAXIMUM(a,b) ( ((a)>(b)) ? (a) : (b))


#endif
