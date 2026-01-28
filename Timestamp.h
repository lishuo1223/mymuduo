#pragma once

#include <iostream>
#include <string>
//时间类
class Timestamp{
public:
    Timestamp();
    Timestamp(int64_t microSecondsSinceArg);
    static Timestamp Now();
    std::string toString() const;
private:
    int64_t microSecondsSinceEpoch_;
    
};