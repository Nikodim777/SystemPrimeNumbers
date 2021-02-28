//Минимальная поддерживаемая версия Windows (Winsows 7)
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

//Перечисление сотояний занятости сокета
enum busyment
{
	FREE,
	BUSY
};

char *port = "666";

SOCKET clientSocket[MAXCLIENTS] = { INVALID_SOCKET }; //Массив сокетов клиентов
enum busyment busymentSocket[MAXCLIENTS] = { FREE }; //Массив значений занятости для каждого сокета
struct sockaddr_in clientAddr[MAXCLIENTS]; //Массив адресов клиентов

HANDLE mutex; //Мьютекс для контроля доступа к глобальным переменным связанным с простыми числами

unsigned __int32 *primes = NULL, //Список простых чисел
	*tmpPrimes = NULL,
	numPrimes = 0, //Количество предвычисленных простых чисел
	maxPreCalcNum = 0, //Максимальное число для которого уже вычислялись простые числа
	numRequests = 0; //Число запросов обработанных сервером

float allProcessTime = 0.0; //Суммарное время обработки запросов
					
//Функции подготавливает систему к аварийному завершению работы
int ProcessErrorExit(SOCKET socket);
void ProcessClientErrorExit(int id);

int GetFreeSocket(); //Функция возвращает первый свободный сокет
int ClientProcess(LPVOID ptrID); //Функция для потока работы с пользователем
unsigned __int32 CalculatePrimes(unsigned __int32 maxNum); //Функция высчитывает/обновляет массив простых чисел

int main()
{
	setlocale(LC_ALL, "rus");

	WSADATA wsaData;
	SOCKET serverSocket = INVALID_SOCKET;
	struct addrinfo *resAddr = NULL, //Указатель на массив адресов
		hints; //Структура для настройки сокетов

	int result = 0, freeSock = 0,
		addrlen = sizeof(clientAddr[0]);

	mutex = CreateMutex(NULL, 0, NULL);

	//Запуск winsock версии 2.2
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		printf("Сбой при выполнении WSAStartup с кодом %d\n", result);
		return 1;
	}

	//Настройка параметров сокетов
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;

	//Получение информации об соответствующих адресах
	result = getaddrinfo(NULL, port, &hints, &resAddr);
	if (result != 0)
	{
		printf("Сбой при выполнении getaddrinfo с кодом %d\n", result);
		WSACleanup();
		return 1;
	}

	//Создание сокета
	serverSocket = socket(resAddr->ai_family, resAddr->ai_socktype, resAddr->ai_protocol);
	if (serverSocket == INVALID_SOCKET)
	{
		printf("Сбой при создании сокета сервера с кодом %d\n", WSAGetLastError());
		freeaddrinfo(resAddr);
		WSACleanup();
		return 1;
	}

	//Связывание сокета с адресом
	result = bind(serverSocket, resAddr->ai_addr, (int)resAddr->ai_addrlen);
	if (result == SOCKET_ERROR)
	{
		printf("Сбой при связывании сокета сервера с адресом произошёл с кодом %d\n", WSAGetLastError());
		freeaddrinfo(resAddr);
		return ProcessErrorExit(serverSocket);
	}

	freeaddrinfo(resAddr);

	result = listen(serverSocket, MAXCLIENTS);
	if (result == SOCKET_ERROR)
	{
		printf("Сбой при выполнении listen с кодом %d\n", WSAGetLastError());
		return ProcessErrorExit(serverSocket);
	}

	//В цикле обрабатываем новые подключения и выделяем новый поток для подключаемых клиентов
	while (1)
	{
		Sleep(100); //Небольшой перерыв между циклами что-бы функция GetFreeSocket не вызывалась слишком часто
		freeSock = GetFreeSocket();
		if (freeSock == -1)
			continue;

		//Принятие подлючения клиента к серверу
		clientSocket[freeSock] = accept(serverSocket, (struct sockaddr*)&clientAddr[freeSock], &addrlen);
		if (clientSocket[freeSock] == INVALID_SOCKET)
			printf("Сбой при выполнении accept с кодом %d\n", WSAGetLastError());

		printf("Соединение установлено!\nIPv4: %s;Порт: %d;ID: %d\n", inet_ntoa(clientAddr[freeSock].sin_addr),
			clientAddr[freeSock].sin_port, freeSock);
		busymentSocket[freeSock] = BUSY;

		//Выделение потока для нового клиента
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ClientProcess, &freeSock, 0, NULL);
	}

	result = shutdown(serverSocket, SD_SEND);
	if (result != 0)
	{
		printf("Сбой при выполнении shutdown с кодом %d\n", WSAGetLastError());
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
		printf("Клиент[IPv4: %s;Порт: %d;ID: %d] отключился\n",
			inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id);
	else
		printf("Сбой при обмене данными c клиентом[IPv4: %s;Порт: %d;ID: %d] с кодом %d\n",
				inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id, WSAGetLastError());

	busymentSocket[id] = FREE; //Освобождаем сокет клиента
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

	unsigned __int32 resNumPrimes, //Количество нужных клиенту простых чисел
		maxNum; //Число до которого нужно получить простые числа

	clock_t t1, t2;
	float t_diff; //Метки времени

	while (1){
		//Получение команды
		ZeroMemory(&buffer, sizeof(buffer));
		result = recv(clientSocket[id], buffer, BUFSIZE, 0);
		if (result == SOCKET_ERROR || result == 0)
		{
			ProcessClientErrorExit(id);
			break;
		}

		printf("Клиент[IPv4: %s;Порт: %d;ID: %d] отправил комманду '%s'\n",
			inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id, buffer);

		//Если клиент хочет получить список простых чисел
		if (sscanf_s(buffer, "prim %u", &maxNum) == 1)
		{
			t1 = clock();
			WaitForSingleObject(mutex, INFINITE); //Берём под контроль мьютекс
			//Обновляем до нужного клиенту предела список простых чисел
			//Фукнция возвращает число простых чисел до определённого числа
			//Единицу от результата отнимаем так как самое первое число в массиве - '1' не является простым,
			//но требуется для простоты алгоритма
			resNumPrimes = CalculatePrimes(maxNum) - 1;
			ReleaseMutex(mutex);
			t2 = clock();

			//Функция вернёт данное число если во время выполнения возникнёт проблемы с памятью
			if (resNumPrimes == 0xfffffffe)
			{
				ProcessClientErrorExit(id);
				break;
			}

			//Посылаем количество простых чисел
			result = send(clientSocket[id], (const char *)&resNumPrimes, UINTNUMBYTES, 0);
			if (result == SOCKET_ERROR)
			{
				ProcessClientErrorExit(id);
				break;
			}

			//Посылаем список простых чисел, т.к. он может быть большим то в цикле
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
				printf("Клиенту[IPv4: %s;Порт: %d;ID: %d] отправлен список весом %d байт\n",
					inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id, result);
				sumresult += result;
			}

			//Посылаем время обработки запроса
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
		//Если получена команда вывода статистики
		else if (!strncmp(buffer, "stat", 4))
		{
			//Посылаем общее число запросов
			result = send(clientSocket[id], (const char *)&numRequests, UINTNUMBYTES, 0);
			if (result == SOCKET_ERROR)
			{
				ProcessClientErrorExit(id);
				break;
			}

			//Посылаем среднее время обработки запроса
			t_diff = (float)(numRequests ? allProcessTime / numRequests : 0.0);
			result = send(clientSocket[id], (const char *)&t_diff, sizeof(float), 0);
			if (result == SOCKET_ERROR)
			{
				ProcessClientErrorExit(id);
				break;
			}
		}
		else
			printf("Неизвестная команда '%s'!\n", buffer);

		printf("Комманда клиента[IPv4: %s;Порт: %d;ID: %d] '%s' обработана\n",
			inet_ntoa(clientAddr[id].sin_addr), clientAddr[id].sin_port, id, buffer);
	}

	shutdown(clientSocket[id], SD_SEND);
	closesocket(clientSocket[id]);

	ExitThread(0);
}

unsigned __int32  CalculatePrimes(unsigned __int32 maxNum)
{
	unsigned __int32 i, j, sqrtMaxNum, //Корень из числа до которого нужно вернуть простые числа
		numNewPrimes = 0; //Количество новых вычисленных простых чисел которые нужно добавить в глобальный массив
	char *isPrime = NULL; //Список хранящий информацию о простоте числа

	//Если простые числа до требуемого предела (или до большего) вычислялись ранее
	if (maxNum <= maxPreCalcNum)
	{
		//Значит они уже есть в глобальном массиве и мы просто считаем нужное пользователю число элементов
		for (i = 0; i < numPrimes && primes[i] <= maxNum; i++);
		printf("%d\n", i);

		return i;
	}
	else
	{
		sqrtMaxNum = (unsigned __int32)sqrt(maxNum + 0.);

		//Выделяем память под массив с информацией о простоте и заполняем его единицами
		//Отнимаем предел для которого уже вычислялись простые числа так как информацию о простоте чисел до этого
		//предела можно однозначно получить из массива primes
		isPrime = (char*)malloc(maxNum - maxPreCalcNum);
		if (isPrime == NULL)
			return 0xffffffff;
		memset(isPrime, 1, maxNum - maxPreCalcNum);

		//В цикле используються уже ранее вычисленные простые числа для первичного отсеивания
		//За счёт того что в primes уже храняться вычисленные при предыдущих вызовах функции простые числа
		//Можно серъёзно сэкономить на памяти под isPrime
		for (i = 1; i < numPrimes && primes[i] <= sqrtMaxNum; i++)
			{
				for (j = primes[i] * primes[i]; j <= maxPreCalcNum; j += primes[i]);
				for (; j <= maxNum; j += primes[i])
					isPrime[j - maxPreCalcNum - 1] = 0;
			}
		
		//Цикл отсеивания простыми числами выше предела для которого уже вычислялись простые числа
		for (i = numPrimes ? maxPreCalcNum + 1 : 2; i <= sqrtMaxNum; i++)
			if (isPrime[i - 1])
				for (j = i * i; j <= maxNum; j += i)
					isPrime[j - maxPreCalcNum - 1] = 0;

		//Подсчёт числа новых вычисленных простых чисел
		for (i = 0; i < maxNum - maxPreCalcNum; i++)
			if (isPrime[i])
				numNewPrimes++;

		//Если это число не равно 0 
		if (numNewPrimes)
		{
			//Перевыделяем память под список простых чисел
			tmpPrimes = (unsigned __int32*)realloc((void *)primes, (numPrimes + numNewPrimes) * UINTNUMBYTES);
			if (tmpPrimes == NULL)
			{
				free(isPrime);
				return 0xffffffff;
			}
			primes = tmpPrimes;

			//Заполняем новую память
			for (i = 0; i < maxNum - maxPreCalcNum; i++)
				if (isPrime[i])
					primes[numPrimes++] = i + maxPreCalcNum + 1;
		}
		
		free(isPrime);

		maxPreCalcNum = maxNum;
		return numPrimes;
	}
}
