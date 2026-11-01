#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
namespace Tab5SdrDsp {
constexpr unsigned N=1024,SampleRate=240000,AudioRate=48000,AudioDecimation=SampleRate/AudioRate;
struct Dsp {
 // A broadcast FM multiplex also contains a 19 kHz pilot, stereo sidebands
 // and RDS. Filter BEFORE decimating: after aliasing they cannot be removed.
 // Symmetric 257-tap Blackman FIR, 15.5 kHz cutoff, evaluated only at 48 kHz.
 static constexpr unsigned AudioTaps=257;
 float audioTaps[(AudioTaps+1)/2]{},audioHistory[AudioTaps*2]{};
 unsigned audioPosition=0;float deemphasis=0,deemphasisAlpha=1;bool pendingByte=false;uint8_t pendingI=0;
 float re[N]{},im[N]{},window[N]{},iMean=127.5f,qMean=127.5f,prevI=0,prevQ=0,dc=0,filter[3]{},channelI[3]{},channelQ[3]{};unsigned decimation=0;
 Dsp(){for(unsigned i=0;i<N;++i)window[i]=0.5f-0.5f*cosf(6.28318530718f*i/(N-1));
  float sum=0;for(unsigned i=0;i<=(AudioTaps-1)/2;++i){int distance=int(i)-int((AudioTaps-1)/2);float x=6.28318530718f*15500.f/SampleRate;
   float tap=(distance?sinf(x*distance)/(3.14159265359f*distance):2*15500.f/SampleRate)*(0.42f-0.5f*cosf(6.28318530718f*i/(AudioTaps-1))+0.08f*cosf(12.56637061436f*i/(AudioTaps-1)));
   audioTaps[i]=tap;sum+=tap*(i==(AudioTaps-1)/2?1:2);}
  for(auto& tap:audioTaps)tap/=sum;resetAudio();
 }
 void resetAudio(unsigned microseconds=50){iMean=qMean=127.5f;prevI=prevQ=dc=0;for(auto& stage:filter)stage=0;for(auto& stage:channelI)stage=0;for(auto& stage:channelQ)stage=0;decimation=0;audioPosition=0;pendingByte=false;deemphasis=0;deemphasisAlpha=microseconds?1.f-expf(-1000000.f/(AudioRate*float(microseconds))):1.f;for(auto& sample:audioHistory)sample=0;}
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
  for(size_t k=0;k<bytes;){
   uint8_t rawI;if(pendingByte){rawI=pendingI;pendingByte=false;}else {rawI=iq[k++];if(k==bytes){pendingI=rawI;pendingByte=true;break;}}uint8_t rawQ=iq[k++];
   // A centred FM signal also has a useful carrier (especially at low
   // deviation). Subtracting its moving I/Q mean removes that carrier and
   // distorts the discriminator. Remove the ADC midpoint, then audio DC.
   float i=rawI-127.5f,q=rawQ-127.5f;
   if(mode!=1){float a=mode==3?0.27f:0.19f;for(unsigned stage=0;stage<3;++stage){channelI[stage]+=a*(i-channelI[stage]);channelQ[stage]+=a*(q-channelQ[stage]);i=channelI[stage];q=channelQ[stage];}}
   float v=fm?atan2f(prevI*q-prevQ*i,prevI*i+prevQ*q):sqrtf(i*i+q*q);prevI=i;prevQ=q;dc+=(v-dc)*0.0001f;v-=dc;
   if(mode==1){audioHistory[audioPosition]=audioHistory[audioPosition+AudioTaps]=v;if(++audioPosition==AudioTaps)audioPosition=0;}
   else for(auto& stage:filter){stage+=0.1f*(v-stage);v=stage;}
   if(++decimation==AudioDecimation){decimation=0;
    if(mode==1){const float* history=audioHistory+audioPosition;v=audioTaps[(AudioTaps-1)/2]*history[(AudioTaps-1)/2];for(unsigned j=0;j<(AudioTaps-1)/2;++j)v+=audioTaps[j]*(history[j]+history[AudioTaps-1-j]);deemphasis+=deemphasisAlpha*(v-deemphasis);v=deemphasis;}
    float scaled=v*(fm?(mode==3?48000.f:12000.f):320.f);out[count++]=int16_t(std::max(-32768.f,std::min(32767.f,scaled)));}
  }return count;
 }
};
}
