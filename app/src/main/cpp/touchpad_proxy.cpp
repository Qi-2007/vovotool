#include "protocol.hpp"
#include <arpa/inet.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>

namespace {
int64_t nowMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Fd{int value=-1;~Fd(){if(value>=0)close(value);}};
struct Slot{int id=-1,x=0,y=0;bool haveX=false,haveY=false;};
struct Proxy {
    Fd input,output,network;bool created=false,enabled=false,touch=false,hadClick=false;
    int selected=0,previousSlot=-1,previousId=-1,previousX=0,previousY=0;
    unsigned physical=0,sent=0;float gain=0.25f;double remainderX=0,remainderY=0;
    int64_t downAt=0,tapUntil=0;long travel=0;std::array<Slot,4> slots{};
    ~Proxy(){if(created){setButtons(0);ioctl(output.value,UI_DEV_DESTROY);}}
    bool emit(int type,int code,int value){input_event event{};event.type=type;event.code=code;event.value=value;return write(output.value,&event,sizeof(event))==sizeof(event);}
    void sync(){emit(EV_SYN,SYN_REPORT,0);}
    void setButtons(unsigned mask){for(unsigned i=0;i<3;i++)if((mask^sent)&(1u<<i))emit(EV_KEY,BTN_LEFT+i,(mask>>i)&1);if(mask!=sent){sent=mask;sync();}}
    void setEnabled(bool value){if(enabled==value)return;enabled=value;previousSlot=-1;tapUntil=0;remainderX=remainderY=0;if(!value)setButtons(0);}
    bool setup(){
        for(int n=0;n<256;n++){
            char path[64],name[128]{};std::snprintf(path,sizeof(path),"/dev/input/event%d",n);
            int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(fd<0)continue;
            if(ioctl(fd,EVIOCGNAME(sizeof(name)),name)>=0&&std::strcmp(name,"Lenovo Keyboard Pack For Yoga Tab Touchpad")==0){input.value=fd;break;}close(fd);
        }
        if(input.value<0){std::fprintf(stderr,"No supported Lenovo touchpad found\n");return false;}
        output.value=open("/dev/uinput",O_WRONLY|O_NONBLOCK|O_CLOEXEC);
        if(output.value<0){std::perror("uinput open");return false;}
        if(ioctl(output.value,UI_SET_EVBIT,EV_KEY)||ioctl(output.value,UI_SET_EVBIT,EV_REL))return false;
        for(int k:{BTN_LEFT,BTN_RIGHT,BTN_MIDDLE})if(ioctl(output.value,UI_SET_KEYBIT,k))return false;
        for(int a:{REL_X,REL_Y,REL_WHEEL,REL_HWHEEL})if(ioctl(output.value,UI_SET_RELBIT,a))return false;
        uinput_setup setup{};setup.id.bustype=BUS_VIRTUAL;setup.id.version=1;
        std::strcpy(setup.name,"vovotool relative touchpad");
        if(ioctl(output.value,UI_DEV_SETUP,&setup)||ioctl(output.value,UI_DEV_CREATE))return false;created=true;
        network.value=socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(network.value<0||bind(network.value,reinterpret_cast<sockaddr*>(&address),sizeof(address)))return false;
        socklen_t length=sizeof(address);if(getsockname(network.value,reinterpret_cast<sockaddr*>(&address),&length))return false;
        std::printf("READY %d\n",ntohs(address.sin_port));std::fflush(stdout);return true;
    }
    void frame(){
        if(!enabled){previousSlot=-1;return;}
        int count=0,index=-1;for(unsigned n=0;n<slots.size();n++)if(slots[n].id>=0&&slots[n].haveX&&slots[n].haveY){count++;index=n;}
        if(touch&&count==1){const Slot& p=slots[index];
            if(previousSlot==index&&previousId==p.id){
                const int x=p.x-previousX,y=p.y-previousY;travel+=std::abs(x)+std::abs(y);
                if(std::abs(x)<800&&std::abs(y)<800){
                    remainderX+=x*gain;remainderY+=y*gain;const int outX=std::trunc(remainderX),outY=std::trunc(remainderY);
                    remainderX-=outX;remainderY-=outY;
                    if(outX||outY){emit(EV_REL,REL_X,outX);emit(EV_REL,REL_Y,outY);sync();}
                }
            }
            previousSlot=index;previousId=p.id;previousX=p.x;previousY=p.y;
        }else previousSlot=-1;
        setButtons(physical|(nowMs()<tapUntil?1:0));
    }
    void event(const input_event& event){
        int code=event.code,value=event.value;
        if(event.type==EV_ABS){
            if(code==ABS_MT_SLOT){if(value>=0&&value<4)selected=value;return;}
            Slot& p=slots[selected];
            if(code==ABS_MT_TRACKING_ID){p.id=value;if(value<0)p.haveX=p.haveY=false;}
            if(code==ABS_MT_POSITION_X){p.x=value;p.haveX=true;}
            if(code==ABS_MT_POSITION_Y){p.y=value;p.haveY=true;}
        }else if(event.type==EV_KEY){
            if(code==BTN_TOUCH){
                if(value&&!touch){downAt=nowMs();travel=0;hadClick=false;}
                if(!value&&touch&&enabled&&!hadClick&&nowMs()-downAt<200&&travel<6)tapUntil=nowMs()+70;
                touch=value!=0;if(!touch)previousSlot=-1;
            }
            if(code>=BTN_LEFT&&code<=BTN_MIDDLE){unsigned bit=1u<<(code-BTN_LEFT);if(value){physical|=bit;hadClick=true;}else physical&=~bit;}
        }else if(event.type==EV_SYN){
            if(code==SYN_REPORT)frame();
            if(code==SYN_DROPPED){setEnabled(false);for(auto& p:slots)p=Slot{};touch=false;}
        }
    }
};
}
int main(int argc,char** argv){
    if(getuid()!=0){std::fprintf(stderr,"Root required\n");return 1;}
    if(argc==2&&std::strcmp(argv[1],"--self-test")==0){Proxy p;if(!p.setup())return 2;std::puts("SELFTEST OK");return 0;}
    if(argc!=3||std::strcmp(argv[1],"--token")||std::strlen(argv[2])!=32){std::fprintf(stderr,"Usage: proxy --token <32 hex characters>\n");return 2;}
    uint8_t token[16];for(unsigned n=0;n<16;n++){char piece[3]={argv[2][2*n],argv[2][2*n+1],0};char* end;unsigned long value=std::strtoul(piece,&end,16);if(end!=piece+2)return 2;token[n]=value;}
    Proxy p;if(!p.setup())return 3;
    const int64_t startup=nowMs();int64_t lastHeartbeat=startup;bool connected=false;
    while(true){
        pollfd fds[2]={{p.input.value,POLLIN,0},{p.network.value,POLLIN,0}};
        if(poll(fds,2,8)<0&&errno!=EINTR)break;
        if(fds[1].revents&POLLIN){
            for(int n=0;n<16;n++){ProxyPacket packet;ssize_t length=recv(p.network.value,&packet,sizeof(packet),MSG_DONTWAIT);
                if(length<0)break;if(length!=sizeof(packet)||packet.magic!=0x47495043||std::memcmp(packet.token,token,16))continue;
                if(packet.enabled==2)return 0;
                if(packet.enabled>1)continue;
                connected=true;lastHeartbeat=nowMs();
                if(std::isfinite(packet.gain)&&packet.gain>=0.05f&&packet.gain<=2)p.gain=packet.gain;
                p.setEnabled(packet.enabled==1);
            }
        }
        if(fds[0].revents&POLLIN){
            for(int n=0;n<4;n++){input_event events[256];ssize_t length=read(p.input.value,events,sizeof(events));
                if(length<0){if(errno!=EAGAIN&&errno!=EINTR)return 4;break;}if(length==0)return 4;
                if(length%sizeof(input_event))return 4;
                for(size_t i=0;i<static_cast<size_t>(length)/sizeof(input_event);i++)p.event(events[i]);
            }
        }
        if(fds[0].revents&(POLLERR|POLLHUP|POLLNVAL))return 4;
        if(p.enabled&&p.tapUntil&&nowMs()>=p.tapUntil){p.tapUntil=0;p.setButtons(p.physical);}
        if(connected&&nowMs()-lastHeartbeat>1000)p.setEnabled(false);
        if((connected&&nowMs()-lastHeartbeat>5000)||(!connected&&nowMs()-startup>60000))break;
    }
    return 0;
}
