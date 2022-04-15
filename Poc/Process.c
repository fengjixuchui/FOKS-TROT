
#include "process.h"
#include "utils.h"
#include "processecure.h"


LIST_ENTRY PocProcessRulesListHead = { 0 };
PKSPIN_LOCK PocProcessRulesListSpinLock = { 0 };

UCHAR* PsGetProcessImageFileName(PEPROCESS EProcess);

NTSTATUS PocCreateProcessRulesNode(
	OUT PPOC_PROCESS_RULES* OutProcessRules);


NTSTATUS PocGetProcessName(
	IN PFLT_CALLBACK_DATA Data,
	IN OUT PWCHAR ProcessName)
{

	if (NULL == Data)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->Data is NULL.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	if (NULL == ProcessName)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ProcessName is NULL.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS Status = 0;

	PEPROCESS eProcess = NULL;

	PUNICODE_STRING uProcessName = NULL;



	eProcess = FltGetRequestorProcess(Data);

	if (NULL == eProcess) {

		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->EProcess FltGetRequestorProcess failed.\n", __FUNCTION__));
		goto EXIT;
	}


	HANDLE ProcessId = PsGetProcessId(eProcess);

	if (NULL == ProcessId)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("%s->PsGetProcessId %p failed.\n",
				__FUNCTION__, eProcess));

		Status = STATUS_UNSUCCESSFUL;

		goto EXIT;
	}

	if (4 == (LONGLONG)ProcessId)
	{
		wcsncpy(ProcessName, L"System", wcslen(L"System"));
		goto EXIT;
	}


	/*
	* ZwQueryInformationProcess���������е����⣬IoQueryFileDosDeviceNameҲ��
	* ��������ȡ��ZwQueryInformationProcess�ڲ����õ�һ��nt��������
	*
	* ZwQueryInformationProcess���������������PreviousMode��
	* �����������NtOpenProcess��һ�������ZwQueryInformationProcessʹ�õĻ�
	* ��Ҫ��KeIsExecutingDpc() == FALSE�����������������NtOpenProcess->...->KiStackAttachProcess
	* ����Ϊ�����������������Ϊ��������Dpc�߳�����ȥAttachProcess��
	* ����Щ��������Dpcȥ����IO���������Ի�������ǵ�minifilter��
	*/


	Status = SeLocateProcessImageName(eProcess, &uProcessName);

	if (!NT_SUCCESS(Status))
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("%s->SeLocateProcessImageName EProcess = %p failed. Status = 0x%x.\n",
				__FUNCTION__, eProcess, Status));

		goto EXIT;
	}

	if (uProcessName->MaximumLength > POC_MAX_NAME_LENGTH * sizeof(WCHAR))
	{
		Status = STATUS_INFO_LENGTH_MISMATCH;

		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
			("%s->ProcessName length to small. Status = 0x%x.\n",
				__FUNCTION__, Status));
		goto EXIT;
	}

	wcsncpy(ProcessName, uProcessName->Buffer, uProcessName->MaximumLength / sizeof(WCHAR));

	Status = STATUS_SUCCESS;

EXIT:

	if (NULL != uProcessName)
	{
		ExFreePool(uProcessName);
		uProcessName = NULL;
	}

	return Status;
}


NTSTATUS PocProcessRulesListInit()
{
	NTSTATUS Status = 0;

	PPOC_PROCESS_RULES ProcessRules = NULL;

	PocProcessRulesListSpinLock = ExAllocatePoolWithTag(
		NonPagedPool,
		sizeof(KSPIN_LOCK),
		POC_PR_LIST_TAG);

	if (NULL == PocProcessRulesListSpinLock)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ExAllocatePoolWithTag PocProcessRulesListSpinLock failed.\n", __FUNCTION__));
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto EXIT;
	}

	RtlZeroMemory(PocProcessRulesListSpinLock, sizeof(KSPIN_LOCK));

	InitializeListHead(&PocProcessRulesListHead);
	KeInitializeSpinLock(PocProcessRulesListSpinLock);


	/*
	* PocUser.exe��������Ȩ���̣�explorer.exe����.doc�ļ��Ǳ����
	*/


	Status = PocCreateProcessRulesNode(&ProcessRules);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocCreateProcessRulesNode failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}

	ProcessRules->Access = POC_PR_ACCESS_READWRITE;

	Status = PocSymbolLinkPathToDosPath(POC_POCUSER_PATH, ProcessRules->ProcessName);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocSymbolLinkPathToDosPath failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}


	Status = PocCreateProcessRulesNode(&ProcessRules);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocCreateProcessRulesNode failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}

	ProcessRules->Access = POC_PR_ACCESS_READWRITE;

	Status = PocSymbolLinkPathToDosPath(POC_EXPLORER_PATH, ProcessRules->ProcessName);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocSymbolLinkPathToDosPath failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}



	/*
	* ����Ľ��̰�����ӣ�ע�⣬��Ҫ�������·��
	*/


	Status = PocCreateProcessRulesNode(&ProcessRules);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocCreateProcessRulesNode failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}

	ProcessRules->Access = POC_PR_ACCESS_READWRITE;

	Status = PocSymbolLinkPathToDosPath(POC_NOTEPAD_PATH, ProcessRules->ProcessName);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocSymbolLinkPathToDosPath failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}



	Status = PocCreateProcessRulesNode(&ProcessRules);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocCreateProcessRulesNode failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}

	ProcessRules->Access = POC_PR_ACCESS_READWRITE;

	Status = PocSymbolLinkPathToDosPath(POC_VSCODE_PATH, ProcessRules->ProcessName);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocSymbolLinkPathToDosPath failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}


	Status = PocCreateProcessRulesNode(&ProcessRules);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocCreateProcessRulesNode failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}

	ProcessRules->Access = POC_PR_ACCESS_READWRITE;

	Status = PocSymbolLinkPathToDosPath(POC_NOTEPADPLUS_PATH, ProcessRules->ProcessName);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocSymbolLinkPathToDosPath failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}


	Status = PocCreateProcessRulesNode(&ProcessRules);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocCreateProcessRulesNode failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}

	ProcessRules->Access = POC_PR_ACCESS_READWRITE;

	Status = PocSymbolLinkPathToDosPath(POC_WPS_PATH, ProcessRules->ProcessName);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocSymbolLinkPathToDosPath failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}


	Status = PocCreateProcessRulesNode(&ProcessRules);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocCreateProcessRulesNode failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}

	ProcessRules->Access = POC_PR_ACCESS_READWRITE;

	Status = PocSymbolLinkPathToDosPath(POC_WPP_PATH, ProcessRules->ProcessName);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocSymbolLinkPathToDosPath failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}


	Status = PocCreateProcessRulesNode(&ProcessRules);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocCreateProcessRulesNode failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}

	ProcessRules->Access = POC_PR_ACCESS_READWRITE;

	Status = PocSymbolLinkPathToDosPath(POC_ET_PATH, ProcessRules->ProcessName);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocSymbolLinkPathToDosPath failed. Status = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}



	return Status;

EXIT:

	PocProcessRulesListCleanup();

	return Status;
}


VOID PocProcessRulesListCleanup()
{

	PPOC_PROCESS_RULES ProcessRules = NULL;
	PLIST_ENTRY pListEntry = { 0 };

	PPOC_CREATED_PROCESS_INFO CreatedProcessInfo = NULL;
	PLIST_ENTRY pCreatedProcessListEntry = { 0 };

	while (NULL != PocProcessRulesListHead.Flink
		&& NULL != PocProcessRulesListSpinLock
		&& !IsListEmpty(&PocProcessRulesListHead))
	{

		pListEntry = ExInterlockedRemoveHeadList(
			&PocProcessRulesListHead,
			PocProcessRulesListSpinLock);

		ProcessRules = CONTAINING_RECORD(pListEntry, POC_PROCESS_RULES, ListEntry);

		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->remove prlist node processname = %ws.\n", __FUNCTION__, ProcessRules->ProcessName));


		while (NULL != ProcessRules &&
			NULL != ProcessRules->PocCreatedProcessListHead.Flink &&
			!IsListEmpty(&ProcessRules->PocCreatedProcessListHead))
		{

			pCreatedProcessListEntry = ExInterlockedRemoveHeadList(
				&ProcessRules->PocCreatedProcessListHead,
				&ProcessRules->PocCreatedProcessListSpinLock);

			CreatedProcessInfo = CONTAINING_RECORD(pCreatedProcessListEntry, POC_CREATED_PROCESS_INFO, ListEntry);

			PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->remove cpilist node processid = %I64d.\n", __FUNCTION__, (LONGLONG)CreatedProcessInfo->ProcessId));

			if (NULL != CreatedProcessInfo)
			{
				ExFreePoolWithTag(CreatedProcessInfo, POC_PR_LIST_TAG);
				CreatedProcessInfo = NULL;
			}
		}


		if (NULL != ProcessRules && NULL != ProcessRules->ProcessName)
		{
			ExFreePoolWithTag(ProcessRules->ProcessName, POC_PR_LIST_TAG);
			ProcessRules->ProcessName = NULL;
		}

		if (NULL != ProcessRules)
		{
			ExFreePoolWithTag(ProcessRules, POC_PR_LIST_TAG);
			ProcessRules = NULL;
		}
	}

	if (NULL != PocProcessRulesListSpinLock)
	{
		ExFreePoolWithTag(PocProcessRulesListSpinLock, POC_PR_LIST_TAG);
		PocProcessRulesListSpinLock = NULL;
	}
}


NTSTATUS PocCreateProcessRulesNode(
	OUT PPOC_PROCESS_RULES* OutProcessRules)
{

	NTSTATUS Status = 0;

	PPOC_PROCESS_RULES ProcessRules = NULL;

	ProcessRules = ExAllocatePoolWithTag(NonPagedPool, sizeof(POC_PROCESS_RULES), POC_PR_LIST_TAG);

	if (NULL == ProcessRules)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ExAllocatePoolWithTag ProcessRules failed.\n", __FUNCTION__));
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto EXIT;
	}

	RtlZeroMemory(ProcessRules, sizeof(POC_PROCESS_RULES));

	ProcessRules->ProcessName = ExAllocatePoolWithTag(NonPagedPool, POC_MAX_NAME_LENGTH * sizeof(WCHAR), POC_PR_LIST_TAG);

	if (NULL == ProcessRules->ProcessName)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ExAllocatePoolWithTag ProcessName failed.\n", __FUNCTION__));
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto EXIT;
	}

	RtlZeroMemory(ProcessRules->ProcessName, POC_MAX_NAME_LENGTH * sizeof(WCHAR));


	InitializeListHead(&ProcessRules->PocCreatedProcessListHead);
	KeInitializeSpinLock(&ProcessRules->PocCreatedProcessListSpinLock);

	ExInterlockedInsertTailList(
		&PocProcessRulesListHead,
		&ProcessRules->ListEntry,
		PocProcessRulesListSpinLock);


	*OutProcessRules = ProcessRules;

	return Status;

EXIT:

	if (NULL != ProcessRules && NULL != ProcessRules->ProcessName)
	{
		ExFreePoolWithTag(ProcessRules->ProcessName, POC_PR_LIST_TAG);
		ProcessRules->ProcessName = NULL;
	}

	if (NULL != ProcessRules)
	{
		ExFreePoolWithTag(ProcessRules, POC_PR_LIST_TAG);
		ProcessRules = NULL;
	}

	if (NULL != PocProcessRulesListSpinLock)
	{
		ExFreePoolWithTag(PocProcessRulesListSpinLock, POC_PR_LIST_TAG);
		PocProcessRulesListSpinLock = NULL;
	}


	return Status;
}


NTSTATUS PocCreateProcessInfoNode(
	IN PPOC_PROCESS_RULES ProcessRules,
	OUT PPOC_CREATED_PROCESS_INFO* OutProcessInfo)
{

	if (NULL == ProcessRules)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ProcessRules is NULL.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}


	NTSTATUS Status = 0;

	PPOC_CREATED_PROCESS_INFO ProcessInfo = NULL;

	ProcessInfo = ExAllocatePoolWithTag(NonPagedPool, sizeof(POC_CREATED_PROCESS_INFO), POC_PR_LIST_TAG);

	if (NULL == ProcessInfo)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ExAllocatePoolWithTag ProcessInfo failed.\n", __FUNCTION__));
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto EXIT;
	}

	RtlZeroMemory(ProcessInfo, sizeof(POC_CREATED_PROCESS_INFO));

	ProcessInfo->OwnedProcessRule = ProcessRules;

	ExInterlockedInsertTailList(
		&ProcessRules->PocCreatedProcessListHead,
		&ProcessInfo->ListEntry,
		&ProcessRules->PocCreatedProcessListSpinLock);


	*OutProcessInfo = ProcessInfo;

	return Status;

EXIT:
	if (NULL != ProcessInfo)
	{
		ExFreePoolWithTag(ProcessInfo, POC_PR_LIST_TAG);
		ProcessInfo = NULL;
	}

	return Status;
}


NTSTATUS PocFindProcessRulesNodeByName(
	IN PWCHAR ProcessName,
	OUT PPOC_PROCESS_RULES* OutProcessRules,
	IN BOOLEAN Remove)
{
	if (NULL == ProcessName)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ProcessName is null.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	PPOC_PROCESS_RULES ProcessRules = { 0 };
	PLIST_ENTRY pListEntry = PocProcessRulesListHead.Flink;
	KIRQL OldIrql = 0;

	PPOC_CREATED_PROCESS_INFO CreatedProcessInfo = NULL;
	PLIST_ENTRY pCreatedProcessListEntry = { 0 };

	while (pListEntry != &PocProcessRulesListHead)
	{

		ProcessRules = CONTAINING_RECORD(pListEntry, POC_PROCESS_RULES, ListEntry);

		if (!_wcsnicmp(ProcessRules->ProcessName, ProcessName, wcslen(ProcessRules->ProcessName)))
		{

			if (Remove)
			{
				PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->Remove process = %ws Access = %d.\n",
					__FUNCTION__,
					ProcessRules->ProcessName,
					ProcessRules->Access));

				while (NULL != ProcessRules &&
					NULL != ProcessRules->PocCreatedProcessListHead.Flink &&
					!IsListEmpty(&ProcessRules->PocCreatedProcessListHead))
				{

					pCreatedProcessListEntry = ExInterlockedRemoveHeadList(
						&ProcessRules->PocCreatedProcessListHead,
						&ProcessRules->PocCreatedProcessListSpinLock);

					CreatedProcessInfo = CONTAINING_RECORD(pCreatedProcessListEntry, POC_CREATED_PROCESS_INFO, ListEntry);

					PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->Remove cpilist node processid = %I64d.\n", __FUNCTION__, (LONGLONG)CreatedProcessInfo->ProcessId));

					if (NULL != CreatedProcessInfo)
					{
						ExFreePoolWithTag(CreatedProcessInfo, POC_PR_LIST_TAG);
						CreatedProcessInfo = NULL;
					}
				}

				OldIrql = KeAcquireSpinLockRaiseToDpc(PocProcessRulesListSpinLock);

				RemoveEntryList(&ProcessRules->ListEntry);

				KeReleaseSpinLock(PocProcessRulesListSpinLock, OldIrql);

				if (NULL != ProcessRules->ProcessName)
				{
					ExFreePoolWithTag(ProcessRules->ProcessName, POC_PR_LIST_TAG);
					ProcessRules->ProcessName = NULL;
				}

				if (NULL != ProcessRules)
				{
					ExFreePoolWithTag(ProcessRules, POC_PR_LIST_TAG);
					ProcessRules = NULL;
				}
			}
			else
			{
				if (OutProcessRules != NULL)
				{
					*OutProcessRules = ProcessRules;
				}
			}

			return STATUS_SUCCESS;
		}

		pListEntry = pListEntry->Flink;
	}

	return STATUS_UNSUCCESSFUL;
}


NTSTATUS PocFindProcessInfoNodeByPid(
	IN HANDLE ProcessId,
	IN PPOC_PROCESS_RULES ProcessRules,
	OUT PPOC_CREATED_PROCESS_INFO* OutProcessInfo,
	IN BOOLEAN Remove)
{
	if (NULL == ProcessId)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ProcessId is null.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	if (NULL == ProcessRules)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ProcessRules is NULL.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	PPOC_CREATED_PROCESS_INFO ProcessInfo = { 0 };
	PLIST_ENTRY pListEntry = ProcessRules->PocCreatedProcessListHead.Flink;
	KIRQL OldIrql = 0;

	while (pListEntry != &ProcessRules->PocCreatedProcessListHead)
	{

		ProcessInfo = CONTAINING_RECORD(pListEntry, POC_CREATED_PROCESS_INFO, ListEntry);

		if (ProcessInfo->ProcessId == ProcessId)
		{

			if (Remove)
			{
				/*PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->Remove ProcessId = %I64d ProcessRules = %p.\n",
					__FUNCTION__,
					(LONGLONG)ProcessInfo->ProcessId,
					ProcessInfo->OwnedProcessRule));*/

				OldIrql = KeAcquireSpinLockRaiseToDpc(&ProcessRules->PocCreatedProcessListSpinLock);

				RemoveEntryList(&ProcessInfo->ListEntry);

				KeReleaseSpinLock(&ProcessRules->PocCreatedProcessListSpinLock, OldIrql);

				if (NULL != ProcessInfo)
				{
					ExFreePoolWithTag(ProcessInfo, POC_PR_LIST_TAG);
					ProcessInfo = NULL;
				}
			}
			else
			{
				if (OutProcessInfo != NULL)
				{
					*OutProcessInfo = ProcessInfo;
				}
			}

			return STATUS_SUCCESS;
		}

		pListEntry = pListEntry->Flink;
	}

	return STATUS_UNSUCCESSFUL;
}


NTSTATUS PocFindProcessInfoNodeByPidEx(
	IN HANDLE ProcessId,
	OUT PPOC_CREATED_PROCESS_INFO* OutProcessInfo,
	IN BOOLEAN Remove,
	IN BOOLEAN IntegrityCheck)
{
	if (NULL == ProcessId && FALSE == IntegrityCheck)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ProcessId is null.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS Status = 0;

	PPOC_PROCESS_RULES ProcessRules = { 0 };
	PLIST_ENTRY pListEntry = PocProcessRulesListHead.Flink;

	PPOC_CREATED_PROCESS_INFO ProcessInfo = { 0 };
	PLIST_ENTRY pProcessInfoListEntry = NULL;
	KIRQL OldIrql = 0;

	PEPROCESS EProcess = NULL;

	while (pListEntry != &PocProcessRulesListHead)
	{

		ProcessRules = CONTAINING_RECORD(pListEntry, POC_PROCESS_RULES, ListEntry);

		pProcessInfoListEntry = ProcessRules->PocCreatedProcessListHead.Flink;

		while (pProcessInfoListEntry != &ProcessRules->PocCreatedProcessListHead)
		{

			ProcessInfo = CONTAINING_RECORD(pProcessInfoListEntry, POC_CREATED_PROCESS_INFO, ListEntry);



			if (IntegrityCheck)
			{
				Status = PsLookupProcessByProcessId(
					ProcessInfo->ProcessId,
					&EProcess);

				if (STATUS_SUCCESS != Status)
				{
					PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PsLookupProcessByProcessId ProcessId = %I64d failed. Status = 0x%x.\n",
						__FUNCTION__,
						(LONGLONG)ProcessInfo->ProcessId,
						Status));
					
					pProcessInfoListEntry = pProcessInfoListEntry->Flink;

					continue;
				}

				Status = PocProcessIntegrityCheck(EProcess);

				if (NULL != EProcess)
				{
					ObDereferenceObject(EProcess);
					EProcess = NULL;
				}
			}
			
			if (FALSE == IntegrityCheck && ProcessInfo->ProcessId == ProcessId)
			{

				if (Remove)
				{
					PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->Remove ProcessId = %I64d ProcessRules = %p.\n",
						__FUNCTION__,
						(LONGLONG)ProcessInfo->ProcessId,
						ProcessInfo->OwnedProcessRule));

					OldIrql = KeAcquireSpinLockRaiseToDpc(&ProcessRules->PocCreatedProcessListSpinLock);

					RemoveEntryList(&ProcessInfo->ListEntry);

					KeReleaseSpinLock(&ProcessRules->PocCreatedProcessListSpinLock, OldIrql);

					if (NULL != ProcessInfo)
					{
						ExFreePoolWithTag(ProcessInfo, POC_PR_LIST_TAG);
						ProcessInfo = NULL;
					}
				}
				else
				{
					if (OutProcessInfo != NULL)
					{
						*OutProcessInfo = ProcessInfo;
					}
				}

				return STATUS_SUCCESS;
			}

			pProcessInfoListEntry = pProcessInfoListEntry->Flink;
		}

		pListEntry = pListEntry->Flink;
	}


	return STATUS_UNSUCCESSFUL;
}


NTSTATUS PocIsUnauthorizedProcess(IN PWCHAR ProcessName)
{
	if (NULL == ProcessName)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ProcessName is NULL.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS Status = 0;
	PPOC_PROCESS_RULES OutProcessRules = NULL;

	/*
	* ���������жϽ��̵�Ȩ��
	*/
	Status = PocFindProcessRulesNodeByName(
		ProcessName,
		&OutProcessRules,
		FALSE);


	if (STATUS_SUCCESS == Status &&
		NULL != OutProcessRules &&
		POC_PR_ACCESS_READWRITE == OutProcessRules->Access)
	{
		//PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,("%s->Auth:: %ws\n", __FUNCTION__, ProcessName));
		return POC_IS_AUTHORIZED_PROCESS;
	}
	else
	{
		//PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,("%s->UnAuth:: %ws\n", __FUNCTION__, ProcessName));
		return POC_IS_UNAUTHORIZED_PROCESS;
	}

}
