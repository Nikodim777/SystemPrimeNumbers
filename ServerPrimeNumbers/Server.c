//����������� �������������� ������ Windows (Winsows 7)
#define _WIN32_WINNT 0x601

#include <winsock2.h>
#pragma comment(lib, "WS2_32.lib")
#include <ws2tcpip.h>

#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#define BUFSIZE 32
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
	maxPreCalcNum = 0, //������������ ����� ��� �������� ��� ����������� ������� �����
	numRequests = 0; //����� �������� ������������ ��������

float allProcessTime = 0.0; //��������� ����� ��������� ��������
					
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
	char buffer[BUFSIZE];
	int result, sumresult, id = *(int*)ptrID;

	unsigned __int32 resNumPrimes, //���������� ������ ������� ������� �����
		maxNum; //����� �� �������� ����� �������� ������� �����

	clock_t t1, t2;
	float t_diff; //����� �������

	while (1){
		//��������� �������
		ZeroMemory(&buffer, sizeof(buffer));
		result = recv(clientSocket[id], buffer, BUFSIZE, 0);
		if (result == SOCKET_ERROR || result == 0)
		{
			ProcessClientErrorExit(id);
			break;
		}

		printf("������[IPv4: %s;����: %d;ID: %d] �������� �������� '%s'\n",
			inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id, buffer);

		//���� ������ ����� �������� ������ ������� �����
		if (sscanf_s(buffer, "prim %u", &maxNum) == 1)
		{
			t1 = clock();
			WaitForSingleObject(mutex, INFINITE); //���� ��� �������� �������
			//��������� �� ������� ������� ������� ������ ������� �����
			//������� ���������� ����� ������� ����� �� ������������ �����
			//������� �� ���������� �������� ��� ��� ����� ������ ����� � ������� - '1' �� �������� �������,
			//�� ��������� ��� �������� ���������
			resNumPrimes = CalculatePrimes(maxNum) - 1;
			ReleaseMutex(mutex);
			t2 = clock();

			//������� ����� ������ ����� ���� �� ����� ���������� �������� �������� � �������
			if (resNumPrimes == 0xfffffffe)
			{
				ProcessClientErrorExit(id);
				break;
			}

			//�������� ���������� ������� �����
			result = send(clientSocket[id], (const char *)&resNumPrimes, UINTNUMBYTES, 0);
			if (result == SOCKET_ERROR)
			{
				ProcessClientErrorExit(id);
				break;
			}

			//�������� ������ ������� �����, �.�. �� ����� ���� ������� �� � �����
			sumresult = 0;
			while ((unsigned)sumresult < resNumPrimes * UINTNUMBYTES)
			{
				result = send(clientSocket[id], (const char *)&primes[sumresult / UINTNUMBYTES + 1],
								resNumPrimes * UINTNUMBYTES - sumresult, 0);
				if (result == SOCKET_ERROR)
				{
					ProcessClientErrorExit(id);
					break;
				}
				printf("�������[IPv4: %s;����: %d;ID: %d] ��������� ������ ����� %d ����\n",
					inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id, result);
				sumresult += result;
			}

			//�������� ����� ��������� �������
			t_diff = (float)(t2 - t1) / CLK_TCK;
			result = send(clientSocket[id], (const char *)&t_diff, sizeof(float), 0);
			if (result == SOCKET_ERROR)
			{
				ProcessClientErrorExit(id);
				break;
			}

			allProcessTime += t_diff;
			numRequests++;
		}
		//���� �������� ������� ������ ����������
		else if (!strncmp(buffer, "stat", 4))
		{
			//�������� ����� ����� ��������
			result = send(clientSocket[id], (const char *)&numRequests, UINTNUMBYTES, 0);
			if (result == SOCKET_ERROR)
			{
				ProcessClientErrorExit(id);
				break;
			}

			//�������� ������� ����� ��������� �������
			t_diff = (float)(numRequests ? allProcessTime / numRequests : 0.0);
			result = send(clientSocket[id], (const char *)&t_diff, sizeof(float), 0);
			if (result == SOCKET_ERROR)
			{
				ProcessClientErrorExit(id);
				break;
			}
		}
		else
			printf("����������� ������� '%s'!\n", buffer);

		printf("�������� �������[IPv4: %s;����: %d;ID: %d] '%s' ����������\n",
			inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id, buffer);
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
			if (isPrime[i - 1])
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
