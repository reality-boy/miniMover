#include <Windows.h>
#include "timer.h"

LARGE_INTEGER msTime::StartingTime;
LARGE_INTEGER msTime::Frequency;
bool msTime::init = false;
