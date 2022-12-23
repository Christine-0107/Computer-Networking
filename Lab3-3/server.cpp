#include <iostream>
#include <winsock2.h>
#include <time.h>
#include <string>
#include <vector>
#include <fstream>
#include <io.h>
#include <Windows.h>
#include "head.h"
#include <thread>
#pragma comment(lib,"ws2_32.lib") //��̬������ļ����ṩ�������API֧��
#pragma warning(disable:4996) //����warning����߼�����
using namespace std;

SOCKET serverSocket; //��������socket
SOCKADDR_IN serverAddrIn; //����������˵�ַ�Ͷ˿ں�
SOCKADDR_IN clientAddrIn; //����ͻ��˵�ַ�Ͷ˿ں�
const int serverPort = 8888;
const int clientPort = 9999;
int curseq = 0; //��ǰ���к�
u_long non_block = 1; //���÷�����
std::string fileDir = "./test";

struct Pseudoheader sendPheader {};
struct Pseudoheader recvPheader {};

int expectedseqnum = 0;
bool waitToExit = false;
int exitTime = 0;

int receiveMesg();
int sendSYNACK(int ack);
int sendFINACK(int ack);
int sendACK(int ack);
void waitExit();

int main()
{
	WORD wVersionRequested = MAKEWORD(2, 2); //(low, high)low��Ҫ�汾�ţ�high��Ҫ�汾��
	WSADATA wsaData; //��ȡ�汾��Ϣ��˵��Ҫʹ�õİ汾
	int result = 0; //�����ú����󷵻صĽ���Ƿ���ȷ

	//1. ��ʼ��Socket��
	result = WSAStartup(wVersionRequested, &wsaData);
	if (result != 0) {
		std::cout << "WSASTARTUP Error! " << std::endl;
		return -1;
	}
	std::cout << "WSASTARTUP Success! " << std::endl;

	//2. ����Socket
	serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (serverSocket == INVALID_SOCKET) {
		std::cout << "Server Socket Invalid Error! " << std::endl;
		return -1;
	}
	std::cout << "Server Socket Created! " << std::endl;

	//3. ���ӿͻ���
	serverAddrIn.sin_family = AF_INET; //��ַ����af
	serverAddrIn.sin_port = htons(serverPort); //�˿ںţ�ע���ֽ���ת��
	serverAddrIn.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //IP��ַ

	clientAddrIn.sin_family = AF_INET; //��ַ����af
	clientAddrIn.sin_port = htons(clientPort); //�˿ںţ�ע���ֽ���ת��
	clientAddrIn.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //IP��ַ

	// bind
	if (bind(serverSocket, (SOCKADDR*)&serverAddrIn, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		std::cout << "Bind Failed! " << std::endl;
		return -1;
	}
	else {
		std::cout << "Bind Success!" << std::endl;
	}

	//4. ��ʼ��α�ײ�
	sendPheader.sourceIP = serverAddrIn.sin_addr.S_un.S_addr;
	sendPheader.destIP = clientAddrIn.sin_addr.S_un.S_addr;
	recvPheader.sourceIP = clientAddrIn.sin_addr.S_un.S_addr;
	recvPheader.destIP = serverAddrIn.sin_addr.S_un.S_addr;

	//5. ��ʼ������Ϣ
	receiveMesg();
}

int receiveMesg()
{
	Message recvMesg{};
	fstream file;
	unsigned int fileSize;
	string fileName;
	while(true)
	{
		int serverLength = sizeof(SOCKADDR);
		int recvLength = recvfrom(serverSocket, (char*)&recvMesg, sizeof(struct Message), 0, (struct sockaddr*)&clientAddrIn, &serverLength);
		if (recvLength > 0) {
			Message_C recv_C(&recvMesg);
			cout << "Receive Message:" << endl;
			printMessage(recv_C);
			//У�����ȷ�����к���ȷ
			recv_C.fillData();
			if (recv_C.isCk(&recvPheader) && recvMesg.seq == expectedseqnum) {
				expectedseqnum++;
				//��ϢΪSYN
				if (recv_C.isSYN()) {
					int result=sendSYNACK(recvMesg.seq);
					if (result != -1) {
						expectedseqnum = 0;
					}
				}
				//��ϢΪFIN
				else if (recv_C.isFIN()) {
					int result=sendFINACK(recvMesg.seq);
					exitTime = 0;
					waitExit();
					/*if (result != -1) {
						closesocket(serverSocket);
						break;
					}*/
				}
				//���ļ���socket
				else {
					//���ļ�ͷ��ʶ����Ϣ�����ļ�
					if (recv_C.isSF()) {
						File_C fileC {};
						memcpy(&fileC, recvMesg.data, sizeof(struct File_C));
						cout << "[Receive file header]: Name:" << fileC.name << " Size:" << fileC.size << endl;
						fileSize = fileC.size;
						fileName = fileC.name;
						file.open(fileName, ios::out | ios::binary);
						//����ACK
						//Sleep(1);
						sendACK(recvMesg.seq);
					}
					//�����ļ�ͷ�����Ӧ�ļ���д
					else {
						file.write(recvMesg.data, recv_C.getLen());
						if (recv_C.isEF()) { //���ļ�β
							cout << "[SUCC] : File " << fileName << "Receive Success! " << endl;
							file.close();
							//Sleep(1);
							int result= sendACK(recvMesg.seq);
							if (result != -1) {
								expectedseqnum = 0;
							}
						}
						else {
							//����ACK
							cout << "recv.seq=" << recvMesg.seq << endl;
							//Sleep(1);
							sendACK(recvMesg.seq);
						}
						
					}
				}
			}
			//У��ʹ�������кŴ���
			else {
				if (recv_C.isSYN()) {
					sendSYNACK(expectedseqnum-1);
				}
				else if(recv_C.isFIN()){
					sendFINACK(expectedseqnum - 1);
				}
				else {
					sendACK(expectedseqnum - 1);
				}
			}
		}
	}
	return 0;
}

int sendSYNACK(int ack) {
	Message* message_SA;
	message_SA = new Message();
	message_SA->source_port = serverPort;
	message_SA->dest_port = clientPort;
	message_SA->seq = expectedseqnum;
	message_SA->ack = ack;
	Message_C message_SAC(message_SA);
	message_SAC.setACK();
	message_SAC.setSYN();
	message_SAC.setLen(0);
	message_SAC.setcksum(&sendPheader);
	Message* message = message_SAC.message;
	cout << "Send SYN_ACK:" << endl;
	printMessage(message_SAC);
	int result = -1;
	if (!isLoss()) {
		result = sendto(serverSocket, (char*)message, sizeof(struct Message), 0, (struct sockaddr*)&clientAddrIn, sizeof(SOCKADDR));
	}
	if (result == -1) {
		cout << "Send SYN_ACK Failed! " << endl;
		return -1;
	}
	cout << "Send SYN_ACK Success! " << endl;
	return 0;
}

int sendFINACK(int ack) {
	Message* message_FA;
	message_FA = new Message();
	message_FA->source_port = serverPort;
	message_FA->dest_port = clientPort;
	message_FA->seq = expectedseqnum;
	message_FA->ack = ack;
	Message_C message_FAC(message_FA);
	message_FAC.setACK();
	message_FAC.setFIN();
	message_FAC.setLen(0);
	message_FAC.setcksum(&sendPheader);
	Message* message = message_FAC.message;
	cout << "Send FIN_ACK:" << endl;
	printMessage(message_FAC);
	int result = -1;
	if (!isLoss()) {
		result = sendto(serverSocket, (char*)message, sizeof(struct Message), 0, (struct sockaddr*)&clientAddrIn, sizeof(SOCKADDR));
	}
	if (result == -1) {
		cout << "Send FIN_ACK Failed! " << endl;
		return -1;
	}
	cout << "Send FIN_ACK Success! " << endl;
	return 0;
}

int sendACK(int ack) {
	Message* message_A;
	message_A = new Message();
	message_A->source_port = serverPort;
	message_A->dest_port = clientPort;
	message_A->seq = expectedseqnum;
	cout << "ack=" << ack << endl;
	message_A->ack = ack;
	Message_C message_AC(message_A);
	message_AC.setACK();
	message_AC.setLen(0);
	message_AC.setcksum(&sendPheader);
	Message* message = message_AC.message;
	cout << "Send ACK:" << endl;
	printMessage(message_AC);
	int result = -1;
	if (!isLoss()) {
		result = sendto(serverSocket, (char*)message, sizeof(struct Message), 0, (struct sockaddr*)&clientAddrIn, sizeof(SOCKADDR));
	}
	if (result == -1) {
		cout << "Send ACK Failed! " << endl;
		return -1;
	}
	cout << "Send ACK Success! " << endl;
	return 0;
}

void waitExit() {
	if (!waitToExit) {
		waitToExit = true;
		std::thread thread([&]() {
			while (true) {
				Sleep(1);
				exitTime++;
				if (exitTime >= 3 * 50) {
					// cleanup resources
					closesocket(serverSocket);
					WSACleanup();
					exit(0);
				}
			}
			});
		thread.detach();
	}
}