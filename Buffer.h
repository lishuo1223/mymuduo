#pragma once

#include "noncopyable.h"

#include <vector>
#include <algorithm>
#include <string>
//网络库底层的缓冲区
class Buffer{
public:
    static const size_t KCheapPrepend = 8;
    static const size_t KInitialSize = 1024;
    explicit Buffer(size_t initialSize = KInitialSize)
        :buffer_(KCheapPrepend + KInitialSize)
        ,readerIndex_(KCheapPrepend)
        ,writerIndex_(KCheapPrepend){}
    size_t readableBytes() const {
        return writerIndex_ - readerIndex_;
    }
    size_t writeableBytes() const {
        return buffer_.size() - writerIndex_;
    }
    size_t prependableBytes() const{
        return readerIndex_;
    }
    //返回缓冲区中的起始地址
    const char* peek() const{
        return begin() + readerIndex_;
    }
    //onMessage
    void retrieve(size_t len){
        if(len < readableBytes()){
            readerIndex_ += len;
        }
        else{
            retrieveAll();
        }
    }
    void retrieveAll(){
        readerIndex_ = KCheapPrepend;
        writerIndex_ = KCheapPrepend;
    }
    std::string retrieveAsString(size_t len){
        std::string result(peek(),len);
        retrieve(len);//上面一句把缓冲区中的可读的数据，已经读取出来，这里肯定要对缓冲区进行复位操作
        return result;
    }
    //把onmessage上报的buffer数据，转成string数据返回
    std::string retrieveAllAsString(){
        return retrieveAsString(readableBytes());
    }
    //往缓冲区写len数据
    void ensureWriterableBytes(size_t len){
        if(writeableBytes() < len){
            makeSpace(len); //扩容函数
        } 
    }
    //把[data,data+len]内存上的数据，添加到writeable缓冲区中
    void append(const char* data,size_t len){
        ensureWriterableBytes(len);
        std::copy(data,data+len,beginWriter());
        writerIndex_ += len;
    }
    char* beginWriter(){
        return begin() + writerIndex_;
    }
    const char* beginWriter() const{
        return begin() + writerIndex_;
    }
    //从fd上读取数据
    ssize_t readFd(int fd,int* saveErrorno);
    //通过fd发生数据
    ssize_t writeFd(int fd,int * saveErrorno);
private:
    char* begin(){
        return &*buffer_.begin();
    }
    const char* begin() const{
        return &*buffer_.begin();
    }
    std::vector<char> buffer_;
    void makeSpace(size_t len){
        if(writeableBytes() + prependableBytes() < len + KCheapPrepend){
            buffer_.resize(writerIndex_ + len);
        }
        else{
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                    begin() + writerIndex_,
                    begin() + KCheapPrepend);
            readerIndex_ = KCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }
    size_t readerIndex_;
    size_t writerIndex_;
};