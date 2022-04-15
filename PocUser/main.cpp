
#include "global.h"
#include "..\PocUserDll\pch.h"

#define POC_HELLO_KERNEL			1
#define POC_PRIVILEGE_DECRYPT		4
#define POC_PRIVILEGE_ENCRYPT		8

//��PocUser.exe·����ִ��cmd, ���� 
// PocUser.exe 4 "C:\Desktop\a.txt" ����
// PocUser.exe 8 "C:\Desktop\a.txt"	����

DWORD WINAPI PocGetMessageThread(_In_ LPVOID lpParameter)
{
	UINT RetValue = 0;
	CHAR Text[20] = { 0 };
	HANDLE* hPort = (HANDLE*)lpParameter;

	PocUserGetMessage(*hPort, &RetValue);

	if (1 == RetValue)
	{
		strncpy_s(Text, "�����ɹ�", strlen("�����ɹ�"));
	}
	else
	{
		sprintf_s(Text, "����ʧ��:0x%X", RetValue);
	}

	MessageBoxA(NULL, Text, "RetValue", MB_OK);
	
	return 0;
}

int main(int argc, char* argv[])
{
	if (NULL == argv[1] || NULL == argv[2])
	{
		return 1;
	}

	HANDLE hPort = NULL;

	PocUserInitCommPort(&hPort);

	HANDLE hThread = CreateThread(NULL, NULL, PocGetMessageThread, &hPort, NULL, NULL);
												//(char)'1' -> (int) 1
	PocUserSendMessage(hPort, (LPVOID)argv[2], *argv[1] - 48);

	system("pause");

	if (NULL != hPort)
	{
		CloseHandle(hPort);
		hPort = NULL;
	}

	return 0;
}
