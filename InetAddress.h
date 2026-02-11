#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <string.h>
//封装socket地址类型
class InetAddress{
public:
    InetAddress() { 
    ::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
  }
    explicit InetAddress(uint16_t port,std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in& addr_):addr_(addr_){}

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;
    const sockaddr_in* getSockAddr() const{
        return &addr_;
    };
    void setSockaddr(const sockaddr_in &addr){
        addr_ = addr;
    }
private:
    sockaddr_in addr_;
};