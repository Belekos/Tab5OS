#pragma once
#include "Tab5CooperativeClient.h"
#include "Tab5SecureClient.h"
class Tab5BoundedClient:public Tab5CooperativeClient<Tab5SecureClient> {
 size_t remaining_;
 public:
 explicit Tab5BoundedClient(size_t bytes,Cancel cancel=nullptr,void* context=nullptr):Tab5CooperativeClient<Tab5SecureClient>(cancel,context),remaining_(bytes){}
 int available()override{if(!remaining_){stop();return 0;}return Tab5CooperativeClient::available();}
 int read(uint8_t* p,size_t n)override{if(!remaining_){stop();return -1;}if(n>remaining_)n=remaining_;int got=Tab5CooperativeClient::read(p,n);if(got>0)remaining_-=got;return got;}
 int read()override{uint8_t c;return read(&c,1)==1?c:-1;}
};
