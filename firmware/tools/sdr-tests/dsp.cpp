#include "Tab5SdrDsp.h"
#include "baseline-dsp.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <chrono>
constexpr double Pi=3.14159265358979323846;
unsigned checks=0;
void check(bool ok,const char* why){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
std::vector<uint8_t> signal(double tone=1000,double deviation=60000,double offset=0,double amplitude=85,bool multiplex=false,bool varying=false){
 std::vector<uint8_t> data(240000*2);double phase=0;uint32_t noise=1234567;
 for(size_t n=0;n<data.size()/2;++n){double t=double(n)/240000;double fm=deviation*sin(2*Pi*tone*t);
  if(multiplex)fm+=7500*sin(2*Pi*19000*t)+30000*cos(2*Pi*38000*t)*sin(2*Pi*2000*t)+4000*sin(2*Pi*57000*t);
  phase+=2*Pi*(offset+fm)/240000;double a=amplitude*(varying?1+.2*sin(2*Pi*233*t):1);
  for(unsigned channel=0;channel<2;++channel){noise=noise*1664525u+1013904223u;double r=(double(noise>>16)/65535.-.5)*.8;
   double value=127.5+a*(channel?sin(phase):cos(phase))+r+(varying?(channel?-.25:.25):0);data[n*2+channel]=uint8_t(std::clamp(std::lround(value),0l,255l));}
 }return data;
}
template<class DSP>std::vector<int16_t> demod(const std::vector<uint8_t>& iq,DSP& dsp,bool fragmented=false){
 std::vector<int16_t> result;int16_t out[512];size_t pos=0;unsigned part=0;const unsigned chunks[]={1,127,9,1024,511,3001,4095,2};
 while(pos<iq.size()){size_t n=std::min(iq.size()-pos,size_t(fragmented?chunks[part++%8]:4096));size_t count=dsp.audio(iq.data()+pos,n,1,out);check(count<=410,"PCM bound for any packet size");result.insert(result.end(),out,out+count);pos+=n;}return result;
}
double amplitude(const std::vector<int16_t>& samples,double hz){double re=0,im=0;for(size_t n=4800;n<samples.size();++n){double p=2*Pi*hz*n/48000;re+=samples[n]*cos(p);im+=samples[n]*sin(p);}return 2*hypot(re,im)/(samples.size()-4800);}
double snr(const std::vector<int16_t>& samples,double hz){double sum=0,power=0;for(size_t n=4800;n<samples.size();++n){sum+=samples[n];power+=double(samples[n])*samples[n];}double a=amplitude(samples,hz),length=samples.size()-4800,signalPower=a*a*.5;return 10*log10(signalPower/std::max(1e-9,power/length-(sum/length)*(sum/length)-signalPower));}
int main(){
 double worst=100;
 for(double offset:{-20000.,0.,20000.})for(double level:{12.,85.}){auto iq=signal(1000,75000,offset,level,false,true);Tab5SdrDsp::Dsp dsp;auto out=demod(iq,dsp);double s=snr(out,1000);worst=std::min(worst,s);printf("FM 75k offset %.0f level %.0f SNR %.2f dB\n",offset,level,s);check(s>29,"75 kHz FM remains intelligible across tuner offset and input amplitude");check(out.size()==48000,"exact sample rate across USB boundaries");}
 auto iq=signal(1000,25000,12000,85,true);Tab5SdrDsp::Dsp modern;BaselineSdrDsp::Dsp legacy;auto now=demod(iq,modern),old=demod(iq,legacy);
 auto aliases=[](const std::vector<int16_t>& a){return hypot(amplitude(a,8000),hypot(amplitude(a,9000),amplitude(a,12000)))/amplitude(a,1000);};double oldDb=20*log10(aliases(old)),newDb=20*log10(aliases(now));
 printf("Stereo/RDS aliases old %.2f dBc, new %.2f dBc; improvement %.2f dB\n",oldDb,newDb,oldDb-newDb);check(oldDb-newDb>20&&newDb< -45,"broadcast multiplex aliases rejected before decimation");
 Tab5SdrDsp::Dsp contiguous,fragmented;check(demod(iq,contiguous)==demod(iq,fragmented,true),"odd, short and full USB chunks are sample-exact equivalent");
 double ratios[3];for(unsigned index=0;index<3;++index){unsigned us=index==0?0:index==1?50:75;Tab5SdrDsp::Dsp low,high;low.resetAudio(us);high.resetAudio(us);ratios[index]=amplitude(demod(signal(8000,15000),high),8000)/amplitude(demod(signal(1000,15000),low),1000);}
 auto quiet=signal(1000,1500);Tab5SdrDsp::Dsp quietNew;BaselineSdrDsp::Dsp quietOld;double quietBefore=snr(demod(quiet,quietOld),1000),quietAfter=snr(demod(quiet,quietNew),1000);printf("Quiet centred carrier old %.2f dB SNR, new %.2f dB SNR\n",quietBefore,quietAfter);check(quietAfter>40&&quietAfter-quietBefore>20,"do not subtract the desired centred FM carrier at low deviation");
 printf("De-emphasis ratios off %.4f / 50us %.4f / 75us %.4f\n",ratios[0],ratios[1],ratios[2]);check(ratios[0]>.95&&ratios[0]<1.05,"mono FIR preserves 1-8 kHz without de-emphasis");check(ratios[1]>.36&&ratios[1]<.44&&ratios[2]>.27&&ratios[2]<.35&&ratios[2]<ratios[1],"50 and 75 microsecond de-emphasis follow expected response");
 modern.resetAudio();Tab5SdrDsp::Dsp fresh;check(demod(iq,modern)==demod(iq,fresh),"retune clears all history, decimation and split-byte state");
 printf("PASS %u DSP checks; minimum clean FM SNR %.2f dB; old aliases %.2f dBc; new aliases %.2f dBc; physical hardware not measured\n",checks,worst,oldDb,newDb);
}
