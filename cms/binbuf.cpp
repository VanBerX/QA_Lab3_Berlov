#include <string.h>
#include "binbuf.hpp"


BinaryBuffer::BinaryBuffer()
{
    data = new char[maxlen = 512];
    datalen = 0;
}

BinaryBuffer::~BinaryBuffer()
{
    delete[] data;
}

void BinaryBuffer::AddData(const char *buf, int size)
{
    ProvideMaxLen(datalen + size);
    memcpy(data + datalen, buf, size);
    datalen += size;
}

void BinaryBuffer::AddChar(char c)
{
    ProvideMaxLen(datalen + 1);
    data[datalen] = c;
    datalen++;
}

int BinaryBuffer::GetData(char *buf, int size)
{
    if(datalen == 0)
        return 0; // a bit of optimization ;-)
    if(size >= datalen) { // all data to be read/removed
        memcpy(buf, data, datalen);
        int ret = datalen;
        datalen = 0;
        return ret;
    } else { // only a part of the data to be removed
        memcpy(buf, data, size);
        DropData(size);
        return size;
    }
}

void BinaryBuffer::DropData(int size)
{
    EraseData(0, size);
}

void BinaryBuffer::EraseData(int index, int size)
{
    if(index+size>=datalen) {
        datalen = index;
    } else {
        memmove(data+index, data+index+size, datalen - (index+size));
        datalen -= size;
    }
}

void BinaryBuffer::AddString(const char *str)
{
    if(!str) return;
    AddData(str, strlen(str));
}


int BinaryBuffer::ReadLine(char *buf, int bufsize)
{
    int crindex = -1;
    for(int i=0; i< datalen; i++)
        if(data[i] == '\n') { crindex = i; break; }
    if(crindex == -1) return 0;
    if(crindex >= bufsize) { // no room for the whole string
        memcpy(buf, data, bufsize-1);
        buf[bufsize-1] = 0;
        DropData(crindex+1);
    } else {
        GetData(buf, crindex+1);
        //assert(buf[crindex] == '\n');
        buf[crindex] = 0;
        if(buf[crindex - 1] == '\r')
            buf[crindex-1] = 0;
    }
    return crindex + 1;
}

bool BinaryBuffer::ReadLine(BinaryBuffer &dest)
{
    int crindex = -1;
    for(int i=0; i< datalen; i++)
        if(data[i] == '\n') { crindex = i; break; }
    if(crindex == -1) return false;
    dest.ProvideMaxLen(crindex+1);
    GetData(dest.data, crindex+1);
    dest.datalen = crindex+1;
    //assert(dest.data[crindex] == '\n');
    dest.data[crindex] = 0;
    if(dest.data[crindex-1] == '\r')
        dest.data[crindex-1] = 0;
    return true;
}

int BinaryBuffer::FindLineMarker(const char *marker) const
{
    for(int i = 0; i< datalen; i++) {
        if(data[i] == '\r') i++;
        if(data[i] != '\n') continue;
        bool found = true;
        int j = 1;
        for(const char *p = marker;*p;p++,j++) {
            if(/* data[i+j]=='\0' ||*/ data[i+j] != *p)
                { found = false; break; }
        }
        if(!found) continue;
        if(data[i+j] == '\r') j++;
        if(data[i+j] != '\n') continue;
        return i+j;
    }
    return -1;
}

int BinaryBuffer::ReadUntilLineMarker(const char *marker, char *buf, int bufsize)
{
    int ind = FindLineMarker(marker);
    if(ind == -1) return -1;
    if(bufsize >= ind+1) {
        return GetData(buf, ind+1);
    } else {
        return GetData(buf, bufsize);
    }
}

bool BinaryBuffer::ReadUntilLineMarker(const char *marker, BinaryBuffer &dest)
{
    int ind = FindLineMarker(marker);
    if(ind == -1) return false;
    dest.ProvideMaxLen(ind+2);
    GetData(dest.data, ind+1);
    dest.data[ind+1] = 0;
    dest.datalen = ind+1;
    return true;
}

bool BinaryBuffer::ContainsExactText(const char *str) const
{
    int i;
    for(i=0; (i<datalen) && (str[i]!=0); i++) {
        if(str[i]!=data[i]) return false;
    }
    return (i==datalen) && (str[i]==0);
}

void BinaryBuffer::ProvideMaxLen(int n)
{
    if(n <= maxlen) return;
    int newlen = maxlen;
    while(newlen < n) newlen*=2;
    char *newbuf = new char[newlen];
    memcpy(newbuf, data, datalen);
    delete [] data;
    data = newbuf;
    maxlen = newlen;
}

