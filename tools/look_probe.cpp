// Root-only, read-only sampling of explicitly named float ranges in a process.
// Does not enumerate memory or write to the target process.
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>
struct Channel {uint64_t address;int count;float low[16]{},high[16]{},maxAbs[16]{};unsigned nonzero[16]{},bad=0;};
int main(int argc,char** argv){
    if(argc<4||argc>10)return 2;
    int pid=std::atoi(argv[1]),seconds=std::atoi(argv[2]);if(pid<1||seconds<1||seconds>50)return 2;
    std::vector<Channel> channels;
    for(int i=3;i<argc;i++){char* end=nullptr;uint64_t address=std::strtoull(argv[i],&end,0);if(!end||*end!=':')return 2;int n=std::atoi(end+1);if(address<4096||n<1||n>16)return 2;channels.push_back(Channel{address,n});}
    char path[64];std::snprintf(path,sizeof(path),"/proc/%d/mem",pid);int fd=open(path,O_RDONLY);if(fd<0){std::perror("open mem");return 1;}
    auto begin=std::chrono::steady_clock::now();unsigned samples=0,shown=0;
    while(std::chrono::steady_clock::now()-begin<std::chrono::seconds(seconds)){
        bool moving=false;float values[8][16]{};
        for(size_t c=0;c<channels.size();c++){auto& x=channels[c];if(pread(fd,values[c],x.count*4,x.address)!=x.count*4){x.bad++;continue;}
            for(int j=0;j<x.count;j++){float v=values[c][j];if(!std::isfinite(v)){x.bad++;continue;}x.low[j]=std::min(x.low[j],v);x.high[j]=std::max(x.high[j],v);x.maxAbs[j]=std::max(x.maxAbs[j],std::abs(v));if(v!=0){x.nonzero[j]++;if(c<2&&j<2)moving=true;}}
        }
        if(moving&&shown<12){shown++;auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();std::printf("sample ms=%lld",(long long)ms);for(size_t c=0;c<channels.size();c++)std::printf(" channel%zu=(%.6g,%.6g)",c,values[c][0],values[c][1]);std::puts("");std::fflush(stdout);}
        samples++;usleep(1000);
    }
    for(size_t c=0;c<channels.size();c++){const auto& x=channels[c];std::printf("channel%zu address=0x%llx samples=%u errors=%u\n",c,(unsigned long long)x.address,samples,x.bad);for(int j=0;j<x.count;j++)std::printf(" [%d] min=%.8g max=%.8g abs=%.8g nonzero=%u\n",j,x.low[j],x.high[j],x.maxAbs[j],x.nonzero[j]);}
    close(fd);return 0;
}
