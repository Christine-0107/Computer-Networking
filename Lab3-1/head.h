#include <iostream>
#include <winsock2.h>
#include <time.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#pragma comment(lib,"ws2_32.lib") //静态加入库文件，提供网络相关API支持
#pragma warning(disable:4996) //屏蔽warning，提高兼容性
using namespace std;

const int MSIZE = 8192; //数据报长度
const int DSIZE = 8172; //消息数据长度
const int Max_Wait_Time = 5; //最长等待时间
const int Max_Send_Count = 10; //最多重发次数


// 定义消息格式
struct Message {
	unsigned short source_port = 0;
	unsigned short dest_port = 0;
	int seq = 0;
	int ack = 0;
	unsigned short lenth = 0;
	unsigned short checksum = 0;
	unsigned short flag = 0;
	unsigned short index = 0; //描述文件需要多少个数据报传送
	char data[DSIZE] = { 0 };
};

//定义伪首部
struct Pseudoheader
{
	unsigned long sourceIP{};
	unsigned long destIP{};
	char zero = 0;
	char protocal = 17; //udp协议号
	unsigned short lenth = sizeof(struct Message);
};


class Message_C
{
public:
	Message* message;
	Message_C(Message* mes)
	{
		message = new Message();
		message->source_port = mes->source_port;
		message->dest_port = mes->dest_port;
		message->seq = mes->seq;
		message->ack = mes->ack;
		message->lenth = mes->lenth;
		message->checksum = mes->checksum;
		message->flag = mes->flag;
		message->index = mes->index;
		strcpy(message->data, mes->data);
	}
	void setLen(short len) {
		message->lenth = len & 0xFFFF;
	}
	unsigned short getLen() {
		return message->lenth;
	}
	void setFIN() {
		message->flag |= 0x0001; //第0位为FIN
	}
	bool isFIN() {
		return message->flag & 0x0001;
	}
	void setSYN() {
		message->flag |= 0x0002; //第1位为SYN
	}
	bool isSYN() {
		return message->flag & 0x0002;
	}
	void setACK() {
		message->flag |= 0x0004; //第2位为ACK
	}
	bool isACK() {
		return message->flag & 0x0004;
	}
	void setREP() {
		message->flag |= 0x0008; //第3位为REP，发送端标识是否重传
	}
	bool isREP() {
		return message->flag & 0x0008;
	}
	void setSF() {
		message->flag |= 0x0010; //第4位为SF，标志文件开始
	}
	bool isSF() {
		return message->flag & 0x0010;
	}
	void setEF() {
		message->flag |= 0x0020; //第5位为EF，标志文件结束
	}
	bool isEF() {
		return message->flag & 0x0020;
	}
	int getSeq() {
		return message->seq;
	}
	int getAck() {
		return message->ack;
	}
	unsigned short getCk() {
		return message->checksum;
	}
	void setIndex(short index) {
		message->index = index & 0xFFFF;
	}
	unsigned short getIndex() {
		return message->index;
	}
	void setData(char* data);
	void setcksum(Pseudoheader* pheader);
	bool isCk(Pseudoheader* pheader);
	void fillData();
};


void Message_C::setData(char* data) {
	memset(message->data, 0, DSIZE);
	memcpy(message->data, data, getLen());
}
void Message_C::setcksum(Pseudoheader* pheader) {
	message->checksum = 0;
	unsigned long long sum = 0;
	for (int i = 0; i < sizeof(struct Pseudoheader) / 2; i++) {
		sum += ((unsigned short*)pheader)[i];
	}
	for (int i = 0; i < 10; i++) {
		sum += ((unsigned short*)message)[i];
	}
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	message->checksum = ~sum;
}

bool Message_C::isCk(Pseudoheader* pheader) {
	unsigned long long sum = 0;
	for (int i = 0; i < sizeof(struct Pseudoheader) / 2; i++) {
		sum += ((unsigned short*)pheader)[i];
	}
	for (int i = 0; i < 10; i++) {
		sum += ((unsigned short*)message)[i];
	}
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	return (sum & 0xFFFF) == 0xFFFF;
}

void Message_C::fillData() {
	int len = message->lenth;
	if (len % 2 == 0) {
		return;
	}
	message->data[len] = 0;
}

void printMessage(Message_C message_C) {
	std::cout << "[Package]: "
		<< "(FIN: " << message_C.isFIN() << "), "
		<< "(SYN: " << message_C.isSYN() << "), "
		<< "(ACK: " << message_C.isACK() << "), "
		<< "(SF: " << message_C.isSF() << "), "
		<< "(EF: " << message_C.isEF() << "), "
		<< "(seq: " << message_C.getSeq() << "), "
		<< "(ack: " << message_C.getAck() << "), "
		<< "(len: " << message_C.getLen() << "), "
		<< "(cks: " << message_C.getCk() << ") " << std::endl;
}


//定义随机丢包
bool isLoss() {
	if (rand() % 500 < 1) {
		return true;
	}
	return false;
}

//定义文件描述符
struct File_C {
	char name[40];
	unsigned int size;
};

