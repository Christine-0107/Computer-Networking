#include <iostream>
#include <winsock2.h>
#include <time.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#pragma comment(lib,"ws2_32.lib") //��̬������ļ����ṩ�������API֧��
#pragma warning(disable:4996) //����warning����߼�����
using namespace std;

const int MSIZE = 8192; //���ݱ�����
const int DSIZE = 8172; //��Ϣ���ݳ���
const int Max_Wait_Time = 5; //��ȴ�ʱ��
const int Max_Send_Count = 10; //����ط�����


// ������Ϣ��ʽ
struct Message {
	unsigned short source_port = 0;
	unsigned short dest_port = 0;
	int seq = 0;
	int ack = 0;
	unsigned short lenth = 0;
	unsigned short checksum = 0;
	unsigned short flag = 0;
	unsigned short index = 0; //�����ļ���Ҫ���ٸ����ݱ�����
	char data[DSIZE] = { 0 };
};

//����α�ײ�
struct Pseudoheader
{
	unsigned long sourceIP{};
	unsigned long destIP{};
	char zero = 0;
	char protocal = 17; //udpЭ���
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
		message->flag |= 0x0001; //��0λΪFIN
	}
	bool isFIN() {
		return message->flag & 0x0001;
	}
	void setSYN() {
		message->flag |= 0x0002; //��1λΪSYN
	}
	bool isSYN() {
		return message->flag & 0x0002;
	}
	void setACK() {
		message->flag |= 0x0004; //��2λΪACK
	}
	bool isACK() {
		return message->flag & 0x0004;
	}
	void setREP() {
		message->flag |= 0x0008; //��3λΪREP�����Ͷ˱�ʶ�Ƿ��ش�
	}
	bool isREP() {
		return message->flag & 0x0008;
	}
	void setSF() {
		message->flag |= 0x0010; //��4λΪSF����־�ļ���ʼ
	}
	bool isSF() {
		return message->flag & 0x0010;
	}
	void setEF() {
		message->flag |= 0x0020; //��5λΪEF����־�ļ�����
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


//�����������
bool isLoss() {
	if (rand() % 500 < 1) {
		return true;
	}
	return false;
}

//�����ļ�������
struct File_C {
	char name[40];
	unsigned int size;
};

