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
const int PORT = 9999;
const int CSIZE = 1024;
const int NUM = 10;
int cnum = 0;
SOCKET clientArray[NUM];
map<SOCKET, int> clientMap;
map<SOCKET, int>::iterator it_m;
enum LOG{INFO, SUCC, JOIN, SEND, RECV, EXIT, ERRO};
char content[CSIZE]; 
map<string, string> names;
map<SOCKET, string> nameMap;
map<SOCKET, string>::iterator it_n;
map<string, char> modeMap;
map<string, char>::iterator it_mo;

DWORD WINAPI handleRequest(LPVOID lparam); //处理消息接受与发送的线程函数
void printTime(); //获取当前时间
void printLog(LOG log, string str); //打印日志


int main()
{
	WORD wVersionRequested = MAKEWORD(2, 2); //(low, high)low主要版本号，high次要版本号
	WSADATA wsaData; //获取版本信息，说明要使用的版本
	SOCKET serverSocket; //服务器端主socket
	SOCKADDR_IN serverAddrIn; //储存服务器地址和端口号
	SOCKADDR_IN clientAddrIn; //储存客户端地址和端口号
	int clientAddrLen=sizeof(clientAddrIn); //客户端地址长度
	int result = 0; //看调用函数后返回的结果是否正确

	printLog(INFO, "Server Started! ");
	//1. 初始化Socket库
	result=WSAStartup(wVersionRequested, &wsaData);
	if (result != 0) {
		printLog(ERRO, "WSASTARTUP Error! ");
		return -1;
	}
	printLog(SUCC, "WSASTARTUP Completed!");

	//2. 创建主Socket
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET) {
		printLog(ERRO, "Server Socket Invalid Error! ");
		return -1;
	}
	printLog(SUCC, "Server Socket Created! ");

	//3. 绑定socket和地址
	serverAddrIn.sin_family = AF_INET; //地址类型af
	serverAddrIn.sin_port = htons(PORT); //端口号，注意字节序转换
	serverAddrIn.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //IP地址
	result = bind(serverSocket, (SOCKADDR*)&serverAddrIn, sizeof(struct sockaddr));
	if (result == SOCKET_ERROR) {
		printLog(ERRO, "Bind Fail! ");
		return -1;
	}
	printLog(SUCC, "Bind Succeed! ");
	
	//4. 使socket进入监听状态，监听远程连接是否到来
	result = listen(serverSocket, NUM);
	if (result == SOCKET_ERROR) {
		printLog(ERRO, "Listen Fail! ");
		return -1;
	}
	printLog(SUCC, "Listen... ");

	//5. 接收消息，有阻塞，要多线程
	while (cnum<=NUM) {
		SOCKET sockConn = accept(serverSocket, (SOCKADDR*)&clientAddrIn, &clientAddrLen); //返回用来与客户端通信的socket
		if (sockConn == INVALID_SOCKET)
		{
			printLog(ERRO, "Accept Fail! ");
			break;
		}
		cnum++;
		printLog(SUCC, "Accept Succeed! ");
		for (int i = 0; i < CSIZE; i++) { //将生成的与客户端通信的socket添加到数组中保存
			if (clientArray[i] == NULL) {
				clientArray[i] = sockConn;
				clientMap[sockConn] = i;
				string str = to_string((long long)sockConn);
				str += " joins.";
				printLog(JOIN, str);
				break;
			}
		}
		HANDLE hThread = CreateThread(NULL, 0, &handleRequest, LPVOID(sockConn), 0, NULL); //创建线程，处理各个客户端通信
		if (hThread == NULL) {
			printLog(ERRO, "Create Thread Fail! ");
			break;
		}
		printLog(SUCC, "Create Thread Succeed! ");
		CloseHandle(hThread);
	}
	printLog(ERRO, "Client Number Is Full! ");
	//7. 关闭socket，释放资源
	closesocket(serverSocket);
	WSACleanup();
	return 0;
}

// 6.发送与接受消息
DWORD WINAPI handleRequest(LPVOID lparam) {
	SOCKET clientSocket = (SOCKET)(LPVOID)lparam;
	int result = 0;
	char recvBuf[CSIZE];
	while (1) {
		result = recv(clientSocket, recvBuf, CSIZE, 0);
		if (result == SOCKET_ERROR) {
			it_m = clientMap.find(clientSocket);
			clientArray[it_m->second] = NULL;
			string str = to_string((long long)clientSocket);
			str += " Disconnected! ";
			printLog(ERRO, str);
			cnum--;
			break;
		}
		string strBuf = recvBuf;
		string recvFlag = strBuf.substr(0, 4); //获取消息头
		string recvTime = strBuf.substr(4, 8); //获取时间信息
		string recvMess = strBuf.substr(12, strBuf.size() - 12); //获取消息内容
		char recvMessC[CSIZE];
		strcpy(recvMessC, recvMess.c_str());
		if (recvFlag.compare("EXIT")==0) {
			std::cout << recvTime << " ";
			it_m = clientMap.find(clientSocket);
			clientArray[it_m->second] = NULL;
			string str = to_string((long long)clientSocket);
			str += " Exited! ";
			printLog(EXIT, str);
			cnum--;
			break;
		}
		if (recvFlag.compare("NAME")==0) {
			nameMap[clientSocket] = recvMess;
			string str1 = "Receive From ";
			str1 += to_string((long long)clientSocket);
			str1 += ": ";
			str1 += recvMess;
			str1 += " joins, welcome!";
			std::cout << recvTime << " ";
			printLog(RECV, str1);
		}
		if (recvFlag.compare("MODE")==0) {
			char mode = recvMess[0]; //获取private/public
			if (mode == 'a') {
				string name2 = recvMess.substr(1, recvMess.size() - 1);
				names[nameMap[clientSocket]] = name2;
				modeMap[nameMap[clientSocket]] = 'a';
				string str1 = "Receive From ";
				str1 += to_string((long long)clientSocket);
				str1 += ": ";
				str1 += nameMap[clientSocket];
				str1 += " chooses to chat with ";
				str1 += name2;
				std::cout << recvTime << " ";
				printLog(RECV, str1);
			}
			else {
				modeMap[nameMap[clientSocket]] = 'b';
				string str1 = "Receive From ";
				str1 += to_string((long long)clientSocket);
				str1 += ": ";
				str1 += nameMap[clientSocket];
				str1 += " chooses to chat with all ";
				std::cout << recvTime << " ";
				printLog(RECV, str1);
			}
		}
		if (recvFlag.compare("NORM")==0) {
			string str1 = "Receive From ";
			str1 += to_string((long long)clientSocket);
			str1 += ": ";
			str1 += recvMess;
			std::cout << recvTime << " ";
			printLog(RECV, str1);
			if (modeMap[nameMap[clientSocket]] == 'a') {
				string name2 = names[nameMap[clientSocket]];
				int flag = 0;
				for (int i = 0; i < NUM; i++) {
					if (clientArray[i] != NULL && nameMap[clientArray[i]]==name2) {
						char sendTo[CSIZE] = "NORM";
						char tmp[10];
						time_t t = time(NULL); //获取当前时间
						tm* curr_tm = localtime(&t); //使用t来填充curr_tm结构
						int hh = curr_tm->tm_hour;
						int mm = curr_tm->tm_min;
						int ss = curr_tm->tm_sec;
						sprintf(tmp, "%02d:%02d:%02d", hh, mm, ss);
						strcat(sendTo, tmp);
						strcat(sendTo, recvMessC);
						send(clientArray[i], sendTo, CSIZE, 0);
						string str = "Send To ";
						str += to_string((long long)clientArray[i]);
						str += ": ";
						str += recvMess;
						printLog(SEND, str);
						flag = 1;
						break;
					}
				}
				if (flag == 0) {
					char sendBack[CSIZE] = "LOSE";
					char tmp[10];
					time_t t = time(NULL); //获取当前时间
					tm* curr_tm = localtime(&t); //使用t来填充curr_tm结构
					int hh = curr_tm->tm_hour;
					int mm = curr_tm->tm_min;
					int ss = curr_tm->tm_sec;
					sprintf(tmp, "%02d:%02d:%02d", hh, mm, ss);
					strcat(sendBack, tmp);
					strcat(sendBack, name2.c_str());
					strcat(sendBack, " Not Connected");
					send(clientSocket, sendBack, CSIZE, 0);
					string str2 = "Send To ";
					str2 += to_string((long long)clientSocket);
					str2 += ": ";
					str2 += name2;
					str2 += " Not Connected! ";
					printLog(SEND, str2);
				}
				
			}
			else {
				for (int i = 0; i < NUM; i++) {
					it_n = nameMap.find(clientArray[i]);
					if (clientArray[i] != NULL && i != clientMap[clientSocket] && it_n!=nameMap.end()) {
						char sendTo[CSIZE] = "NORM";
						char tmp[10];
						time_t t = time(NULL); //获取当前时间
						tm* curr_tm = localtime(&t); //使用t来填充curr_tm结构
						int hh = curr_tm->tm_hour;
						int mm = curr_tm->tm_min;
						int ss = curr_tm->tm_sec;
						sprintf(tmp, "%02d:%02d:%02d", hh, mm, ss);
						strcat(sendTo, tmp);
						strcat(sendTo, recvMessC);
						send(clientArray[i], sendTo, CSIZE, 0);
						string str = "Send To ";
						str += to_string((long long)clientArray[i]);
						str += ": ";
						str += recvMessC;
						printLog(SEND, str);
					}
				}
			}
		}
	}
	closesocket(clientSocket);
	return 0;
}

void printTime() {
	char tmp[10];
	time_t t = time(NULL); //获取当前时间
	tm* curr_tm = localtime(&t); //使用t来填充curr_tm结构
	int hh = curr_tm->tm_hour;
	int mm = curr_tm->tm_min;
	int ss = curr_tm->tm_sec;
	sprintf(tmp, "%02d:%02d:%02d ", hh, mm, ss);
	std::cout << tmp;
}

void printLog(LOG log, string str) {
	switch (log) {
	case INFO:
		printTime();
		std::cout << "[ INFO ] " << str << endl;
		break;
	case SUCC:
		printTime();
		std::cout << "[ SUCC ] " << str << endl;
		break;
	case JOIN:
		printTime();
		std::cout << "[ JOIN ] " << str << endl;
		break;
	case SEND:
		printTime();
		std::cout << "[ SEND ] " << str << endl;
		break;
	case RECV:
		std::cout << "[ RECV ] " << str << endl;
		break;
	case EXIT:
		std::cout << "[ EXIT ] " << str << endl;
		break;
	case ERRO:
		printTime();
		std::cout << "[ ERRO ] " << str << endl;
		break;
	}
}