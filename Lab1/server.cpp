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

DWORD WINAPI handleRequest(LPVOID lparam); //������Ϣ�����뷢�͵��̺߳���
void printTime(); //��ȡ��ǰʱ��
void printLog(LOG log, string str); //��ӡ��־


int main()
{
	WORD wVersionRequested = MAKEWORD(2, 2); //(low, high)low��Ҫ�汾�ţ�high��Ҫ�汾��
	WSADATA wsaData; //��ȡ�汾��Ϣ��˵��Ҫʹ�õİ汾
	SOCKET serverSocket; //����������socket
	SOCKADDR_IN serverAddrIn; //�����������ַ�Ͷ˿ں�
	SOCKADDR_IN clientAddrIn; //����ͻ��˵�ַ�Ͷ˿ں�
	int clientAddrLen=sizeof(clientAddrIn); //�ͻ��˵�ַ����
	int result = 0; //�����ú����󷵻صĽ���Ƿ���ȷ

	printLog(INFO, "Server Started! ");
	//1. ��ʼ��Socket��
	result=WSAStartup(wVersionRequested, &wsaData);
	if (result != 0) {
		printLog(ERRO, "WSASTARTUP Error! ");
		return -1;
	}
	printLog(SUCC, "WSASTARTUP Completed!");

	//2. ������Socket
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET) {
		printLog(ERRO, "Server Socket Invalid Error! ");
		return -1;
	}
	printLog(SUCC, "Server Socket Created! ");

	//3. ��socket�͵�ַ
	serverAddrIn.sin_family = AF_INET; //��ַ����af
	serverAddrIn.sin_port = htons(PORT); //�˿ںţ�ע���ֽ���ת��
	serverAddrIn.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //IP��ַ
	result = bind(serverSocket, (SOCKADDR*)&serverAddrIn, sizeof(struct sockaddr));
	if (result == SOCKET_ERROR) {
		printLog(ERRO, "Bind Fail! ");
		return -1;
	}
	printLog(SUCC, "Bind Succeed! ");
	
	//4. ʹsocket�������״̬������Զ�������Ƿ���
	result = listen(serverSocket, NUM);
	if (result == SOCKET_ERROR) {
		printLog(ERRO, "Listen Fail! ");
		return -1;
	}
	printLog(SUCC, "Listen... ");

	//5. ������Ϣ����������Ҫ���߳�
	while (cnum<=NUM) {
		SOCKET sockConn = accept(serverSocket, (SOCKADDR*)&clientAddrIn, &clientAddrLen); //����������ͻ���ͨ�ŵ�socket
		if (sockConn == INVALID_SOCKET)
		{
			printLog(ERRO, "Accept Fail! ");
			break;
		}
		cnum++;
		printLog(SUCC, "Accept Succeed! ");
		for (int i = 0; i < CSIZE; i++) { //�����ɵ���ͻ���ͨ�ŵ�socket��ӵ������б���
			if (clientArray[i] == NULL) {
				clientArray[i] = sockConn;
				clientMap[sockConn] = i;
				string str = to_string((long long)sockConn);
				str += " joins.";
				printLog(JOIN, str);
				break;
			}
		}
		HANDLE hThread = CreateThread(NULL, 0, &handleRequest, LPVOID(sockConn), 0, NULL); //�����̣߳���������ͻ���ͨ��
		if (hThread == NULL) {
			printLog(ERRO, "Create Thread Fail! ");
			break;
		}
		printLog(SUCC, "Create Thread Succeed! ");
		CloseHandle(hThread);
	}
	printLog(ERRO, "Client Number Is Full! ");
	//7. �ر�socket���ͷ���Դ
	closesocket(serverSocket);
	WSACleanup();
	return 0;
}

// 6.�����������Ϣ
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
		string recvFlag = strBuf.substr(0, 4); //��ȡ��Ϣͷ
		string recvTime = strBuf.substr(4, 8); //��ȡʱ����Ϣ
		string recvMess = strBuf.substr(12, strBuf.size() - 12); //��ȡ��Ϣ����
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
			char mode = recvMess[0]; //��ȡprivate/public
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
						time_t t = time(NULL); //��ȡ��ǰʱ��
						tm* curr_tm = localtime(&t); //ʹ��t�����curr_tm�ṹ
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
					time_t t = time(NULL); //��ȡ��ǰʱ��
					tm* curr_tm = localtime(&t); //ʹ��t�����curr_tm�ṹ
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
						time_t t = time(NULL); //��ȡ��ǰʱ��
						tm* curr_tm = localtime(&t); //ʹ��t�����curr_tm�ṹ
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
	time_t t = time(NULL); //��ȡ��ǰʱ��
	tm* curr_tm = localtime(&t); //ʹ��t�����curr_tm�ṹ
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