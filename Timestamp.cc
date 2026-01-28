#include "Timestamp.h"

#include <cstdio>
#include <time.h>


Timestamp::Timestamp():microSecondsSinceEpoch_(0){}
Timestamp::Timestamp(int64_t microSecondsSinceArg):
microSecondsSinceEpoch_(microSecondsSinceArg){}
Timestamp Timestamp::Now(){
    time_t ti = time(NULL);
    return Timestamp(ti);
}
std::string Timestamp::toString() const{
    char buf[128]={0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf, 128, "%04d/%02d/%02d %02d:%02d:%02d", tm_time->tm_year+1900,
    tm_time->tm_mon+1,tm_time->tm_mday,tm_time->tm_hour,tm_time->tm_min,tm_time->tm_sec);
    return buf;
}

