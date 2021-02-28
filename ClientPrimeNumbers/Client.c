//Минимальная поддерживаемая версия Windows (Winsows 7)
#define _WIN32_WINNT 0x603

#include <winsock2.h>
#pragma comment(lib, "WS2_32.lib")
#include <ws2tcpip.h>

#include <locale.h>
#include <stdio.h>
#include <string.h>

#define BUFSIZE 32
#define UINTNUMBYTES 4

//Функции подготавливают систему к аварийному завершению работы
int ProcessErrorExit(SOCKET socket);
int ProcessServerErrorExit();

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "rus");

	WSADATA wsaData;
	SOCKET connSocket = INVALID_SOCKET;

	struct addrinfo *resAddr = NULL, //Массив адресов
		*iterAddr = NULL, //Итератор для массива
		hints; //Структура для настройки сокетов

	const char *srvport = "666";
	char buffer[BUFSIZE], host[BUFSIZE] = "localhost";
	const int timeout = 10000;
	int result, sumresult;

	unsigned __int32 *primes = NULL, //Список простых чисел
		numPrimes, //Количество простых чисел
		numRequests, //Число запросов обработанных сервером
		tmp;

	float aveProcessTime, //Среднее время обработки запроса
		processTime; //Время обработки запроса

	//Обработка аргументов программы
	if (argc == 2 && strcmp(argv[1], "/?"))
		strcpy_s(host, BUFSIZE, argv[1]);
	else if (argc > 2)
	{
		printf("Использование: %s [server-name]\n", argv[0]);
		return 1;
	}

	//Запуск winsock версии 2.2
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		printf("Сбой при выполнении WSAStartup с кодом %d\n", result);
		system("pause");
		return 1;
	}

	//Настройка параметров сокета
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;

	//Получение информации об адресах сервера
	result = getaddrinfo(host, srvport, &hints, &resAddr);
	if (result != 0)
	{
		printf("Сбой при выполнении getaddrinfo с кодом %d\n", result);
		return ProcessErrorExit((SOCKET)NULL);
	}

	//Поиск рабочего адреса для подключения
	for (iterAddr = resAddr; iterAddr != NULL; iterAddr = iterAddr->ai_next)
	{
		connSocket = socket(iterAddr->ai_family, iterAddr->ai_socktype, iterAddr->ai_protocol);
		if (connSocket == INVALID_SOCKET)
		{
			printf("Сбой при создании сокета с кодом %d\n", WSAGetLastError());
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
		printf("Невозможно подключиться к серверу!\n");
		return ProcessErrorExit((SOCKET)NULL);
	}
	else
		printf("Соединение с сервером установлено!\n"
			   "Чтобы получить список простых чисел до N введите 'prim N'.\n"
			   "N может быть в диапазоне от 2 до 100,000,000\n"
			   "Чтобы получить статистику с сервера введите 'stat'\n"
			   "Чтобы выйти введите 'exit'\n");

	//Установка максимального времени ожидания
	setsockopt(connSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(int));

	while (1)
	{
		//Печать приглашения для ввода
		printf(">> ");

		//Считывание команды
		ZeroMemory(&buffer, sizeof(buffer));
		gets_s(buffer, BUFSIZE);

		if ((sscanf_s(buffer, "prim %u", &tmp) == 1 && tmp >= 2 && tmp <= 100000000) || !strcmp(buffer, "stat"))
		{
			//Отправка команды на сервер
			result = send(connSocket, buffer, strlen(buffer), 0);
			if (result == SOCKET_ERROR)
			{
				printf("Сбой при отправке данных с кодом %d\n", WSAGetLastError());
				return ProcessErrorExit(connSocket);
			}
		}

		//Если отправлена команда для получения списка простых чисел
		if (sscanf_s(buffer, "prim %u", &tmp) == 1)
		{
			if (tmp < 2 || tmp > 100000000)
			{
				printf("Число должно быть в диапазоне от 2 до 100000000\n");
				continue;
			}

			//Получение количества простых чисел
			result = recv(connSocket, (char *)&numPrimes, UINTNUMBYTES, 0);
			if (result == SOCKET_ERROR)
				return ProcessServerErrorExit(connSocket);

			//Выделение памяти под список
			primes = (unsigned __int32*)malloc(numPrimes * UINTNUMBYTES);
			if (primes == NULL)
			{
				printf("Не удалось выделить столько памяти\n");
				return ProcessErrorExit(connSocket);
			}

			//Получение списка простых чисел
			//Поскольку список может быть очень большой он передасться по частям и поэтому принимаем данные в цикле
			sumresult = 0;
			while ((unsigned)sumresult < numPrimes * UINTNUMBYTES){
				result = recv(connSocket, (char *)&primes[sumresult / UINTNUMBYTES],
								numPrimes * UINTNUMBYTES - sumresult, 0);
				if (result == SOCKET_ERROR)
					return ProcessServerErrorExit(connSocket);
				printf("Принята часть списка весом %d байт\n", result);
				sumresult += result;
			}

			//Получение времени обработки запроса
			result = recv(connSocket, (char *)&processTime, sizeof(float), 0);
			if (result == SOCKET_ERROR)
				return ProcessServerErrorExit(connSocket);


			//Вывод результатов
			printf("Время обработки запроса: %.3f секунд\n", processTime);
			printf("Возвращён список из %u простых чисел. Вывести?(yes/no)\n", numPrimes);
			gets_s(buffer, BUFSIZE);

			if (!strcmp(buffer, "yes"))
			{
				for (int i = 0; i < (int)numPrimes; i++)
					printf("%u ", primes[i]);
				puts("");
			}

			free(primes);
		}
		//Если отправлена команда для получения статистики
		else if (!strcmp(buffer, "stat"))
		{
			//Получение числа запросов обработанных сервером
			result = recv(connSocket, (char *)&numRequests, UINTNUMBYTES, 0);
			if (result == SOCKET_ERROR)
				return ProcessServerErrorExit(connSocket);

			//Получение среднего времени обработки запроса
			result = recv(connSocket, (char *)&aveProcessTime, sizeof(float), 0);
			if (result == SOCKET_ERROR)
				return ProcessServerErrorExit(connSocket);

			//Вывод результатов
			printf("Число запросов обработанных сервером: %u\n", numRequests);
			printf("Среднее время обработки запроса: %.3f секунд\n", aveProcessTime);
		}
		else if (!strcmp(buffer, "exit"))
			break;
		else
			printf("Неизвестная команда '%s'\n", buffer);
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
		puts("Сервер не отвечает\n");
	else
		printf("Сбой при принятии данных произошёл с кодом %d\n", WSAGetLastError());

	return ProcessErrorExit(socket);
}