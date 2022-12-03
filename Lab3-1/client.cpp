#include <iostream>
#include <winsock2.h>
#include <time.h>
#include <string>
#include <vector>
#include <fstream>
#include <io.h>
#include <Windows.h>
#include "head.h"
#pragma comment(lib,"ws2_32.lib") //��̬������ļ����ṩ�������API֧��
#pragma warning(disable:4996) //����warning����߼�����
using namespace std;

SOCKET clientSocket; //�ͻ���socket
SOCKADDR_IN serverAddrIn; //����������˵�ַ�Ͷ˿ں�
SOCKADDR_IN clientAddrIn; //����ͻ��˵�ַ�Ͷ˿ں�
const int serverPort = 8888;
const int clientPort = 9999;
int curseq = 0; //��ǰ���к�
u_long non_block = 1; //���÷�����
std::string fileDir = "./test";
string name1 = "./test/1.jpg";
string name2 = "./test/2.jpg";
string name3 = "./test/3.jpg";
string name4 = "./test/helloworld.txt";


struct Pseudoheader sendPheader {};
struct Pseudoheader recvPheader {};

int connection();
int sendPackage(Message_C message_C);
int sendFiles();
int sendFile(File_C file);
int disConnection();


int main()
{
	WORD wVersionRequested = MAKEWORD(2, 2); //(low, high)low��Ҫ�汾�ţ�high��Ҫ�汾��
	WSADATA wsaData; //��ȡ�汾��Ϣ��˵��Ҫʹ�õİ汾
	int result = 0; //�����ú����󷵻صĽ���Ƿ���ȷ

	//1. ��ʼ��Socket��
	result = WSAStartup(wVersionRequested, &wsaData);
	if (result != 0) {
		cout << "WSASTARTUP Error! " << endl;
		return -1;
	}
	cout << "WSASTARTUP Success! " << endl;
	
	//2. ����Socket
	clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		cout << "Client Socket Invalid Error! " << endl;
		return -1;
	}
	cout << "Client Socket Created! " << endl;

	//����recv����Ϊ������
	struct timeval timeout;
	timeout.tv_sec = 1;//��
	timeout.tv_usec = 0;//΢��
	if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == -1) {
		cout << "setsockopt failed:";
	}

	//3. ���ӷ�����
	serverAddrIn.sin_family = AF_INET; //��ַ����af
	serverAddrIn.sin_port = htons(serverPort); //�˿ںţ�ע���ֽ���ת��
	serverAddrIn.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //IP��ַ

	clientAddrIn.sin_family = AF_INET; //��ַ����af
	clientAddrIn.sin_port = htons(clientPort); //�˿ںţ�ע���ֽ���ת��
	clientAddrIn.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //IP��ַ

	// bind
	if (bind(clientSocket, (SOCKADDR*)&clientAddrIn, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "Bind Failed! " << endl;
		return -1;
	}
	else {
		cout << "Bind Success!" << endl;
	}

	//4. ��ʼ��α�ײ�
	sendPheader.sourceIP = clientAddrIn.sin_addr.S_un.S_addr;
	sendPheader.destIP = serverAddrIn.sin_addr.S_un.S_addr;
	recvPheader.sourceIP = serverAddrIn.sin_addr.S_un.S_addr;
	recvPheader.destIP = clientAddrIn.sin_addr.S_un.S_addr;

	//5. ����������
	while ((result = connection()) == -1) {
		cout << "Retry connection in 1 seconds" << endl;
		Sleep(1000);
	}

	//6. �����ļ�
	result = sendFiles();

	//7. �Ͽ�����
	while ((result = disConnection()) == -1) {
		cout << "Retry disconnection in 1 seconds" << endl;
		Sleep(1000);
	}
	
	WSACleanup();
	return 0;
}

int connection() {
	cout << "Wait for connection. " << endl;
	Message* message_SYN;
	message_SYN = new Message();
	message_SYN->source_port = clientPort;
	message_SYN->dest_port = serverPort;
	message_SYN->seq = curseq;
	message_SYN->ack = 0;
	Message_C message(message_SYN);
	message.setLen(0);
	message.setSYN(); //SYN��־λ��1
	message.setcksum(&sendPheader);
	int result = sendPackage(message);
	if (result == -1) {
		cout << "[ERROR] : Connection Failed! " << endl;
		return -1;
	}
	cout << "[SUCC] : Connection Success! " << endl;
	return 0;
}

//���Ͱ�������ȷ���ش�
int sendPackage(Message_C message_C) {
	Message* message = message_C.message;
	printMessage(message_C);
	int result = -1;
	//��һ�η�����Ϣ
	if (!isLoss()) {
		result = sendto(clientSocket, (char*)message, sizeof(struct Message), 0, (struct sockaddr*)&serverAddrIn, sizeof(SOCKADDR));
	}
	if (result == -1) {
		cout << "Send Failed! " << endl;
	}
	cout << "Send Success! " << endl;

	clock_t start; //��ʼ��ʱ��
	clock_t end;   //������ʱ��	
	// ������ʱ�����ȴ�ACK
	start = clock();
	int sendCount = 0;
	while (true) {
		Message recvMesg;
		int serverLength = sizeof(SOCKADDR);
		int recvLength = recvfrom(clientSocket, (char*)&recvMesg, sizeof(struct Message), 0, (struct sockaddr*)&serverAddrIn, &serverLength);
		if (recvLength > 0) {
			Message_C recv_C(&recvMesg);
			printMessage(recv_C);
			//���У�����ACK
			//У�����ȷ��ACK��ȷ��˵�����ͳɹ�
			if (recv_C.isCk(&recvPheader) && recvMesg.ack == curseq) {
				end = clock();
				//�������к�
				curseq = (curseq + 1) % 2;
				cout << "[ACK] : Package (SEQ:" << recvMesg.ack << ") send success! " << endl;
				return 0;
			}
			//У��ʹ����ACK���󣬼����ȴ���ֱ����ʱ�ش�
			else {
				end = clock();
				cout << "[RESEND] : Client received failed. Wait for timeout to resend" << endl;
				cout << "Time: " << (end - start) << " ms" << endl;
				if (sendCount > Max_Send_Count) {
					cout << "[ERROR] : Resend too many times! " << endl;
					return -1;
				}
				if ((end - start)/ CLOCKS_PER_SEC > Max_Wait_Time) {
					sendCount++;
					cout << "[Timeout] : Resend Package! " << endl;
					result = -1;
					if (!isLoss()) {
						result = sendto(clientSocket, (char*)message, sizeof(struct Message), 0, (struct sockaddr*)&serverAddrIn, sizeof(SOCKADDR));
					}
					if (result == -1) {
						cout << "Send Failed! " << endl;
					}
					cout << "Send Success! " << endl;
					printMessage(message_C);
					start = clock(); //���ö�ʱ��
				}
			}
		}
		else {
			end = clock();
			cout << "[RESEND] : Client received failed. Wait for timeout to resend" << endl;
			cout << "Time: " << (end - start) << " ms" << endl;
			if (sendCount > Max_Send_Count) {
				cout << "[ERROR] : Resend too many times! " << endl;
				return -1;
			}
			if ((end - start) / CLOCKS_PER_SEC > Max_Wait_Time) {
				sendCount++;
				cout << "[Timeout] : Resend Package! " << endl;
				result = -1;
				if (!isLoss()) {
					result = sendto(clientSocket, (char*)message, sizeof(struct Message), 0, (struct sockaddr*)&serverAddrIn, sizeof(SOCKADDR));
				}
				if (result == -1) {
					cout << "Send Failed! " << endl;
				}
				cout << "Send Success! " << endl;
				printMessage(message_C);
				start = clock(); //���ö�ʱ��
			}
		}
	}
}

//���ļ�
int sendFiles() {
	vector<string> names;
	names.push_back(name1);
	names.push_back(name2);
	names.push_back(name3);
	names.push_back(name4);
	vector<File_C> files;
	for (auto name : names) {
		ifstream infile;
		infile.open(name, std::ios::in);
		infile.seekg(0, std::ios_base::end);
		std::streampos fileSize = infile.tellg();
		infile.seekg(0, std::ios_base::beg);
		infile.close();
		File_C file{};
		strcpy(file.name, name.c_str());
		file.size = fileSize;
		files.push_back(file);
	}
	for (auto file : files) {
		cout << "Send file: " << file.name << " size: " << file.size << " begin! " << endl;
		srand((unsigned)time(NULL));
		long long head, tail, freq;
		QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		QueryPerformanceCounter((LARGE_INTEGER*)&head);
		int result=sendFile(file);
		QueryPerformanceCounter((LARGE_INTEGER*)&tail);
		double duration = (tail - head) * 1000 / freq;
		cout << "Send file: " << file.name << " size: " << file.size << " in " << duration << "ms " << "with throughoutput in " << file.size/duration << "KB/s ! "<<endl;
	}
	return 0;
}
//���͵����ļ�
int sendFile(File_C file) {
	//�ļ���ʼ��Ϣ�������ļ����ʹ�С
	Message* message_SEND;
	message_SEND = new Message();
	message_SEND->source_port = clientPort;
	message_SEND->dest_port = serverPort;
	message_SEND->seq = curseq;
	message_SEND->ack = 0;
	Message_C message_S(message_SEND);
	message_S.setACK();
	message_S.setSF();
	message_S.setLen(sizeof(struct File_C));
	message_S.setData((char*)&file);
	//������Ҫ���ٸ����ݱ����ܷ���һ���ļ�
	int index = ceil(((double)file.size) / DSIZE);
	message_S.setIndex((short)index - 1); //index��0��ʼ
	message_S.setcksum(&sendPheader);
	int result = sendPackage(message_S);
	if (result == -1) {
		cout << "[ERROR] : File " << file.name << " Send Failed! " << endl;
		return -1;
	}
	//�����ļ�����
	ifstream fileStream(file.name, ios::binary | ios::app);
	int len = file.size;
	for (int i = 0; i < index; i++) {
		//��ȡ�ļ�����
		char fileContent[DSIZE];
		fileStream.read(fileContent, fmin(len, DSIZE));
		//�����ļ�����
		Message* message_FILE;
		message_FILE = new Message();
		message_FILE->source_port = clientPort;
		message_FILE->dest_port = serverPort;
		message_FILE->seq = curseq;
		message_FILE->ack = 0;
		Message_C message_F(message_FILE);
		message_F.setACK();
		message_F.setLen(fmin(len, DSIZE));
		message_F.setData(fileContent);
		if (i == message_S.getIndex()) {
			message_F.setEF();
		}
		message_F.setcksum(&sendPheader);
		len -= DSIZE;
		cout << "[FILE INDEX] " << i << " in " << index << endl;
		int result = sendPackage(message_F);
		if (result == -1) {
			cout << "[ERROR] : File " << file.name << "Index " << i << " Send Failed! " << endl;
			return -1;
		}
	}
}

int disConnection() {
	Message* message_FIN;
	message_FIN = new Message();
	message_FIN->source_port = clientPort;
	message_FIN->dest_port = serverPort;
	message_FIN->seq = curseq;
	message_FIN->ack = 0;
	Message_C message(message_FIN);
	message.setLen(0);
	message.setFIN();
	message.setcksum(&sendPheader);
	int result = sendPackage(message);
	if (result == -1) {
		cout << "[ERROR] : Disconnection Failed! " << endl;
		return -1;
	}
	closesocket(clientSocket);
	cout << "[SUCC] : Connection Destroyed! " << endl;
}