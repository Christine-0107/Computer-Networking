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
#pragma comment(lib,"ws2_32.lib") //静态加入库文件，提供网络相关API支持
#pragma warning(disable:4996) //屏蔽warning，提高兼容性
using namespace std;

SOCKET clientSocket; //客户端socket
SOCKADDR_IN serverAddrIn; //储存服务器端地址和端口号
SOCKADDR_IN clientAddrIn; //储存客户端地址和端口号
const int serverPort = 8888;
const int clientPort = 9999;

u_long non_block = 1; //设置非阻塞
std::string fileDir = "./test";
string name1 = "./test/1.jpg";
string name2 = "./test/2.jpg";
string name3 = "./test/3.jpg";
string name4 = "./test/helloworld.txt";


struct Pseudoheader sendPheader {};
struct Pseudoheader recvPheader {};

//GBN
int N = 10; //窗口大小
int base = 0;
int nextseqnum = 0;
int datanum = 0;
Message_C sendBuf[2000]; //发送缓冲区
int curseq = 0;
Timer timer;
int exitFlag = 0;

//RENO
int dupACKcount = 0;
int state = 0; //慢启动0，拥塞控制1，快速恢复2
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

	//5. 开启接收线程
	beginRecv();

	//6. 开启超时重传线程
	beginTimeout();

	//7. 发起建立连接
	while ((result = connection()) == -1) {
		cout << "Retry connection in 1 seconds" << endl;
		Sleep(1000);
	}

	//8. 发送文件
	result = sendFiles();

	//9. 断开连接
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
				//检查校验和
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
					//根据返回的ACK设置状态
					bool newACK = false;
					if (recvMesg.ack + 1 > base) {
						newACK = true;
					}
					//更新base
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
					//更新状态
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
							//重传该重复ack对应消息
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
							//重传该重复ack对应消息
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
	message.setSYN(); //SYN标志位置1
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

//发送包，使用滑动窗口
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
//发送单个文件
int sendFile(File_C file) {
	//文件开始消息，包括文件名和大小
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
	//计算需要多少个数据报才能发完一个文件
	int index = ceil(((double)file.size) / DSIZE);
	message_S.setIndex((short)index - 1); //index从0开始
	message_S.setcksum(&sendPheader);
	sendBuf[datanum++] = message_S;
	/*int result = sendPackage(message_S);
	if (result == -1) {
		cout << "[ERROR] : File " << file.name << " Send Failed! " << endl;
		return -1;
	}*/
	//读取文件内容，写入发送缓冲区
	ifstream fileStream(file.name, ios::binary | ios::app);
	int len = file.size;
	for (int i = 0; i < index; i++) {
		//读取文件内容
		char fileContent[DSIZE];
		fileStream.read(fileContent, fmin(len, DSIZE));
		//写入发送缓冲区
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