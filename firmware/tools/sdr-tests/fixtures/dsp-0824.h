#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
namespace Tab5SdrDsp {
constexpr unsigned N=1024,SampleRate=240000;
struct Dsp {
 float re[N]{},im[N]{},window[N]{},iMean=127.5f,qMean=127.5f,prevI=0,prevQ=0,dc=0,filter[3]{},channelI[3]{},channelQ[3]{};unsigned decimation=0;
 Dsp(){for(unsigned i=0;i<N;++i)window[i]=0.5f-0.5f*cosf(6.28318530718f*i/(N-1));}
 void resetAudio(){iMean=qMean=127.5f;prevI=prevQ=dc=0;for(auto& stage:filter)stage=0;for(auto& stage:channelI)stage=0;for(auto& stage:channelQ)stage=0;decimation=0;}
 void spectrum(const uint8_t* iq,int8_t* bins){
  for(unsigned i=0;i<N;++i){re[i]=(float(iq[2*i])-iMean)*window[i];im[i]=(float(iq[2*i+1])-qMean)*window[i];}
  for(unsigned i=1,j=0;i<N;++i){unsigned bit=N>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){std::swap(re[i],re[j]);std::swap(im[i],im[j]);}}
  for(unsigned length=2;length<=N;length<<=1){float angle=-6.28318530718f/length,cr=cosf(angle),ci=sinf(angle);for(unsigned i=0;i<N;i+=length){float wr=1,wi=0;for(unsigned j=0;j<length/2;++j){unsigned a=i+j,b=a+length/2;float r=wr*re[b]-wi*im[b],q=wr*im[b]+wi*re[b];re[b]=re[a]-r;im[b]=im[a]-q;re[a]+=r;im[a]+=q;float next=wr*cr-wi*ci;wi=wr*ci+wi*cr;wr=next;}}}
  // Full-scale complex tone, corrected for the coherent Hann window gain.
  // These are dBFS, not dBm: RF gain and antenna calibration are unknown.
  const float reference=127.5f*127.5f*(N-1)*(N-1)*0.25f;
  for(unsigned i=0;i<256;++i){float power=0;for(unsigned k=0;k<4;++k){unsigned j=(i*4+k+N/2)%N;power=std::max(power,re[j]*re[j]+im[j]*im[j]);}float db=10*log10f(std::max(power,1e-12f)/reference);bins[i]=int8_t(std::max(-120.f,std::min(0.f,db)));}
 }
 size_t audio(const uint8_t* iq,size_t bytes,unsigned mode,int16_t* out){
  size_t count=0;bool fm=mode==1||mode==3;
  for(size_t k=0;k+1<bytes;k+=2){
   // An AM carrier exactly at the centre is useful DC in the complex signal;
   // adapting its I/Q mean away would destroy envelope demodulation.
   float i=iq[k]-(fm?iMean:127.5f),q=iq[k+1]-(fm?qMean:127.5f);
   if(fm){iMean+=i*0.00005f;qMean+=q*0.00005f;}
   if(mode!=1){float a=mode==3?0.27f:0.19f;for(unsigned stage=0;stage<3;++stage){channelI[stage]+=a*(i-channelI[stage]);channelQ[stage]+=a*(q-channelQ[stage]);i=channelI[stage];q=channelQ[stage];}}
   float v=fm?atan2f(prevI*q-prevQ*i,prevI*i+prevQ*q):sqrtf(i*i+q*q);prevI=i;prevQ=q;dc+=(v-dc)*0.0001f;v-=dc;
   float alpha=mode==1?0.28f:0.1f;for(auto& stage:filter){stage+=alpha*(v-stage);v=stage;}
   if(++decimation==5){decimation=0;float scaled=v*(fm?(mode==3?48000.f:12000.f):320.f);out[count++]=int16_t(std::max(-32768.f,std::min(32767.f,scaled)));}
  }return count;
 }
};
}
