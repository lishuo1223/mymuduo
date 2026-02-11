#include "Buffer.h"
#include <error.h>
#include <sys/uio.h>
#include <unistd.h>

//从fd上读取数据，Poller工作在LT模式
ssize_t Buffer::readFd(int fd,int* saveErrorno){
    char extrabuf[65536] = {0};  //栈上内存空间
    struct iovec vec[2];
    const size_t writeable = writeableBytes();  //buffer缓冲区底层剩余的可写空间的大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writeable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writeable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd,vec,iovcnt);
    if(n < 0){
        *saveErrorno = errno;
    }
    else if(n <= writeable){  //buffer的可写缓冲区已经够存储读出来的数据
        writerIndex_ += n;
    }
    else{
        writerIndex_ = buffer_.size();
        append(extrabuf,n - writeable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd,int * saveErrorno){
    ssize_t n = ::write(fd,peek(),readableBytes());
    if(n < 0){
        *saveErrorno = errno;
    }
    return n;
}
