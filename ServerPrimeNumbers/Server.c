//����������� �������������� ������ Windows (Winsows 7)
#define _WIN32_WINNT 0x601

#include <winsock2.h>
#pragma comment(lib, "WS2_32.lib")
#include <ws2tcpip.h>

#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include "../Command.h"

#define MAXCLIENTS 16
#define UINTNUMBYTES 4

//������������ �������� ��������� ������
enum busyment
{
	FREE,
	BUSY
};

char *port = "666";

SOCKET clientSocket[MAXCLIENTS] = { INVALID_SOCKET }; //������ ������� ��������
enum busyment busymentSocket[MAXCLIENTS] = { FREE }; //������ �������� ��������� ��� ������� ������
struct sockaddr_in clientAddr[MAXCLIENTS]; //������ ������� ��������

HANDLE mutex; //������� ��� �������� ������� � ���������� ���������� ��������� � �������� �������

unsigned __int32 *primes = NULL, //������ ������� �����
	*tmpPrimes = NULL,
	numPrimes = 0, //���������� ��������������� ������� �����
	maxPreCalcNum = 0; //������������ ����� ��� �������� ��� ����������� ������� �����

Statistics stat = { 0, 0, 0.0 };
					
//������� �������������� ������� � ���������� ���������� ������
int ProcessErrorExit(SOCKET socket);
void ProcessClientErrorExit(int id);

int GetFreeSocket(); //������� ���������� ������ ��������� �����
int ClientProcess(LPVOID ptrID); //������� ��� ������ ������ � �������������
unsigned __int32 CalculatePrimes(unsigned __int32 maxNum); //������� �����������/��������� ������ ������� �����

int main()
{
	setlocale(LC_ALL, "rus");

	WSADATA wsaData;
	SOCKET serverSocket = INVALID_SOCKET;
	struct addrinfo *resAddr = NULL, //��������� �� ������ �������
		hints; //��������� ��� ��������� �������

	int result = 0, freeSock = 0,
		addrlen = sizeof(clientAddr[0]);

	mutex = CreateMutex(NULL, 0, NULL);

	//������ winsock ������ 2.2
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		printf("���� ��� ���������� WSAStartup � ����� %d\n", result);
		return 1;
	}

	//��������� ���������� �������
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;

	//��������� ���������� �� ��������������� �������
	result = getaddrinfo(NULL, port, &hints, &resAddr);
	if (result != 0)
	{
		printf("���� ��� ���������� getaddrinfo � ����� %d\n", result);
		WSACleanup();
		return 1;
	}

	//�������� ������
	serverSocket = socket(resAddr->ai_family, resAddr->ai_socktype, resAddr->ai_protocol);
	if (serverSocket == INVALID_SOCKET)
	{
		printf("���� ��� �������� ������ ������� � ����� %d\n", WSAGetLastError());
		freeaddrinfo(resAddr);
		WSACleanup();
		return 1;
	}

	//���������� ������ � �������
	result = bind(serverSocket, resAddr->ai_addr, (int)resAddr->ai_addrlen);
	if (result == SOCKET_ERROR)
	{
		printf("���� ��� ���������� ������ ������� � ������� ��������� � ����� %d\n", WSAGetLastError());
		freeaddrinfo(resAddr);
		return ProcessErrorExit(serverSocket);
	}

	freeaddrinfo(resAddr);

	result = listen(serverSocket, MAXCLIENTS);
	if (result == SOCKET_ERROR)
	{
		printf("���� ��� ���������� listen � ����� %d\n", WSAGetLastError());
		return ProcessErrorExit(serverSocket);
	}

	//� ����� ������������ ����� ����������� � �������� ����� ����� ��� ������������ ��������
	while (1)
	{
		Sleep(100); //��������� ������� ����� ������� ���-�� ������� GetFreeSocket �� ���������� ������� �����
		freeSock = GetFreeSocket();
		if (freeSock == -1)
			continue;

		//�������� ���������� ������� � �������
		clientSocket[freeSock] = accept(serverSocket, (struct sockaddr*)&clientAddr[freeSock], &addrlen);
		if (clientSocket[freeSock] == INVALID_SOCKET)
			printf("���� ��� ���������� accept � ����� %d\n", WSAGetLastError());

		printf("���������� �����������!\nIPv4: %s;����: %d;ID: %d\n", inet_ntoa(clientAddr[freeSock].sin_addr),
			clientAddr[freeSock].sin_port, freeSock);
		busymentSocket[freeSock] = BUSY;

		//��������� ������ ��� ������ �������
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ClientProcess, &freeSock, 0, NULL);
	}

	result = shutdown(serverSocket, SD_SEND);
	if (result != 0)
	{
		printf("���� ��� ���������� shutdown � ����� %d\n", WSAGetLastError());
		return ProcessErrorExit(serverSocket);
	}
	closesocket(serverSocket);
	WSACleanup();

	free(primes);

	return 0;
}

int ProcessErrorExit(SOCKET socket)
{
	shutdown(socket, SD_BOTH);
	closesocket(socket);
	WSACleanup();

	free(primes);

	system("pause");
	return 1;
}

void ProcessClientErrorExit(int id)
{
	if (WSAGetLastError() == WSAECONNRESET || WSAGetLastError() == 0)
		printf("������[IPv4: %s;����: %d;ID: %d] ����������\n",
			inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id);
	else
		printf("���� ��� ������ ������� c ��������[IPv4: %s;����: %d;ID: %d] � ����� %d\n",
				inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id, WSAGetLastError());

	busymentSocket[id] = FREE; //����������� ����� �������
}

int GetFreeSocket()
{
	int i;
	for (i = 0; i < MAXCLIENTS; i++)
	if (busymentSocket[i] == FREE)
		return i;
	return -1;
}

int ClientProcess(LPVOID ptrID)
{
	int id = *(int*)ptrID;
	unsigned __int32 resNumPrimes, //���������� ������ ������� ������� �����
		result, sumresult, packetSize;
	float processTime;

	Command command;//����������� ��������

	clock_t t1, t2;

	char *packetPrimes = NULL;


	while (1){
		//��������� �������
		result = recv(clientSocket[id], (char *)&command, sizeof(Command), 0);
		if (result == SOCKET_ERROR || result == 0)
		{
			ProcessClientErrorExit(id);
			break;
		}

		printf("������[IPv4: %s;����: %d;ID: %d] �������� �������� '%c %u' c id %u\n",
			inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id, command.type, command.data, command.id);

		//���� ������ ����� �������� ������ ������� �����
		if (command.type == PRIM)
		{
			t1 = clock();
			WaitForSingleObject(mutex, INFINITE); //���� ��� �������� �������
			//��������� �� ������� ������� ������� ������ ������� �����
			//������� ���������� ����� ������� ����� �� ������������ �����
			//������� �� ���������� �������� ��� ��� ����� ������ ����� � ������� - '1' �� �������� �������,
			//�� ��������� ��� �������� ���������
			resNumPrimes = CalculatePrimes(command.data) - 1;
			ReleaseMutex(mutex);
			t2 = clock();
			processTime = (float)(t2 - t1) / CLK_TCK;//������� ������� ��������� �������

			//������� ����� ������ ����� ���� �� ����� ���������� �������� �������� � �������
			if (resNumPrimes == 0xfffffffe)
			{
				ProcessClientErrorExit(id);
				break;
			}

			//���������� ������
			packetSize = UINTNUMBYTES + UINTNUMBYTES + resNumPrimes * UINTNUMBYTES + sizeof(float);
			packetPrimes = malloc(packetSize);
			if (packetPrimes == NULL)
			{
				ProcessClientErrorExit(id);
				break;
			}
			memcpy(packetPrimes, (void*)&command.id, UINTNUMBYTES); //�������� id
			memcpy(&packetPrimes[UINTNUMBYTES], (void*)&resNumPrimes, UINTNUMBYTES); //�������� ���������� ������� �����
			memcpy(&packetPrimes[2 * UINTNUMBYTES], (void*)&primes[1], resNumPrimes * UINTNUMBYTES); //�������� ������ ������� �����
			memcpy(&packetPrimes[packetSize - sizeof(float)], (void*)&processTime, sizeof(float)); //�������� ������� ��������� �������

			//�������� ������ ������� �����, �.�. �� ����� ���� ������� �� � �����
			sumresult = 0;
			while (sumresult < packetSize)
			{
				result = send(clientSocket[id], &packetPrimes[sumresult], packetSize - sumresult, 0);
				if (result == SOCKET_ERROR)
				{
					ProcessClientErrorExit(id);
					break;
				}
				printf("�������[IPv4: %s;����: %d;ID: %d] ��������� ������ ����� %u ����\n",
					inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id, result);
				sumresult += result;
			}

			free(packetPrimes);

			//��������� ����������
			WaitForSingleObject(mutex, INFINITE); //���� ��� �������� �������
			stat.aveProcessTime = (stat.aveProcessTime * stat.numRequests + processTime) / (stat.numRequests + 1);
			stat.numRequests++;
			ReleaseMutex(mutex);
		}
		//���� �������� ������� ������ ����������
		else if (command.type == STAT)
		{
			stat.id = command.id;

			result = send(clientSocket[id], (const char *)&stat, sizeof(Statistics), 0);
			if (result == SOCKET_ERROR)
			{
				ProcessClientErrorExit(id);
				break;
			}
		}
		else
			printf("����������� ������� '%c %u'!\n", command.type, command.data);

		printf("�������� �������[IPv4: %s;����: %d;ID: %d] c id %u ����������\n",
			inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id, command.id);
	}

	shutdown(clientSocket[id], SD_SEND);
	closesocket(clientSocket[id]);

	ExitThread(0);
}

unsigned __int32  CalculatePrimes(unsigned __int32 maxNum)
{
	unsigned __int32 i, j, sqrtMaxNum, //������ �� ����� �� �������� ����� ������� ������� �����
		numNewPrimes = 0; //���������� ����� ����������� ������� ����� ������� ����� �������� � ���������� ������
	char *isPrime = NULL; //������ �������� ���������� � �������� �����

	//���� ������� ����� �� ���������� ������� (��� �� ��������) ����������� �����
	if (maxNum <= maxPreCalcNum)
	{
		//������ ��� ��� ���� � ���������� ������� � �� ������ ������� ������ ������������ ����� ���������
		for (i = 0; i < numPrimes && primes[i] <= maxNum; i++);
		printf("%d\n", i);

		return i;
	}
	else
	{
		sqrtMaxNum = (unsigned __int32)sqrt(maxNum + 0.);

		//�������� ������ ��� ������ � ����������� � �������� � ��������� ��� ���������
		//�������� ������ ��� �������� ��� ����������� ������� ����� ��� ��� ���������� � �������� ����� �� �����
		//������� ����� ���������� �������� �� ������� primes
		isPrime = (char*)malloc(maxNum - maxPreCalcNum);
		if (isPrime == NULL)
			return 0xffffffff;
		memset(isPrime, 1, maxNum - maxPreCalcNum);

		//� ����� ������������� ��� ����� ����������� ������� ����� ��� ���������� ����������
		//�� ���� ���� ��� � primes ��� ��������� ����������� ��� ���������� ������� ������� ������� �����
		//����� �������� ���������� �� ������ ��� isPrime
		for (i = 1; i < numPrimes && primes[i] <= sqrtMaxNum; i++)
			{
				for (j = primes[i] * primes[i]; j <= maxPreCalcNum; j += primes[i]);
				for (; j <= maxNum; j += primes[i])
					isPrime[j - maxPreCalcNum - 1] = 0;
			}
		
		//���� ���������� �������� ������� ���� ������� ��� �������� ��� ����������� ������� �����
		for (i = numPrimes ? maxPreCalcNum + 1 : 2; i <= sqrtMaxNum; i++)
			if (isPrime[i - maxPreCalcNum - 1])
				for (j = i * i; j <= maxNum; j += i)
					isPrime[j - maxPreCalcNum - 1] = 0;

		//������� ����� ����� ����������� ������� �����
		for (i = 0; i < maxNum - maxPreCalcNum; i++)
			if (isPrime[i])
				numNewPrimes++;

		//���� ��� ����� �� ����� 0 
		if (numNewPrimes)
		{
			//������������ ������ ��� ������ ������� �����
			tmpPrimes = (unsigned __int32*)realloc((void *)primes, (numPrimes + numNewPrimes) * UINTNUMBYTES);
			if (tmpPrimes == NULL)
			{
				free(isPrime);
				return 0xffffffff;
			}
			primes = tmpPrimes;

			//��������� ����� ������
			for (i = 0; i < maxNum - maxPreCalcNum; i++)
				if (isPrime[i])
					primes[numPrimes++] = i + maxPreCalcNum + 1;
		}
		
		free(isPrime);

		maxPreCalcNum = maxNum;
		return numPrimes;
	}
}
