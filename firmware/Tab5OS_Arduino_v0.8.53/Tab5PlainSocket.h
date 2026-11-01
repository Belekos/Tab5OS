#pragma once
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
// PlainStart in the bundled core has an uninitialised fd_set in peek_net_receive.
// Use the raw descriptor only until the 101 header ends; TLS owns all later bytes.
namespace tab5socket {
inline int available(int fd){int n=0;return fd>=0&&ioctl(fd,FIONREAD,&n)==0?n:0;}
inline int read(int fd,uint8_t* p,size_t n){return n?recv(fd,p,n,MSG_DONTWAIT):0;}
inline size_t write(int fd,const uint8_t* p,size_t n){int r=send(fd,p,n,MSG_DONTWAIT);return r>0?size_t(r):0;}
inline uint8_t connected(int fd){uint8_t c;int r=recv(fd,&c,1,MSG_PEEK|MSG_DONTWAIT);return r>0||(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK));}
}
