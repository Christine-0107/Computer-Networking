#include <iostream>
#include <winsock2.h>
#include <time.h>
#include <string>
#include <vector>
#include <fstream>
#include <io.h>
#include <Windows.h>
#include <thread>
#include "head.h"
#pragma comment(lib,"ws2_32.lib") //��̬������ļ����ṩ�������API֧��
#pragma warning(disable:4996) //����warning����߼�����
using namespace std;

SOCKET clientSocket; //�ͻ���socket
SOCKADDR_IN serverAddrIn; //����������˵�ַ�Ͷ˿ں�
SOCKADDR_IN clientAddrIn; //����ͻ��˵�ַ�Ͷ˿ں�
const int serverPort = 8888;
const int clientPort = 9999;

u_long non_block = 1; //���÷�����
std::string fileDir = "./test";
string name1 = "./test/1.jpg";
string name2 = "./test/2.jpg";
string name3 = "./test/3.jpg";
string name4 = "./test/helloworld.txt";


struct Pseudoheader sendPheader {};
struct Pseudoheader recvPheader {};

//GBN
int N = 10; //���ڴ�С
int base = 0;
int nextseqnum = 0;
int datanum = 0;
Message_C sendBuf[2000]; //���ͻ�����
int curseq = 0;
Timer timer;
int exitFlag = 0;

//RENO
int dupACKcount = 0;
int state = 0; //������0��ӵ������1�����ٻָ�2
int cwnd = 1;
int ssthresh = 16;
int newACKcount = 0;


int connection();
int sendPackage(Message_C message_C);
void sendPackage2();
int sendFiles();
int sendFile(File_C file);
int disConnection();
void beginRecv();
void beginTimeout();


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

	//5. ���������߳�
	beginRecv();

	//6. ������ʱ�ش��߳�
	beginTimeout();

	//7. ����������
	while ((result = connection()) == -1) {
		cout << "Retry connection in 1 seconds" << endl;
		Sleep(1000);
	}

	//8. �����ļ�
	result = sendFiles();

	//9. �Ͽ�����
	while ((result = disConnection()) == -1) {
		cout << "Retry disconnection in 1 seconds" << endl;
		Sleep(1000);
	}
	
	WSACleanup();
	return 0;
}

void beginRecv() {
	std::thread recvThread([&]() {
		while (true) {
			Message recvMesg;
			int serverLength = sizeof(SOCKADDR);
			int recvLength = recvfrom(clientSocket, (char*)&recvMesg, sizeof(struct Message), 0, (struct sockaddr*)&serverAddrIn, &serverLength);
			if (recvLength > 0) {
				Message_C recv_C(&recvMesg);
				cout << "[RECV] : ";
				printMessage(recv_C);
				//���У���
				if (recv_C.isCk(&recvPheader)) {
					if (recv_C.isSYN() && recv_C.isACK()) {
						cout << "[SUCC] : Connection Success! " << endl;
					}
					else if (recv_C.isFIN() && recv_C.isACK()) {
						closesocket(clientSocket);
						cout << "[SUCC] : Connection Destroyed! " << endl;
					}
					else {
						cout << "[ACK] : Package (SEQ:" << recvMesg.ack << ") send success! " << endl;
					}
					if (recv_C.isFIN()) {
						timer.stop();
						exitFlag = 1;
						return;
					}
					//���ݷ��ص�ACK����״̬
					bool newACK = false;
					if (recvMesg.ack + 1 > base) {
						newACK = true;
					}
					//����base
					base = recvMesg.ack + 1 ;
					cout << "[WINDOW] : base = " << base << " nextseqnum = " << nextseqnum << " cwnd=" << cwnd << endl;
					if (base == nextseqnum) {
						timer.stop();
						if (nextseqnum >= datanum) {
							exitFlag = 1;
						}
					}
					else {
						timer.start();
					}
					//����״̬
					switch (state) {
					case 0:
						if (newACK) {
							cwnd++;
							dupACKcount = 0;
							cout << "[RENO] : State=SLOW_STARTUP cwnd=" << cwnd << " ssthresh=" << ssthresh << " dupACKcount=" << dupACKcount << endl;
							if (cwnd >= ssthresh) {
								state = 1;
								cout << "[STATE] : Switch from SLOW_STARTUP to CONGESTION_CONTROL" << endl;
							}
						}
						else {
							dupACKcount++;
							cout << "[RENO] : State=SLOW_STARTUP cwnd=" << cwnd << " ssthresh=" << ssthresh << " dupACKcount=" << dupACKcount << endl;
						}
						if (dupACKcount >= 3) {
							ssthresh = cwnd / 2;
							cwnd = ssthresh + 3;
							cout << "[RENO] : State=SLOW_STARTUP cwnd=" << cwnd << " ssthresh=" << ssthresh << " dupACKcount=" << dupACKcount << endl;
							state = 2;
							cout << "[STATE] : Switch from SLOW_STARTUP to FAST_RECOVER" << endl;
							//�ش����ظ�ack��Ӧ��Ϣ
							Message* message = sendBuf[base].message;
							cout << "[RESEND] : ";
							printMessage(sendBuf[base]);
							if (!isLoss()) {
								sendto(clientSocket, (char*)message, sizeof(struct Message), 0, (struct sockaddr*)&serverAddrIn, sizeof(SOCKADDR));
							}
						}
						break;
					case 1:
						if (newACK) {
							newACKcount++;
							if (newACKcount >= cwnd) {
								cwnd++;
								newACKcount = 0;
							}
							dupACKcount = 0;
							cout << "[RENO] : State=CONGESTION_CONTROL cwnd=" << cwnd << " ssthresh=" << ssthresh << " dupACKcount=" << dupACKcount << endl;
						}
						else {
							dupACKcount++;
							cout << "[RENO] : State=CONGESTION_CONTROL cwnd=" << cwnd << " ssthresh=" << ssthresh << " dupACKcount=" << dupACKcount << endl;
						}
						if (dupACKcount >= 3) {
							ssthresh = cwnd / 2;
							cwnd = ssthresh + 3;
							cout << "[RENO] : State=CONGESTION_CONTROL cwnd=" << cwnd << " ssthresh=" << ssthresh << " dupACKcount=" << dupACKcount << endl;
							state = 2;
							cout << "[STATE] : Switch from CONGESTION_CONTROL to FAST_RECOVER" << endl;
							//�ش����ظ�ack��Ӧ��Ϣ
							Message* message = sendBuf[base].message;
							cout << "[RESEND] : ";
							printMessage(sendBuf[base]);
							if (!isLoss()) {
								sendto(clientSocket, (char*)message, sizeof(struct Message), 0, (struct sockaddr*)&serverAddrIn, sizeof(SOCKADDR));
							}
						}
						break;
					case 2:
						if (newACK) {
							cwnd = ssthresh;
							dupACKcount = 0;
							cout << "[RENO] : State=FAST_RECOVER cwnd=" << cwnd << " ssthresh=" << ssthresh << " dupACKcount=" << dupACKcount << endl;
							state = 1;
							cout << "[STATE] : Switch from FAST_RECOVER to CONGESTION_CONTROL" << endl;
						}
						else {
							cwnd++;
							cout << "[RENO] : State=FAST_RECOVER cwnd=" << cwnd << " ssthresh=" << ssthresh << " dupACKcount=" << dupACKcount << endl;
						}
						break;
					}
				}
				else {
					cout << "[FAILED] : Checksum failed. Wait for timeout to resend" << endl;
				}
			}
			else {
				cout << "[FAILED] : Client received failed. Wait for timeout to resend" << endl;
			}
		}
		});
	recvThread.detach();
}

void beginTimeout() {
	std::thread resendThread([&]() {
		while (true) {
			while(!timer.isTimeout()){}
			if (state == 0) {
				cout << "[STATE] : Still at SLOW_STARTUP" << endl;
			}
			else if (state == 1) {
				cout << "[STATE] : Switch from CONGESTION_CONTROL to SLOW_STARTUP" << endl;
			}
			else if (state == 2) {
				cout << "[STATE] : Switch from FAST_RECOVER to SLOW_STARTUP" << endl;
			}
			ssthresh = cwnd / 2;
			cwnd = 1;
			dupACKcount = 0;
			state = 0;
			cout << "[RENO] : State=SLOW_STARTUP cwnd=" << cwnd << " ssthresh=" << ssthresh << " dupACKcount=" << dupACKcount << endl;
			int i = base;
			do {
				Message* message = sendBuf[i].message;
				cout << "[RESEND] : ";
				printMessage(sendBuf[i]);
				int result = -1;
				if (!isLoss()) {
					result = sendto(clientSocket, (char*)message, sizeof(struct Message), 0, (struct sockaddr*)&serverAddrIn, sizeof(SOCKADDR));
				}
				timer.start();
			} while (++i < nextseqnum);
		}
		});
	resendThread.detach();
}

int connection() {
	cout << "Wait for connection. " << endl;
	base = 0;
	nextseqnum = 0;
	datanum = 0;
	memset(sendBuf, 0, 2000);
	Message* message_SYN;
	message_SYN = new Message();
	message_SYN->source_port = clientPort;
	message_SYN->dest_port = serverPort;
	message_SYN->seq = datanum;
	message_SYN->ack = 0;
	Message_C message(message_SYN);
	message.setLen(0);
	message.setSYN(); //SYN��־λ��1
	message.setcksum(&sendPheader);
	sendBuf[datanum++] = message;
	/*int result = sendPackage(message);
	if (result == -1) {
		cout << "[ERROR] : Connection Failed! " << endl;
		return -1;
	}
	cout << "[SUCC] : Connection Success! " << endl;*/
	sendPackage2();
	return 0;
}

//���Ͱ���ʹ�û�������
void sendPackage2() {
	exitFlag = 0;
	while (true) {
		if (exitFlag == 1) {
			break;
		}
		if (nextseqnum < base + N && nextseqnum < base + cwnd && nextseqnum < datanum) {
			int result = -1;
			Message* message = sendBuf[nextseqnum].message;
			cout << "[SEND] : ";
			printMessage(sendBuf[nextseqnum]);
			if (!isLoss()) {
				result = sendto(clientSocket, (char*)message, sizeof(struct Message), 0, (struct sockaddr*)&serverAddrIn, sizeof(SOCKADDR));
			}
			if (base == nextseqnum) {
				timer.start();
			}
			nextseqnum++;
			cout << "[WINDOW] : base = " << base << " nextseqnum = " << nextseqnum << " cwnd=" << cwnd << endl;
		}
	}
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
		base = 0;
		nextseqnum = 0;
		datanum = 0;
		memset(sendBuf, 0, 2000);
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
	message_SEND->seq = datanum;
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
	sendBuf[datanum++] = message_S;
	/*int result = sendPackage(message_S);
	if (result == -1) {
		cout << "[ERROR] : File " << file.name << " Send Failed! " << endl;
		return -1;
	}*/
	//��ȡ�ļ����ݣ�д�뷢�ͻ�����
	ifstream fileStream(file.name, ios::binary | ios::app);
	int len = file.size;
	for (int i = 0; i < index; i++) {
		//��ȡ�ļ�����
		char fileContent[DSIZE];
		fileStream.read(fileContent, fmin(len, DSIZE));
		//д�뷢�ͻ�����
		Message* message_FILE;
		message_FILE = new Message();
		message_FILE->source_port = clientPort;
		message_FILE->dest_port = serverPort;
		message_FILE->seq = datanum;
		message_FILE->ack = 0;
		Message_C message_F(message_FILE);
		message_F.setACK();
		message_F.setLen(fmin(len, DSIZE));
		message_F.setData(fileContent);
		if (i == message_S.getIndex()) {
			message_F.setEF();
		}
		message_F.setcksum(&sendPheader);
		sendBuf[datanum++] = message_F;
		len -= DSIZE;
		//cout << "[FILE INDEX] " << i << " in " << index << endl;
		/*int result = sendPackage(message_F);
		if (result == -1) {
			cout << "[ERROR] : File " << file.name << "Index " << i << " Send Failed! " << endl;
			return -1;
		}*/
	}
	sendPackage2();
	return 0;
}

int disConnection() {
	base = 0;
	nextseqnum = 0;
	datanum = 0;
	memset(sendBuf, 0, 2000);
	Message* message_FIN;
	message_FIN = new Message();
	message_FIN->source_port = clientPort;
	message_FIN->dest_port = serverPort;
	message_FIN->seq = datanum;
	message_FIN->ack = 0;
	Message_C message(message_FIN);
	message.setLen(0);
	message.setFIN();
	message.setcksum(&sendPheader);
	sendBuf[datanum++] = message;
	/*int result = sendPackage(message);
	if (result == -1) {
		cout << "[ERROR] : Disconnection Failed! " << endl;
		return -1;
	}
	closesocket(clientSocket);
	cout << "[SUCC] : Connection Destroyed! " << endl;*/
	sendPackage2();
	return 0;
}