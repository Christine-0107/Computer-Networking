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
const int MSIZE = 1024; //�ܳ���
const int NSIZE = 20; //���ֳ���
const int GSIZE = 986;
enum LOG { INFO, NAME, SUCC, SEND, RECV, ERRO, GETI};
char message[MSIZE];
char getMessage[GSIZE];
char name[NSIZE];
char name2[NSIZE];
char mode;
char getInput[GSIZE];
char cstr[MSIZE];
DWORD WINAPI handleRequest(LPVOID lparam); //������Ϣ�����뷢�͵��̺߳���
void printTime(); //��ȡ��ǰʱ��
void printLog(LOG log, string str); //��ӡ��־
int changeToPrivate(SOCKET clientSocket);
int changeToPublic(SOCKET clientSocket);
//����4����Ϣͷָʾ��ͬ��Ϣ���ͣ�NAME  MODE  NORM  EXIT



int main()
{
	WORD wVersionRequested = MAKEWORD(2, 2); //(low, high)low��Ҫ�汾�ţ�high��Ҫ�汾��
	WSADATA wsaData; //��ȡ�汾��Ϣ��˵��Ҫʹ�õİ汾
	SOCKET clientSocket; //�ͻ���socket
	SOCKADDR_IN serverAddrIn; //����������˵�ַ�Ͷ˿ں�
	int result = 0; //�����ú����󷵻صĽ���Ƿ���ȷ

	

	//1. ��ʼ��Socket��
	result=WSAStartup(wVersionRequested, &wsaData);
	if (result != 0) {
		printLog(ERRO, "WSASTARTUP Error! ");
		return -1;
	}
	printLog(SUCC, "WSASTARTUP Completed!");

	//2. ����Socket
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		printLog(ERRO, "Client Socket Invalid Error! ");
		return -1;
	}
	printLog(SUCC, "Client Socket Created! ");

	//3. ���ӷ�����
	serverAddrIn.sin_family = AF_INET; //��ַ����af
	serverAddrIn.sin_port = htons(PORT); //�˿ںţ�ע���ֽ���ת��
	serverAddrIn.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //IP��ַ
	result = connect(clientSocket, (SOCKADDR*)&serverAddrIn, sizeof(SOCKADDR));
	if (result == SOCKET_ERROR) {
		printLog(ERRO, "Connect Fail! ");
		return -1;
	}
	printLog(SUCC, "Connect Succeed! ");

	//4. ������Ϣ����Ҫ�������߳�
	HANDLE hThread = CreateThread(NULL, 0, &handleRequest, LPVOID(clientSocket), 0, NULL);
	if (hThread == NULL) {
		printLog(ERRO, "Create Thread Fail! ");
		return -1;
	}
	printLog(SUCC, "Create Thread Succeed! ");

	//5. ������Ϣ
	// �ȷ����û�������Ϊ��������
	printLog(NAME, "Enter your name, no more than 20 characters: ");
	strcat(message, "NAME"); //��Ϣ����ΪNAME����ʽΪ��NAME+ʱ��+"name"
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
	// ѡ���������/Ⱥ��
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
	
	//����������Ϣ
	while (1)
	{
		memset(message, 0, MSIZE);
		memset(getMessage, 0, GSIZE);
		memset(getInput, 0, GSIZE);
		cin.getline(getMessage, sizeof(getMessage));
		if (!strcmp(getMessage, "EXIT")) { //�����˳�����
			while (1) {
				printLog(GETI, "Are you sure to exit (y/n) ? ");
				if (getInput[0] == 'y') { //ȷ���˳��������������
					strcpy(message, getMessage); //��Ϣ����EXIT����ʽ��EXIT+"hh:mm:ss"
					char tmp[10];
					time_t t = time(NULL); //��ȡ��ǰʱ��
					tm* curr_tm = localtime(&t); //ʹ��t�����curr_tm�ṹ
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
				else if (getInput[0] == 'n') { //ȡ���˳����������������
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
					time_t t = time(NULL); //��ȡ��ǰʱ��
					tm* curr_tm = localtime(&t); //ʹ��t�����curr_tm�ṹ
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
			strcat(message, "NORM"); //��ͨ��Ϣ����ʽΪ��NORM+ʱ��+name+[]: +����
			char tmp[10];
			time_t t = time(NULL); //��ȡ��ǰʱ��
			tm* curr_tm = localtime(&t); //ʹ��t�����curr_tm�ṹ
			int hh = curr_tm->tm_hour;
			int mm = curr_tm->tm_min;
			int ss = curr_tm->tm_sec;
			sprintf(tmp, "%02d:%02d:%02d", hh, mm, ss);
			strcat(message, tmp);
			strcat(message, name);
			if (mode == 'a') {
				strcat(message, " [˽��]: ");
			}
			else {
				strcat(message, " [Ⱥ��]: ");
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
				strcat(cstr, " [˽��]: ");
			}
			else {
				strcat(cstr, " [Ⱥ��]: ");
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
		string recvFlag = strBuf.substr(0, 4); //��ȡ��Ϣͷ
		string recvTime = strBuf.substr(4, 8); //��ȡʱ����Ϣ
		string recvMess = strBuf.substr(12, strBuf.size() - 12); //��ȡ��Ϣ����

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
	time_t t = time(NULL); //��ȡ��ǰʱ��
	tm* curr_tm = localtime(&t); //ʹ��t�����curr_tm�ṹ
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
	strcat(message, "MODE"); //��Ϣ����ΪMODE, ��ʽΪMODE+"hh:mm:ss"+a+"name2"����ʾ��name2˽��
	char tmp[10];
	time_t t = time(NULL); //��ȡ��ǰʱ��
	tm* curr_tm = localtime(&t); //ʹ��t�����curr_tm�ṹ
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
	strcat(message, "MODE"); //��Ϣ����ΪMODE����ʽΪMODE+"hh:mm:ss"+b����ʾȺ��
	char tmp[10];
	time_t t = time(NULL); //��ȡ��ǰʱ��
	tm* curr_tm = localtime(&t); //ʹ��t�����curr_tm�ṹ
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