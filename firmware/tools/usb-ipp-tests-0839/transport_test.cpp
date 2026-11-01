#include "printer.cpp"
#include <iostream>
static unsigned checks=0;
#define CHECK(x) do{++checks;assert(x);}while(0)
struct Source:Stream{std::string body="IPP-DATA";size_t at=0;bool truncated=false;size_t readBytes(char* out,size_t n)override{n=std::min(n,body.size()-at);if(truncated)n=0;memcpy(out,body.data()+at,n);at+=n;return n;}};
static bool cancelled=false;
static bool cancel(void*){return cancelled;}
static void reset(){using namespace Tab5UsbPrinter;if(device)CHECK(closeDevice());if(gate)vSemaphoreDelete(gate);if(completed)vSemaphoreDelete(completed);if(arrivals)vQueueDelete(arrivals);gate=completed=nullptr;arrivals=nullptr;client=nullptr;usable=false;gone=false;poisoned=false;generation=1;changed=0;reserveFail=allocateFail=taskFail=claimFail=shortOut=timeoutInput=pendingTransfer=cancelled=false;submitted=outs=ins=claims=releases=closed=halts=flushes=0;now=0;fragment=2048;rxAt=0;tx.clear();rx="HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nOKAY";onSubmit={};descriptor[16]=4;CHECK(live==0);CHECK(begin());attach(1);CHECK(ready());}
static bool post(Source& source,Tab5UsbPrinter::Response& output,int& status){return Tab5UsbPrinter::post(source,source.body.size(),output,status,Tab5UsbPrinter::connectionToken(),15000,cancel,nullptr);}
int main(){using namespace Tab5UsbPrinter;
 reset();{Source s;Response out;int status;CHECK(post(s,out,status));CHECK(status==200&&out.size()==4);CHECK(!memcmp(out.data(),"OKAY",4));CHECK(tx.find("POST /ipp/print HTTP/1.1\r\nHost: localhost\r\n") == 0);CHECK(tx.find("Content-Length: 8\r\n")!=std::string::npos);CHECK(tx.substr(tx.size()-8)==s.body);CHECK(ready());CHECK(gate->count==1);}
 reset();{fragment=1;Source s;Response out;int status;CHECK(post(s,out,status));CHECK(out.size()==4);CHECK(ins==int(rx.size()));CHECK(ready());}
 reset();{timeoutInput=true;Source s;Response out;int status;CHECK(post(s,out,status));CHECK(halts==1&&flushes==1);CHECK(out.size()==4);CHECK(ready());}
 reset();{Source s;Response out;int status;reserveFail=true;CHECK(!post(s,out,status));CHECK(submitted==0);CHECK(ready());}
 reset();{Source s;Response out;int status;allocateFail=true;CHECK(!post(s,out,status));CHECK(submitted==0);CHECK(ready());}
 reset();{Source s;Response out;int status;CHECK(!Tab5UsbPrinter::post(s,s.body.size(),out,status,connectionToken()-1,15000,cancel,nullptr));CHECK(submitted==0);CHECK(ready());}
 reset();{Source s;Response out;int status;cancelled=true;CHECK(!post(s,out,status));CHECK(submitted==0);CHECK(ready());}
 reset();{Source s;Response out;int status;onSubmit=[](){cancelled=true;};CHECK(!post(s,out,status));CHECK(outs==1);CHECK(!ready());onSubmit={};cancelled=false;const int before=submitted;CHECK(!post(s,out,status));CHECK(submitted==before);}
 reset();{Source s;Response out;int status;s.truncated=true;CHECK(!post(s,out,status));CHECK(outs==1);CHECK(!ready());}
 reset();{Source s;Response out;int status;shortOut=true;CHECK(!post(s,out,status));CHECK(outs==1);CHECK(!ready());}
 reset();{Source s;Response out;int status;rx="HTTP/1.1 200 OK\r\nContent-Length: 9\r\n\r\nshort";CHECK(!post(s,out,status));CHECK(!ready());CHECK(now>=15000);}
 reset();{Source s;Response out;int status;rx="HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nOKAYextra";CHECK(!post(s,out,status));CHECK(!ready());}
 reset();{Source s;Response out;int status;rx="HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";CHECK(post(s,out,status));CHECK(status==400);CHECK(ready());}
 reset();{Source s;Response out;int status;rx="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nOKAY\r\n0\r\n\r\n";fragment=7;CHECK(post(s,out,status));CHECK(out.size()==4);CHECK(ready());}
 reset();{Source s;Response out;int status;onSubmit=[](){usb_host_client_event_msg_t e{};e.event=USB_HOST_CLIENT_EVENT_DEV_GONE;e.dev_gone.dev_hdl=usbDevice;usbEvent(&e,nullptr);};CHECK(!post(s,out,status));CHECK(!ready());CHECK(closed==0);onSubmit={};CHECK(closeDevice());CHECK(closed==1);}
 reset();{Source s;Response out;int status;s.body=std::string(5000,'z');CHECK(post(s,out,status));CHECK(outs==4);CHECK(tx.substr(tx.size()-5000)==s.body);}
 reset();{CHECK(closeDevice());descriptor[16]=2;attach(1);CHECK(!ready());CHECK(claims==1);descriptor[16]=4;attach(1);CHECK(ready());}
 CHECK(closeDevice());vSemaphoreDelete(gate);vSemaphoreDelete(completed);vQueueDelete(arrivals);gate=completed=nullptr;arrivals=nullptr;client=nullptr;CHECK(live==0);
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<"}\n";
}
