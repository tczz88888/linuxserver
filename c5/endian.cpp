
#include <iostream>
#include <netinet/in.h>
using namespace std;
union Test{
    unsigned val;
    char ch[sizeof(short)];
};

int main(){
    Test test;
    test.val=0x01020304;
    unsigned p=0;
    for(int i=3;i>=0;i--){
        p=(p<<8)+test.ch[i];
    }
    cout<<hex<<p<<' '<<test.val<<endl;
    if(p==test.val) cout<<"small endian"<<endl;
    else cout<<"big endian"<<endl;
    p=htonl(test.val);
    // 还有ntohl，l是unsigned int，s是unsigned short
    cout<<hex<<p<<' '<<test.val<<endl;
    if(p==test.val) cout<<"small endian"<<endl;
    else cout<<"big endian"<<endl;
    return 0;
}