//����������� �������������� ������ Windows (Winsows 7)
#define _WIN32_WINNT 0x603

#include <winsock2.h>
#pragma comment(lib, "WS2_32.lib")
#include <ws2tcpip.h>

#include <locale.h>
#include <stdio.h>
#include <string.h>

#define BUFSIZE 32
#define UINTNUMBYTES 4

//������� �������������� ������� � ���������� ���������� ������
int ProcessErrorExit(SOCKET socket);
int ProcessServerErrorExit();

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "rus");

	WSADATA wsaData;
	SOCKET connSocket = INVALID_SOCKET;

	struct addrinfo *resAddr = NULL, //������ �������
		*iterAddr = NULL, //�������� ��� �������
		hints; //��������� ��� ��������� �������

	const char *srvport = "666";
	char buffer[BUFSIZE], host[BUFSIZE] = "localhost";
	const int timeout = 10000;
	int result, sumresult;

	unsigned __int32 *primes = NULL, //������ ������� �����
		numPrimes, //���������� ������� �����
		numRequests, //����� �������� ������������ ��������
		tmp;

	float aveProcessTime, //������� ����� ��������� �������
		processTime; //����� ��������� �������

	//��������� ���������� ���������
	if (argc == 2 && strcmp(argv[1], "/?"))
		strcpy_s(host, BUFSIZE, argv[1]);
	else if (argc > 2)
	{
		printf("�������������: %s [server-name]\n", argv[0]);
		return 1;
	}

	//������ winsock ������ 2.2
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		printf("���� ��� ���������� WSAStartup � ����� %d\n", result);
		system("pause");
		return 1;
	}

	//��������� ���������� ������
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;

	//��������� ���������� �� ������� �������
	result = getaddrinfo(host, srvport, &hints, &resAddr);
	if (result != 0)
	{
		printf("���� ��� ���������� getaddrinfo � ����� %d\n", result);
		return ProcessErrorExit((SOCKET)NULL);
	}

	//����� �������� ������ ��� �����������
	for (iterAddr = resAddr; iterAddr != NULL; iterAddr = iterAddr->ai_next)
	{
		connSocket = socket(iterAddr->ai_family, iterAddr->ai_socktype, iterAddr->ai_protocol);
		if (connSocket == INVALID_SOCKET)
		{
			printf("���� ��� �������� ������ � ����� %d\n", WSAGetLastError());
			freeaddrinfo(resAddr);
			return ProcessErrorExit((SOCKET)NULL);
		}

		result = connect(connSocket, iterAddr->ai_addr, (int)iterAddr->ai_addrlen);
		if (result != SOCKET_ERROR)
			break;

		closesocket(connSocket);
		connSocket = INVALID_SOCKET;
	}

	freeaddrinfo(resAddr);

	if (connSocket == INVALID_SOCKET)
	{
		printf("���������� ������������ � �������!\n");
		return ProcessErrorExit((SOCKET)NULL);
	}
	else
		printf("���������� � �������� �����������!\n"
			   "����� �������� ������ ������� ����� �� N ������� 'prim N'.\n"
			   "N ����� ���� � ��������� �� 2 �� 100,000,000\n"
			   "����� �������� ���������� � ������� ������� 'stat'\n"
			   "����� ����� ������� 'exit'\n");

	//��������� ������������� ������� ��������
	setsockopt(connSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(int));

	while (1)
	{
		//������ ����������� ��� �����
		printf(">> ");

		//���������� �������
		ZeroMemory(&buffer, sizeof(buffer));
		gets_s(buffer, BUFSIZE);

		if ((sscanf_s(buffer, "prim %u", &tmp) == 1 && tmp >= 2 && tmp <= 100000000) || !strcmp(buffer, "stat"))
		{
			//�������� ������� �� ������
			result = send(connSocket, buffer, strlen(buffer), 0);
			if (result == SOCKET_ERROR)
			{
				printf("���� ��� �������� ������ � ����� %d\n", WSAGetLastError());
				return ProcessErrorExit(connSocket);
			}
		}

		//���� ���������� ������� ��� ��������� ������ ������� �����
		if (sscanf_s(buffer, "prim %u", &tmp) == 1)
		{
			if (tmp < 2 || tmp > 100000000)
			{
				printf("����� ������ ���� � ��������� �� 2 �� 100000000\n");
				continue;
			}

			//��������� ���������� ������� �����
			result = recv(connSocket, (char *)&numPrimes, UINTNUMBYTES, 0);
			if (result == SOCKET_ERROR)
				return ProcessServerErrorExit(connSocket);

			//��������� ������ ��� ������
			primes = (unsigned __int32*)malloc(numPrimes * UINTNUMBYTES);
			if (primes == NULL)
			{
				printf("�� ������� �������� ������� ������\n");
				return ProcessErrorExit(connSocket);
			}

			//��������� ������ ������� �����
			//��������� ������ ����� ���� ����� ������� �� ����������� �� ������ � ������� ��������� ������ � �����
			sumresult = 0;
			while ((unsigned)sumresult < numPrimes * UINTNUMBYTES){
				result = recv(connSocket, (char *)&primes[sumresult / UINTNUMBYTES],
								numPrimes * UINTNUMBYTES - sumresult, 0);
				if (result == SOCKET_ERROR)
					return ProcessServerErrorExit(connSocket);
				printf("������� ����� ������ ����� %d ����\n", result);
				sumresult += result;
			}

			//��������� ������� ��������� �������
			result = recv(connSocket, (char *)&processTime, sizeof(float), 0);
			if (result == SOCKET_ERROR)
				return ProcessServerErrorExit(connSocket);


			//����� �����������
			printf("����� ��������� �������: %.3f ������\n", processTime);
			printf("��������� ������ �� %u ������� �����. �������?(yes/no)\n", numPrimes);
			gets_s(buffer, BUFSIZE);

			if (!strcmp(buffer, "yes"))
			{
				for (int i = 0; i < (int)numPrimes; i++)
					printf("%u ", primes[i]);
				puts("");
			}

			free(primes);
		}
		//���� ���������� ������� ��� ��������� ����������
		else if (!strcmp(buffer, "stat"))
		{
			//��������� ����� �������� ������������ ��������
			result = recv(connSocket, (char *)&numRequests, UINTNUMBYTES, 0);
			if (result == SOCKET_ERROR)
				return ProcessServerErrorExit(connSocket);

			//��������� �������� ������� ��������� �������
			result = recv(connSocket, (char *)&aveProcessTime, sizeof(float), 0);
			if (result == SOCKET_ERROR)
				return ProcessServerErrorExit(connSocket);

			//����� �����������
			printf("����� �������� ������������ ��������: %u\n", numRequests);
			printf("������� ����� ��������� �������: %.3f ������\n", aveProcessTime);
		}
		else if (!strcmp(buffer, "exit"))
			break;
		else
			printf("����������� ������� '%s'\n", buffer);
	}

	shutdown(connSocket, SD_BOTH);
	closesocket(connSocket);
	WSACleanup();

	return 0;
}

int ProcessErrorExit(SOCKET socket)
{
	shutdown(socket, SD_BOTH);
	closesocket(socket);
	WSACleanup();

	system("pause");
	return 1;
}

int ProcessServerErrorExit(SOCKET socket)
{
	if (WSAGetLastError() == WSAECONNRESET || WSAGetLastError() == WSAETIMEDOUT)
		puts("������ �� ��������\n");
	else
		printf("���� ��� �������� ������ ��������� � ����� %d\n", WSAGetLastError());

	return ProcessErrorExit(socket);
}