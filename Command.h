#pragma once

typedef enum
{
	PRIM = 'p',
	STAT = 's',
	EXIT = 'e',
	UNKNOWN = 'u'
} CommandType;

typedef struct
{
	CommandType type;
	unsigned __int32 id;
	unsigned __int32 data;
} Command;

typedef struct
{
	unsigned __int32 id;
	unsigned __int32 numRequests;
	float aveProcessTime;
} Statistics;