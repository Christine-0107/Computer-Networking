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
const int MSIZE = 1024; //总长度
const int NSIZE = 20; //名字长度
const int GSIZE = 986;
enum LOG { INFO, NAME, SUCC, SEND, RECV, ERRO, GETI};
char message[MSIZE];
char getMessage[GSIZE];
char name[NSIZE];
char name2[NSIZE];
char mode;
char getInput[GSIZE];
char cstr[MSIZE];
DWORD WINAPI handleRequest(LPVOID lparam); //处理消息接受与发送的线程函数
void printTime(); //获取当前时间
void printLog(LOG log, string str); //打印日志
int changeToPrivate(SOCKET clientSocket);
int changeToPublic(SOCKET clientSocket);
//设置4种消息头指示不同消息类型，NAME  MODE  NORM  EXIT



int main()
{
	WORD wVersionRequested = MAKEWORD(2, 2); //(low, high)low主要版本号，high次要版本号
	WSADATA wsaData; //获取版本信息，说明要使用的版本
	SOCKET clientSocket; //客户端socket
	SOCKADDR_IN serverAddrIn; //储存服务器端地址和端口号
	int result = 0; //看调用函数后返回的结果是否正确

	

	//1. 初始化Socket库
	result=WSAStartup(wVersionRequested, &wsaData);
	if (result != 0) {
		printLog(ERRO, "WSASTARTUP Error! ");
		return -1;
	}
	printLog(SUCC, "WSASTARTUP Completed!");

	//2. 创建Socket
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		printLog(ERRO, "Client Socket Invalid Error! ");
		return -1;
	}
	printLog(SUCC, "Client Socket Created! ");

	//3. 连接服务器
	serverAddrIn.sin_family = AF_INET; //地址类型af
	serverAddrIn.sin_port = htons(PORT); //端口号，注意字节序转换
	serverAddrIn.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //IP地址
	result = connect(clientSocket, (SOCKADDR*)&serverAddrIn, sizeof(SOCKADDR));
	if (result == SOCKET_ERROR) {
		printLog(ERRO, "Connect Fail! ");
		return -1;
	}
	printLog(SUCC, "Connect Succeed! ");

	//4. 接受消息，需要创建多线程
	HANDLE hThread = CreateThread(NULL, 0, &handleRequest, LPVOID(clientSocket), 0, NULL);
	if (hThread == NULL) {
		printLog(ERRO, "Create Thread Fail! ");
		return -1;
	}
	printLog(SUCC, "Create Thread Succeed! ");

	//5. 发送消息
	// 先发送用户名，作为加入申请
	printLog(NAME, "Enter your name, no more than 20 characters: ");
	strcat(message, "NAME"); //消息类型为NAME，格式为：NAME+时间+"name"
	char tmp[10] = { 0 };
	time_t t = time(0);
	strftime(tmp, sizeof(tmp), "%H:%M:%S", localtime(&t));
	strcat(message, tmp);
	strcat(message, name);
	result = send(clientSocket, message, MSIZE, 0);
	if (result == SOCKET_ERROR)
	{
		printLog(ERRO, "Join Fail! ");
		return -1;
	}
	memset(cstr, 0, MSIZE);
	strcpy(cstr, name);
	strcat(cstr, " joins, welcome! ");
	printLog(SUCC, cstr);
	// 选择聊天对象/群聊
	while (1) {
		memset(getInput, 0, GSIZE);
		printLog(GETI, "Choose Private or Public (PRIVATE/PUBLIC): ");
		if (!strcmp(getInput,"PRIVATE")) {
			result = changeToPrivate(clientSocket);
			if (result == -1) {
				continue;
			}
			break;
		}
		else if (!strcmp(getInput,"PUBLIC")) {
			result = changeToPublic(clientSocket);
			if (result == -1) {
				continue;
			}
			break;
		}
		else {
			printLog(INFO, "Invalid! Input again! ");
		}
	}
	
	//发送聊天消息
	while (1)
	{
		memset(message, 0, MSIZE);
		memset(getMessage, 0, GSIZE);
		memset(getInput, 0, GSIZE);
		cin.getline(getMessage, sizeof(getMessage));
		if (!strcmp(getMessage, "EXIT")) { //输入退出请求
			while (1) {
				printLog(GETI, "Are you sure to exit (y/n) ? ");
				if (getInput[0] == 'y') { //确认退出，向服务器发送
					strcpy(message, getMessage); //消息类型EXIT，格式：EXIT+"hh:mm:ss"
					char tmp[10];
					time_t t = time(NULL); //获取当前时间
					tm* curr_tm = localtime(&t); //使用t来填充curr_tm结构
					int hh = curr_tm->tm_hour;
					int mm = curr_tm->tm_min;
					int ss = curr_tm->tm_sec;
					sprintf(tmp, "%02d:%02d:%02d", hh, mm, ss);
					strcat(message, tmp);
					result = send(clientSocket, message, MSIZE, 0);
					if (result == SOCKET_ERROR)
					{
						printLog(ERRO, "Exit Fail! ");
						continue;
					}
					memset(cstr, 0, MSIZE);
					strcpy(cstr, name);
					strcat(cstr, " Exit");
					printLog(SUCC, cstr);
					return -1;
				}
				else if (getInput[0] == 'n') { //取消退出，不向服务器发送
					printLog(INFO, "NOT EXIT");
					break;
				}
				else {
					printLog(INFO, "Invalid! Input again! ");
				}
			}
		}
		else if (!strcmp(getMessage, "PRIVATE")) {
			if (mode == 'a') {
				memset(getInput, 0, GSIZE);
				printLog(GETI, "Input his or her name, no more than 20 characters: ");
				if (!strcmp(getInput, name2)) {
					printLog(INFO, "You are already at PRIVATE mode with this one now!");
				}
				else {
					char temp[NSIZE];
					strcpy(temp, name2);
					memset(name2, 0, NSIZE);
					strcpy(name2, getInput);
					memset(message, 0, MSIZE);
					strcat(message, "MODE");
					char tmp[10];
					time_t t = time(NULL); //获取当前时间
					tm* curr_tm = localtime(&t); //使用t来填充curr_tm结构
					int hh = curr_tm->tm_hour;
					int mm = curr_tm->tm_min;
					int ss = curr_tm->tm_sec;
					sprintf(tmp, "%02d:%02d:%02d", hh, mm, ss);
					strcat(message, tmp);
					strcat(message, "a");
					strcat(message, name2);
					result = send(clientSocket, message, MSIZE, 0);
					if (result == SOCKET_ERROR)
					{
						printLog(ERRO, "Choose Fail! ");
						strcpy(name2, temp);
						continue;
					}
					memset(cstr, 0, MSIZE);
					strcpy(cstr, name);
					strcat(cstr, ": choose to chat with ");
					strcat(cstr, name2);
					printLog(SUCC, cstr);
				}
			}
			else {
				result = changeToPrivate(clientSocket);
				if (result == -1) {
					memset(name2, 0, NSIZE);
					continue;
				}
			}
		}
		else if (!strcmp(getMessage, "PUBLIC")) {
			if (mode == 'b') {
				printLog(INFO, "You are at PUBLIC mode now!");
			}
			else {
				result = changeToPublic(clientSocket);
				if (result == -1) {
					return -1;
				}
			}
		}
		else {
			memset(message, 0, MSIZE);
			strcat(message, "NORM"); //普通消息，格式为：NORM+时间+name+[]: +内容
			char tmp[10];
			time_t t = time(NULL); //获取当前时间
			tm* curr_tm = localtime(&t); //使用t来填充curr_tm结构
			int hh = curr_tm->tm_hour;
			int mm = curr_tm->tm_min;
			int ss = curr_tm->tm_sec;
			sprintf(tmp, "%02d:%02d:%02d", hh, mm, ss);
			strcat(message, tmp);
			strcat(message, name);
			if (mode == 'a') {
				strcat(message, " [私信]: ");
			}
			else {
				strcat(message, " [群聊]: ");
			}
			strcat(message, getMessage);
			result = send(clientSocket, message, MSIZE, 0);
			if (result == SOCKET_ERROR)
			{
				printLog(ERRO, "Send Fail! ");
				return -1;
			}
			memset(cstr, 0, MSIZE);
			strcpy(cstr, name);
			if (mode == 'a') {
				strcat(cstr, " [私信]: ");
			}
			else {
				strcat(cstr, " [群聊]: ");
			}
			strcat(cstr, getMessage);
			printLog(SEND, cstr);
		}
	}

	CloseHandle(hThread);
	closesocket(clientSocket);
	WSACleanup();
	return 0;
}

DWORD WINAPI handleRequest(LPVOID lparam) {
	SOCKET clientSocket = (SOCKET)(LPVOID)lparam;
	int result = 0;
	char recvBuf[1024];
	while (1) {
		result = recv(clientSocket, recvBuf, MSIZE, 0);
		if (result == SOCKET_ERROR) {
			printLog(ERRO, "Receive Error! ");
			break;
		}
		string strBuf = recvBuf;
		string recvFlag = strBuf.substr(0, 4); //获取消息头
		string recvTime = strBuf.substr(4, 8); //获取时间信息
		string recvMess = strBuf.substr(12, strBuf.size() - 12); //获取消息内容

		if (recvFlag.compare("LOSE")==0) {
			cout << recvTime << " ";
			printLog(RECV, recvMess);
		}
		if (recvFlag.compare("NORM")==0) {
			cout << recvTime << " ";
			printLog(RECV, recvMess);
		}
		
	}
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
	cout << tmp;
}

void printLog(LOG log, string str) {
	switch (log) {
	case INFO:
		printTime();
		cout << "[ INFO ] " << str << endl;
		break;
	case NAME:
		printTime();
		cout << "[ NAME ] " << str ;
		cin.getline(name, sizeof(name));
		break;
	case SUCC:
		printTime();
		cout << "[ SUCC ] " << str << endl;
		break;
	case SEND:
		printTime();
		cout << "[ SEND ] " << str << endl;
		break;
	case RECV:
		cout << "[ RECV ] " << str << endl;
		break;
	case ERRO:
		printTime();
		cout << "[ ERRO ] " << str << endl;
		break;
	case GETI:
		printTime();
		cout << "[ GETI ]" << str;
		cin.getline(getInput, sizeof(getInput));
		break;
	}
}

int changeToPrivate(SOCKET clientSocket) {
	mode = 'a';
	int result = 0;
	memset(getInput, 0, MSIZE);
	printLog(GETI, "Input his/her name, no more than 20 characters: ");
	memset(name2, 0, NSIZE);
	strcpy(name2, getInput);
	memset(message, 0, MSIZE);
	strcat(message, "MODE"); //消息类型为MODE, 格式为MODE+"hh:mm:ss"+a+"name2"，表示和name2私聊
	char tmp[10];
	time_t t = time(NULL); //获取当前时间
	tm* curr_tm = localtime(&t); //使用t来填充curr_tm结构
	int hh = curr_tm->tm_hour;
	int mm = curr_tm->tm_min;
	int ss = curr_tm->tm_sec;
	sprintf(tmp, "%02d:%02d:%02d", hh, mm, ss);
	strcat(message, tmp);
	strcat(message, "a");
	strcat(message, name2);
	result = send(clientSocket, message, MSIZE, 0);
	if (result == SOCKET_ERROR)
	{
		printLog(ERRO, "Choose Fail! ");
		return -1;
	}
	memset(cstr, 0, MSIZE);
	strcpy(cstr, name);
	strcat(cstr, ": choose to chat with ");
	strcat(cstr, name2);
	printLog(SUCC, cstr);
	return 0;
}

int changeToPublic(SOCKET clientSocket) {
	mode = 'b';
	int result = 0;
	memset(message, 0, MSIZE);
	strcat(message, "MODE"); //消息类型为MODE，格式为MODE+"hh:mm:ss"+b，表示群聊
	char tmp[10];
	time_t t = time(NULL); //获取当前时间
	tm* curr_tm = localtime(&t); //使用t来填充curr_tm结构
	int hh = curr_tm->tm_hour;
	int mm = curr_tm->tm_min;
	int ss = curr_tm->tm_sec;
	sprintf(tmp, "%02d:%02d:%02d", hh, mm, ss);
	strcat(message, tmp);
	strcat(message, "b");
	result = send(clientSocket, message, MSIZE, 0);
	if (result == SOCKET_ERROR)
	{
		printLog(ERRO, "Connect Fail! ");
		return -1;
	}
	memset(cstr, 0, MSIZE);
	strcpy(cstr, name);
	strcat(cstr, ": choose to chat with all");
	printLog(SUCC, cstr);
	return 0;
}