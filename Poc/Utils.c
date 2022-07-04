
#include "utils.h"
#include "global.h"
#include "context.h"
#include "import.h"
#include "filefuncs.h"
#include <wdm.h>


NTSTATUS PocGetFileNameOrExtension(
	IN PFLT_CALLBACK_DATA Data,
	IN OUT PWCHAR FileExtension,
	IN OUT PWCHAR FileName)
{

	NTSTATUS Status;
	PFLT_FILE_NAME_INFORMATION FileNameInfo = NULL;

	Status = FltGetFileNameInformation(
		Data,
		FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP,
		&FileNameInfo);

	if (!NT_SUCCESS(Status))
	{
		if (STATUS_FLT_NAME_CACHE_MISS == Status)
		{
			PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocGetFileExtension->FltGetFileNameInformation failed. Status = STATUS_FLT_NAME_CACHE_MISS\n"));
		}
		else
		{
			// PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocGetFileExtension->FltGetFileNameInformation failed. Status = 0x%x\n", Status));
		}
		return Status;
	}

	Status = FltParseFileNameInformation(FileNameInfo);

	if (STATUS_SUCCESS != Status)
	{
		goto EXIT;
	}

	if (NULL != FileExtension &&
		NULL != FileNameInfo->Extension.Buffer &&
		wcslen(FileNameInfo->Extension.Buffer) < POC_MAX_NAME_LENGTH)
	{
		RtlMoveMemory(FileExtension, FileNameInfo->Extension.Buffer, wcslen(FileNameInfo->Extension.Buffer) * sizeof(WCHAR));
	}

	if (NULL != FileName &&
		NULL != FileNameInfo->Name.Buffer &&
		wcslen(FileNameInfo->Name.Buffer) < POC_MAX_NAME_LENGTH)
	{
		RtlMoveMemory(FileName, FileNameInfo->Name.Buffer, wcslen(FileNameInfo->Name.Buffer) * sizeof(WCHAR));
	}

	// PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocGetFileExtension->FileName is %ws.\n", FileNameInfo->Name.Buffer);

EXIT:
	if (NULL != FileNameInfo)
	{
		FltReleaseFileNameInformation(FileNameInfo);
		FileNameInfo = NULL;
	}

	return Status;
}


LONGLONG PocQueryEndOfFileInfo(
	IN PFLT_INSTANCE Instance,
	IN PFILE_OBJECT FileObject)
{

	FILE_STANDARD_INFORMATION StandardInfo = {0};
	ULONG LengthReturned = 0;
	NTSTATUS Status;

	Status = FltQueryInformationFile(Instance, FileObject, &StandardInfo, sizeof(FILE_STANDARD_INFORMATION), FileStandardInformation, &LengthReturned);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocQueryEndOfFileInfo->FltQueryInformationFile failed. Status = 0x%x.\n", Status));
		return 0;
	}

	return StandardInfo.EndOfFile.QuadPart;
}


NTSTATUS PocSetEndOfFileInfo(
	IN PFLT_INSTANCE Instance,
	IN PFILE_OBJECT FileObject,
	IN LONGLONG FileSize)
{
	FILE_END_OF_FILE_INFORMATION EndOfFileInfo = {0};
	NTSTATUS Status;

	EndOfFileInfo.EndOfFile.QuadPart = FileSize;

	Status = FltSetInformationFile(Instance, FileObject, &EndOfFileInfo, sizeof(FILE_END_OF_FILE_INFORMATION), FileEndOfFileInformation);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocQueryEndOfFileInfo->FltSetInformationFile failed. Status = 0x%x.\n", Status));
	}

	return Status;
}


USHORT PocQueryVolumeSectorSize(IN PFLT_VOLUME Volume)
{
	// Therefore, a minifilter driver commonly calls this routine from a post-mount callback function
	// or an InstanceSetupCallback (PFLT_INSTANCE_SETUP_CALLBACK) routine to determine whether to attach to a given volume.

	UCHAR VolPropBuffer[sizeof(FLT_VOLUME_PROPERTIES) + 512] = {0};
	PFLT_VOLUME_PROPERTIES VolProp = (PFLT_VOLUME_PROPERTIES)VolPropBuffer;
	ULONG LengthReturned = 0;
	NTSTATUS Status;

	Status = FltGetVolumeProperties(Volume, VolProp, sizeof(VolPropBuffer), &LengthReturned);

	if (STATUS_SUCCESS != Status)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocQueryVolumeSectorSize->FltGetVolumeProperties failed. Status = 0x%x.\n", Status));
		return 0;
	}

	return max(VolProp->SectorSize, MIN_SECTOR_SIZE);
}


NTSTATUS PocParseFileNameExtension(
	IN PWCHAR FileName,
	IN OUT PWCHAR FileExtension)
{
	if (NULL == FileName)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocParseFileNameExtension->FileName is NULL.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	if (NULL == FileExtension)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocParseFileNameExtension->FileExtension is NULL.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	PWCHAR lpExtension = NULL;

	lpExtension = FileName + wcslen(FileName);

	while (lpExtension > FileName)
	{
		if (*lpExtension == L'.' && wcslen(lpExtension) < POC_MAX_NAME_LENGTH)
		{
			lpExtension++;
			RtlMoveMemory(FileExtension, lpExtension, wcslen(lpExtension) * sizeof(WCHAR));
			return STATUS_SUCCESS;
		}

		lpExtension--;
	}

	return STATUS_UNSUCCESSFUL;
}


NTSTATUS PocBypassIrrelevantFileExtension(
	IN PWCHAR FileExtension)
/*
 * ���˵���Ŀ����չ���ļ�
 */
{

	if (NULL == FileExtension)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocBypassIrrelevantFileExtension->FileExtension is NULL.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	for (int i = 0; i < secure_extension_count; i++)
	{
		if (0 == _wcsnicmp(FileExtension, secure_extension[i], 
			wcslen(FileExtension) > wcslen(secure_extension[i]) ? wcslen(FileExtension) : wcslen(secure_extension[i])))
		{
			return POC_IS_TARGET_FILE_EXTENSION;
		}
	}

	return POC_IRRELEVENT_FILE_EXTENSION;
}


NTSTATUS PocQuerySymbolicLink(
	IN PUNICODE_STRING SymbolicLinkName,
	OUT PUNICODE_STRING LinkTarget)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	OBJECT_ATTRIBUTES ObjectAttributes = {0};
	HANDLE LinkHandle = NULL;

	InitializeObjectAttributes(
		&ObjectAttributes,
		SymbolicLinkName,
		OBJ_CASE_INSENSITIVE,
		0,
		0);

	Status = ZwOpenSymbolicLinkObject(&LinkHandle, GENERIC_READ, &ObjectAttributes);

	if (!NT_SUCCESS(Status))
	{
		//PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocQuerySymbolicLink->ZwOpenSymbolicLinkObject1 failed. Status = 0x%x.\n", Status));
		goto EXIT;
	}

	LinkTarget->MaximumLength = 260 * sizeof(WCHAR);
	LinkTarget->Length = 0;
	LinkTarget->Buffer = ExAllocatePoolWithTag(NonPagedPool, LinkTarget->MaximumLength, DOS_NAME_BUFFER_TAG);

	if (NULL == LinkTarget->Buffer)
	{
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto EXIT;
	}

	RtlZeroMemory(LinkTarget->Buffer, LinkTarget->MaximumLength);

	Status = ZwQuerySymbolicLinkObject(LinkHandle, LinkTarget, NULL);

	if (!NT_SUCCESS(Status))
	{
		//PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocQuerySymbolicLink->ZwOpenSymbolicLinkObject2 failed. Status = 0x%x.\n", Status));
		ExFreePoolWithTag(LinkTarget->Buffer, DOS_NAME_BUFFER_TAG);
		goto EXIT;
	}

	// �����е�'/'תΪ'\\'
	if (NULL == LinkTarget->Buffer)
	{
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto EXIT;
	}

	wchar_t *p = wcsstr(LinkTarget->Buffer, L"/");
	while (p)
	{
		*p = L'\\';
		p = wcsstr(p + 1, L"/");
	}
	

	Status = STATUS_SUCCESS;

EXIT:

	if (NULL != LinkHandle)
	{
		ZwClose(LinkHandle);
		LinkHandle = NULL;
	}

	return Status;
}


NTSTATUS PocGetVolumeInstance(
	IN PFLT_FILTER pFilter,
	IN PUNICODE_STRING pVolumeName,
	OUT PFLT_INSTANCE *Instance)
/*
 * �õ���Ӧ���ʵ��
 * pVolumeName->BufferӦ����ΪDos����ʾ�� L"\\Device\\HarddiskVolume2"
 */
{
	NTSTATUS Status;
	PFLT_INSTANCE pInstance = NULL;
	PFLT_VOLUME pVolumeList[100];
	ULONG uRet;
	UNICODE_STRING uniName = {0};
	ULONG index = 0;
	WCHAR wszNameBuffer[POC_MAX_NAME_LENGTH] = {0};

	Status = FltEnumerateVolumes(pFilter,
								 NULL,
								 0,
								 &uRet);
	if (Status != STATUS_BUFFER_TOO_SMALL)
	{
		return Status;
	}

	Status = FltEnumerateVolumes(pFilter,
								 pVolumeList,
								 uRet,
								 &uRet);

	if (!NT_SUCCESS(Status))
	{
		return Status;
	}
	uniName.Buffer = wszNameBuffer;

	if (uniName.Buffer == NULL)
	{
		for (index = 0; index < uRet; index++)
			FltObjectDereference(pVolumeList[index]);

		return STATUS_UNSUCCESSFUL;
	}

	uniName.MaximumLength = sizeof(wszNameBuffer);

	for (index = 0; index < uRet; index++)
	{
		uniName.Length = 0;

		Status = FltGetVolumeName(pVolumeList[index],
								  &uniName,
								  NULL);

		if (!NT_SUCCESS(Status))
			continue;

		if (RtlCompareUnicodeString(&uniName,
									pVolumeName,
									TRUE) != 0)
			continue;

		Status = FltGetVolumeInstanceFromName(pFilter,
											  pVolumeList[index],
											  NULL,
											  &pInstance);

		if (NT_SUCCESS(Status))
		{
			FltObjectDereference(pInstance);
			break;
		}
	}

	for (index = 0; index < uRet; index++)
	{
		FltObjectDereference(pVolumeList[index]);
	}

	*Instance = pInstance;

	return Status;
}


NTSTATUS PocSymbolLinkPathToDosPath(
	IN PWCHAR Path,
	IN OUT PWCHAR DosPath)
{
	if (NULL == Path)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->Path is NULL.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	if (NULL == DosPath)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->DosPath is NULL.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS Status = 0;

	UNICODE_STRING uSymbolLinkName = {0};
	WCHAR wSymbolLinkName[POC_MAX_NAME_LENGTH] = {0};

	PWCHAR lpPath = wSymbolLinkName;

	UNICODE_STRING uDosName = {0};

	RtlMoveMemory(wSymbolLinkName, L"\\??\\", wcslen(L"\\??\\") * sizeof(WCHAR));
	RtlMoveMemory(wSymbolLinkName + wcslen(wSymbolLinkName), Path, wcslen(Path) * sizeof(WCHAR));

	while (*lpPath != L':' &&
		   lpPath < wSymbolLinkName + wcslen(wSymbolLinkName))
	{
		lpPath++;
	}

	RtlZeroMemory(lpPath + 1, wcslen(lpPath + 1) * sizeof(WCHAR));

	RtlInitUnicodeString(&uSymbolLinkName, wSymbolLinkName);
	uSymbolLinkName.MaximumLength = sizeof(wSymbolLinkName);

	Status = PocQuerySymbolicLink(
		&uSymbolLinkName,
		&uDosName);

	if (STATUS_SUCCESS != Status || NULL == uDosName.Buffer)
	{
		//PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocQuerySymbolicLink failed ststus = 0x%x.\n", __FUNCTION__, Status));
		goto EXIT;
	}

	RtlMoveMemory(DosPath, uDosName.Buffer, wcslen(uDosName.Buffer) * sizeof(WCHAR));

	RtlMoveMemory(DosPath + wcslen(DosPath), Path + wcslen(L"C:"), wcslen(Path + wcslen(L"C:")) * sizeof(WCHAR));

	Status = STATUS_SUCCESS;

EXIT:
	if (NULL != uDosName.Buffer)
	{
		ExFreePool(uDosName.Buffer);
		uDosName.Buffer = NULL;
	}

	return Status;
}


NTSTATUS PocAnsi2Unicode(const char* ansi, wchar_t* unicode, int unicode_size)
/*---------------------------------------------------------
��������:	PocAnsi2Unicode
��������:	��ANSI�ַ���תΪUNICODE
����:		wangzhankun
ʱ�䣺		2022.06.01
����ά��:
---------------------------------------------------------*/
{
	//����C6271	��DbgPrint�������˶������ : _Param_(3) δ�ɸ�ʽ�ַ���ʹ��

	//POC_IS_PARAMETER_NULL(ansi);
	//POC_IS_PARAMETER_NULL(unicode);

	if (NULL == ansi)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ansi is NULL.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	if (NULL == unicode)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->unicode is NULL.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	ANSI_STRING ansi_str;
	UNICODE_STRING unicode_str;

	ansi_str.Length = ansi_str.MaximumLength = (USHORT)strlen(ansi);
	ansi_str.Buffer = (PCHAR)ansi;

	unicode_str.Length = 0;
	unicode_str.MaximumLength = (USHORT)(unicode_size * sizeof(wchar_t));
	unicode_str.Buffer = unicode;

	return RtlAnsiStringToUnicodeString(&unicode_str, &ansi_str, FALSE); // PASSIVE_LEVEL
}


NTSTATUS PocAnyPath2DosPath(const PWCHAR src_path, PWCHAR dest_path, const size_t max_len_dest_path)
/*---------------------------------------------------------
��������:	PocAnyPath2DosPath
��������:	���κθ�ʽ��·��ת��ΪDos·��
����:		wangzhankun
ʱ�䣺		2022.06.01
����ά��:	hkx3upper���wcsstr��Сд��������
---------------------------------------------------------*/
{
	if (NULL == src_path)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocAnyPath2DosPath->src_path is NULL.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	if (NULL == dest_path)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocAnyPath2DosPath->dest_path is NULL.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	RtlZeroMemory(dest_path, max_len_dest_path);

	NTSTATUS Status = STATUS_SUCCESS;

	/* 
	* �ж� src_path��·����ʽ��ȷ��dest_path��dos��ʽ��·����
	* PocQuerySymbolicLink����ֻ��ת��\\??\\C:�������ͣ�
	* ����ת��\\??\\C:\Desktop���־����ĳ���ļ���
	*/
	if (wcsstr(src_path, L"\\??\\") != NULL)
	{
		WCHAR TempFileName[POC_MAX_NAME_LENGTH] = { 0 };

		wcsncpy(TempFileName, src_path + wcslen(L"\\??\\"), wcslen(src_path + wcslen(L"\\??\\")));

		Status = PocSymbolLinkPathToDosPath(TempFileName, dest_path);

		if (Status != STATUS_SUCCESS)
		{
			PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
				("%s->PocSymbolLinkPathToDosPath src_path = %ws failed. Status = 0x%x.\n",
					__FUNCTION__, src_path, Status));
			goto EXIT;
		}

	}
	else if (_wcsnicmp(src_path, L"\\Device\\HarddiskVolume", wcslen(L"\\Device\\HarddiskVolume")) == 0 ||
		_wcsnicmp(src_path, L"\\Device\\Mup", wcslen(L"\\Device\\Mup")) == 0)
	{
		RtlMoveMemory(dest_path, src_path, wcslen(src_path) * sizeof(WCHAR));
	}
	else
	{
		Status = PocSymbolLinkPathToDosPath(src_path, dest_path);

		if (Status != STATUS_SUCCESS)
		{
			PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
				("%s->PocSymbolLinkPathToDosPath src_path = %ws failed. Status = 0x%x.\n",
					__FUNCTION__, src_path, Status));
			goto EXIT;
		}
	}
	

	
	LONGLONG len = wcslen(dest_path);
	if (len == 0 || len * sizeof(WCHAR) > max_len_dest_path)
	{
		Status = STATUS_UNSUCCESSFUL;
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->dos_src_path is invalid, the length is %I64d.\n", __FUNCTION__, len));
		goto EXIT;
	}
	

	// �����е�'/'תΪ'\\'
	wchar_t* p = wcsstr(dest_path, L"/");
	while (p)
	{
		*p = L'\\';
		p = wcsstr(p + 1, L"/");
	}
	

EXIT:
	return Status;
}


NTSTATUS PocAddOrFindRelevantPath(IN CONST PWCHAR folder_name, BOOLEAN find_relevant_path)
/*---------------------------------------------------------
��������:	PocAddOrFindRelevantPath
��������:	����·������ӻ��ѯ����
����:		wangzhankun
ʱ�䣺		2022.06.01
����ά��:
---------------------------------------------------------*/
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	/*
	* �������C:\Desktop�������C:\Desktop\Debug��
	* �����������ʶһ�£��Ա�current_relevant_path_inx++��
	* ������û����ģ���������һ�������Ժ󣬻�������������Ҫ����һ��
	*/
	BOOLEAN IamConfused = TRUE;

	/*
	* Ŀǰ���ֻ�����256�������ļ���·��
	*/
	Status = STATUS_INVALID_PARAMETER;
	if (current_relevant_path_inx == 256)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	if (folder_name != NULL)
	{

		RtlZeroMemory(RelevantPath[current_relevant_path_inx], sizeof(RelevantPath[current_relevant_path_inx]));
		wcsncpy(RelevantPath[current_relevant_path_inx], folder_name, wcslen(folder_name));

		//���*p�����һ���ַ�����'\\'����'/'����Ҫ����'\\'����Ϊ���ļ���·������ֹ����ƥ���������
		size_t len = wcslen(RelevantPath[current_relevant_path_inx]);
		if (RelevantPath[current_relevant_path_inx][len - 1] != L'\\')
		{
			wcscat(RelevantPath[current_relevant_path_inx], L"\\");
		}
		
		
		// �ж�folder_name�Ƿ��ǻ����ļ��е�·���������Ƿ�����ӻ��ǲ��Ҷ����Ƚ��в���
		// �����ӵ��ļ����Ѿ��ǻ���·���Ļ����ǾͲ���Ҫ�����
		Status = POC_IS_IRRELEVENT_PATH;
		PWCHAR lpFileName = RelevantPath[current_relevant_path_inx];


		for (ULONG i = 0; i < current_relevant_path_inx; i++)
		{
			if (wcslen(RelevantPath[i]) > wcslen(lpFileName))
			{
				continue;
			}

			if (wcsncmp(RelevantPath[i], lpFileName, wcslen(lpFileName)) == 0)
			{
				IamConfused = FALSE;
			}

			int j = 0;
			len = wcslen(RelevantPath[i]);
			for (; j < len; ++j)
			{
				if (RelevantPath[i][j] != lpFileName[j])
				{
					// ��PocAnyPath2DosPath���Ѿ������õ�'/'ת��Ϊ��'\\'���������ﲻ��Ҫ����
					break;
				}
			}
			if (len == j)
			{
				Status = STATUS_SUCCESS;
				break;
			}
		}
		
		// TODO �����ظ���ӵ�·����Ҫ���ز�ͬ�ķ���ֵ
		if (find_relevant_path == FALSE && //��Ҫ��ӻ���·��
			IamConfused)
		{
			current_relevant_path_inx++;
			Status = STATUS_SUCCESS;
		}
	}

	return Status;
}


NTSTATUS PocBypassRelevantPath(IN PWCHAR FileName)
/**
 * @brief ���ʱ����·���򷵻�ֵδ STATUS_SUCCESS �����򷵻�ֵΪ POC_IS_IRRELEVENT_PATH
 */
{

	if (NULL == FileName)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocBypassWordBackupFile->FileName is NULL.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS Status = PocAddOrFindRelevantPath(FileName, TRUE);

	return Status;
}


NTSTATUS PocAddSecureExtensionW(IN CONST PWCHAR extension)
/*---------------------------------------------------------
��������:	PocAddSecureExtensionW
��������:	�������˵��ļ���չ�����ַ�����
����:		wangzhankun
ʱ�䣺		2022.06.01
����ά��:
---------------------------------------------------------*/
{
	if (extension == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}
	// �������ظ����
	if (PocBypassIrrelevantFileExtension(extension) == POC_IS_TARGET_FILE_EXTENSION)
	{
		return POC_OBJECT_REPEAT;
	}

	if (secure_extension_count >= MAX_SECURE_EXTENSION_COUNT)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocAddSecureExtensionW->secure_extension_count >= MAX_SECURE_EXTENSION_COUNT.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlMoveMemory(secure_extension[secure_extension_count], extension, wcslen(extension) * sizeof(WCHAR));
	secure_extension_count++;
	return STATUS_SUCCESS;
}


NTSTATUS PocAddSecureExtension(IN const PCHAR extension)
/*---------------------------------------------------------
��������:	PocAddSecureExtension
��������:   �������˵��ļ���չ��
����:		wangzhankun
ʱ�䣺		2022.06.01
����ά��:
---------------------------------------------------------*/
{
	//����C6271	��DbgPrint�������˶������ : _Param_(3) δ�ɸ�ʽ�ַ���ʹ��

	//POC_IS_PARAMETER_NULL(extension);

	if (NULL == extension)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->extension is NULL.\n", __FUNCTION__));
		return STATUS_INVALID_PARAMETER;
	}

	WCHAR w_extension[32];
	NTSTATUS Status = PocAnsi2Unicode(extension, w_extension, sizeof(w_extension));
	if (Status != STATUS_SUCCESS)
	{
		PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocAnsi2Unicode failed, status is %08x\n", __FUNCTION__, Status));
		return Status;
	}
	Status = PocAddSecureExtensionW(w_extension);
	return Status;
}


NTSTATUS PocBypassIrrelevantBy_PathAndExtension(
	IN PFLT_CALLBACK_DATA Data)
/*---------------------------------------------------------
��������:	PocBypassIrrelevantBy_PathAndExtension
��������:	�����ļ���չ���ͻ����ļ���
����:		wangzhankun
ʱ�䣺		2022.06.01
����ά��:
---------------------------------------------------------*/

{
	// TODO �Ľ�����ֵ�ı�ʾ����
	WCHAR FileName[POC_MAX_NAME_LENGTH] = { 0 };
	WCHAR FileExtension[POC_MAX_NAME_LENGTH] = { 0 };

	if (STATUS_SUCCESS == PocGetFileNameOrExtension(Data, FileExtension, FileName))
	{
		// TODO minifilter��PreCreate���������ļ�ϵͳ��Create���������ȹ��˼򵥵���չ����������ϣ��ٹ���·��
		if (PocBypassIrrelevantFileExtension(FileExtension) == POC_IS_TARGET_FILE_EXTENSION &&
			PocBypassRelevantPath(FileName) == STATUS_SUCCESS)
		{
			//PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s@%s@d%d: FileName is %ws.\n", __FUNCTION__, __FILE__, __LINE__, FileName));

			return POC_IS_TARGET_FILE_EXTENSION;

		}
	}
	// return POC_IS_IRRELEVANT_PATH_AND_EXTENSION;
	return POC_IRRELEVENT_FILE_EXTENSION;
}


NTSTATUS PocInitFolderAndExt()
{
	/*
	* ���������DriverEntry��ʼ�����Ա��������������֮��ˢ�²������������Щֵ��
	*/

	NTSTATUS Status = 0;

	RtlZeroMemory(RelevantPath, sizeof(RelevantPath));

	for (const PWCHAR* p = allowed_path; *p; p++)
	{
		Status = PocAnyPath2DosPath(*p, RelevantPath[current_relevant_path_inx], sizeof(RelevantPath[current_relevant_path_inx]));

		if (Status != STATUS_SUCCESS)
		{
			//PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s@%s@%d: PocAnyPath2DosPath failed, status is %08x\n", __FUNCTION__, __FILE__, __LINE__, Status));
			goto EXIT;
		}

		//���*p�����һ���ַ�����'\\'����'/'����Ҫ����'\\'����Ϊ���ļ���·������ֹ����ƥ���������
		size_t len = wcslen(RelevantPath[current_relevant_path_inx]);
		if (RelevantPath[current_relevant_path_inx][len - 1] != L'\\')
		{
			wcscat(RelevantPath[current_relevant_path_inx], L"\\");
		}

		current_relevant_path_inx++;
	}


	RtlZeroMemory(secure_extension, sizeof(secure_extension));

	while (allowed_extension[secure_extension_count])
	{
		RtlMoveMemory(secure_extension[secure_extension_count],
			allowed_extension[secure_extension_count],
			wcslen(allowed_extension[secure_extension_count]) * sizeof(WCHAR));
		secure_extension_count++;
	}

	Status = STATUS_SUCCESS;

EXIT:

	return Status;
}
