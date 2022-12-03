#include <iostream>
#include <winsock2.h>
#include <time.h>
#include <string>
#include <vector>
#include <fstream>
#include <io.h>
#include <Windows.h>
#include "head.h"
#pragma comment(lib,"ws2_32.lib") //静态加入库文件，提供网络相关API支持
#pragma warning(disable:4996) //屏蔽warning，提高兼容性
using namespace std;

SOCKET clientSocket; //客户端socket
SOCKADDR_IN serverAddrIn; //储存服务器端地址和端口号
SOCKADDR_IN clientAddrIn; //储存客户端地址和端口号
const int serverPort = 8888;
const int clientPort = 9999;
int curseq = 0; //当前序列号
u_long non_block = 1; //设置非阻塞
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
	WORD wVersionRequested = MAKEWORD(2, 2); //(low, high)low主要版本号，high次要版本号
	WSADATA wsaData; //获取版本信息，说明要使用的版本
	int result = 0; //看调用函数后返回的结果是否正确

	//1. 初始化Socket库
	result = WSAStartup(wVersionRequested, &wsaData);
	if (result != 0) {
		cout << "WSASTARTUP Error! " << endl;
		return -1;
	}
	cout << "WSASTARTUP Success! " << endl;
	
	//2. 创建Socket
	clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		cout << "Client Socket Invalid Error! " << endl;
		return -1;
	}
	cout << "Client Socket Created! " << endl;

	//设置recv函数为非阻塞
	struct timeval timeout;
	timeout.tv_sec = 1;//秒
	timeout.tv_usec = 0;//微秒
	if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == -1) {
		cout << "setsockopt failed:";
	}

	//3. 连接服务器
	serverAddrIn.sin_family = AF_INET; //地址类型af
	serverAddrIn.sin_port = htons(serverPort); //端口号，注意字节序转换
	serverAddrIn.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //IP地址

	clientAddrIn.sin_family = AF_INET; //地址类型af
	clientAddrIn.sin_port = htons(clientPort); //端口号，注意字节序转换
	clientAddrIn.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //IP地址

	// bind
	if (bind(clientSocket, (SOCKADDR*)&clientAddrIn, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		cout << "Bind Failed! " << endl;
		return -1;
	}
	else {
		cout << "Bind Success!" << endl;
	}

	//4. 初始化伪首部
	sendPheader.sourceIP = clientAddrIn.sin_addr.S_un.S_addr;
	sendPheader.destIP = serverAddrIn.sin_addr.S_un.S_addr;
	recvPheader.sourceIP = serverAddrIn.sin_addr.S_un.S_addr;
	recvPheader.destIP = clientAddrIn.sin_addr.S_un.S_addr;

	//5. 发起建立连接
	while ((result = connection()) == -1) {
		cout << "Retry connection in 1 seconds" << endl;
		Sleep(1000);
	}

	//6. 发送文件
	result = sendFiles();

	//7. 断开连接
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
	message.setSYN(); //SYN标志位置1
	message.setcksum(&sendPheader);
	int result = sendPackage(message);
	if (result == -1) {
		cout << "[ERROR] : Connection Failed! " << endl;
		return -1;
	}
	cout << "[SUCC] : Connection Success! " << endl;
	return 0;
}

//发送包，包含确认重传
int sendPackage(Message_C message_C) {
	Message* message = message_C.message;
	printMessage(message_C);
	int result = -1;
	//第一次发送消息
	if (!isLoss()) {
		result = sendto(clientSocket, (char*)message, sizeof(struct Message), 0, (struct sockaddr*)&serverAddrIn, sizeof(SOCKADDR));
	}
	if (result == -1) {
		cout << "Send Failed! " << endl;
	}
	cout << "Send Success! " << endl;

	clock_t start; //开始计时器
	clock_t end;   //结束计时器	
	// 开启定时器，等待ACK
	start = clock();
	int sendCount = 0;
	while (true) {
		Message recvMesg;
		int serverLength = sizeof(SOCKADDR);
		int recvLength = recvfrom(clientSocket, (char*)&recvMesg, sizeof(struct Message), 0, (struct sockaddr*)&serverAddrIn, &serverLength);
		if (recvLength > 0) {
			Message_C recv_C(&recvMesg);
			printMessage(recv_C);
			//检查校验和与ACK
			//校验和正确且ACK正确，说明发送成功
			if (recv_C.isCk(&recvPheader) && recvMesg.ack == curseq) {
				end = clock();
				//更新序列号
				curseq = (curseq + 1) % 2;
				cout << "[ACK] : Package (SEQ:" << recvMesg.ack << ") send success! " << endl;
				return 0;
			}
			//校验和错误或ACK错误，继续等待，直到超时重传
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
					start = clock(); //重置定时器
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
				start = clock(); //重置定时器
			}
		}
	}
}

//读文件
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
//发送单个文件
int sendFile(File_C file) {
	//文件开始消息，包括文件名和大小
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
	//计算需要多少个数据报才能发完一个文件
	int index = ceil(((double)file.size) / DSIZE);
	message_S.setIndex((short)index - 1); //index从0开始
	message_S.setcksum(&sendPheader);
	int result = sendPackage(message_S);
	if (result == -1) {
		cout << "[ERROR] : File " << file.name << " Send Failed! " << endl;
		return -1;
	}
	//发送文件内容
	ifstream fileStream(file.name, ios::binary | ios::app);
	int len = file.size;
	for (int i = 0; i < index; i++) {
		//读取文件内容
		char fileContent[DSIZE];
		fileStream.read(fileContent, fmin(len, DSIZE));
		//发送文件内容
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