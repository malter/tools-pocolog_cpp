#include "FileStream.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

pocolog_cpp::FileStream::FileStream() : fd(-1)
{

}

pocolog_cpp::FileStream::FileStream(const char* __s, std::ios_base::openmode mode)
{
    if(!open(__s, mode))
    {
        throw std::runtime_error(std::string("Error opening file") + __s);
    }
}

bool pocolog_cpp::FileStream::open(const char* fileName, std::ios_base::openmode mode)
{
    fd = ::open(fileName, O_NONBLOCK);
    if(fd < 0)
    {
        goodFlag = false;
        return false;
    }
    
    struct stat stats;
    int ret = ::fstat(fd, &stats);
    if(ret < 0)
    {
        goodFlag = false;
        return false;
    }
    
    fileSize = stats.st_size;
    blockSize = stats.st_blksize;
    
    goodFlag = true;
    readBufferPosition = -1;
    readBufferEndPosition = -1;
    bufferSize = 1024*1024;
    readBuffer.resize(bufferSize);
    readPos = 0;
    writePos = 0;
    
//     std::cout << "File opened " << (fd > 0) << " file size " << fileSize << std::endl;
    
    return true;
}

bool pocolog_cpp::FileStream::posInBuffer(off_t pos)
{
    return pos >= readBufferPosition && pos < readBufferEndPosition;
}

bool pocolog_cpp::FileStream::reloadBuffer(off_t position)
{
//     std::cout << "Loading Buffer " << position << std::endl;
    off_t newPos = ::lseek(fd, position, SEEK_SET);
    if(newPos == -1)
    {
        goodFlag = false;
        std::cout << "Seek Failed" << std::endl;
        return false;
    }

    assert(position == newPos);
    
    readBufferPosition = position;
    readBufferEndPosition = position + bufferSize;
    
    size_t toRead = bufferSize;
    if(readBufferEndPosition > fileSize)
        toRead = fileSize - readBufferPosition;
    
    size_t readSize = 0;
    while(readSize < toRead)
    {
        int ret = ::read(fd, readBuffer.data() + readSize, toRead - readSize);
        if(ret < 0)
        {
            goodFlag = false;
            std::cout << "Read Error" << std::endl;
            return false;
        }
        if(ret == 0)
        {
            //should not happen
            throw std::runtime_error("Internal error in FileStream, Read returned EOF");
            //end of file reached
            break;
        }
        readSize += ret;
    }
    
//     std::cout << "Buffer Loaded " << readBufferPosition << " end " << readBufferEndPosition << std::endl;
    
    return true;
}

void pocolog_cpp::FileStream::read(char* buffer, size_t size)
{
//     std::cout << "Reading Bytes from pos " << readPos << std::endl;

    size_t posInBuf = readPos - readBufferPosition;
    
    for(size_t i = 0; i < size; i++)
    {
        if(eof())
        {
            goodFlag = false;
            return;
        }
        
        if(!posInBuffer(readPos))
        {
            //load new buffer
            if(!reloadBuffer(readPos))
                return;
            
            posInBuf = readPos - readBufferPosition;
            assert(posInBuf == 0);
        }
        
        buffer[i] = readBuffer[posInBuf];
        posInBuf++;
        readPos++;
    }
    
}

bool pocolog_cpp::FileStream::good() const
{
    return goodFlag;
}

bool pocolog_cpp::FileStream::fail() const
{
    return !goodFlag;
}

std::streampos pocolog_cpp::FileStream::seekg(std::streampos pos)
{
    readPos = pos;
    return readPos;
}

std::streampos pocolog_cpp::FileStream::seekp(std::streampos pos)
{
    writePos = pos;
    return writePos;
}

std::streampos pocolog_cpp::FileStream::tellg()
{
    return readPos;
}

std::streampos pocolog_cpp::FileStream::tellp()
{
    return writePos;
}

bool pocolog_cpp::FileStream::eof() const
{
    return readPos >= fileSize;
}

void pocolog_cpp::FileStream::close()
{
    goodFlag = false;
    if(fd > 0)
        ::close(fd);
    fd = -1;
}

