/*++

Module Name:

    Poc.c

Abstract:

    This is the main module of the Poc miniFilter driver.

Environment:

    Kernel mode

--*/

#include <fltKernel.h>
#include <dontuse.h>
#include "global.h"
#include "utils.h"
#include "context.h"
#include "filefuncs.h"
#include "cipher.h"
#include "fileobject.h"
#include "write.h"
#include "read.h"
#include "fileinfo.h"
#include "commport.h"
#include "process.h"
#include "processecure.h"
#include "Dpc.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


PFLT_FILTER gFilterHandle = NULL;
PDEVICE_OBJECT gDeviceObject = NULL;


/*************************************************************************
    Prototypes
*************************************************************************/

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS
PocUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
PocInstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

FLT_PREOP_CALLBACK_STATUS
PocPreCreateOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
PocPostCreateOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
PocPreCloseOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
PocPostCloseOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_POSTOP_CALLBACK_STATUS
PocPostCreateOperationWhenSafe(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_POSTOP_CALLBACK_STATUS
PocPostCloseOperationWhenSafe(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, PocUnload)
#pragma alloc_text(PAGE, PocInstanceSetup)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

#if 0 // TODO - List all of the requests to filter.
    { IRP_MJ_CREATE,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_CREATE_NAMED_PIPE,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_CLOSE,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_READ,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_WRITE,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_QUERY_INFORMATION,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_SET_INFORMATION,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_QUERY_EA,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_SET_EA,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_FLUSH_BUFFERS,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_QUERY_VOLUME_INFORMATION,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_SET_VOLUME_INFORMATION,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_DIRECTORY_CONTROL,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_FILE_SYSTEM_CONTROL,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_DEVICE_CONTROL,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_INTERNAL_DEVICE_CONTROL,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_SHUTDOWN,
      0,
      PocPreOperationNoPostOperation,
      NULL },                               //post operations not supported

    { IRP_MJ_LOCK_CONTROL,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_CLEANUP,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_CREATE_MAILSLOT,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_QUERY_SECURITY,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_SET_SECURITY,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_QUERY_QUOTA,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_SET_QUOTA,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_PNP,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_ACQUIRE_FOR_MOD_WRITE,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_RELEASE_FOR_MOD_WRITE,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_ACQUIRE_FOR_CC_FLUSH,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_RELEASE_FOR_CC_FLUSH,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_NETWORK_QUERY_OPEN,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_MDL_READ,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_MDL_READ_COMPLETE,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_PREPARE_MDL_WRITE,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_MDL_WRITE_COMPLETE,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_VOLUME_MOUNT,
      0,
      PocPreOperation,
      PocPostOperation },

    { IRP_MJ_VOLUME_DISMOUNT,
      0,
      PocPreOperation,
      PocPostOperation },

#endif // TODO

    { IRP_MJ_CREATE,
      0,
      PocPreCreateOperation,
      PocPostCreateOperation },

    { IRP_MJ_READ,
      0,
      PocPreReadOperation,
      PocPostReadOperation },

    { IRP_MJ_WRITE,
      0,
      PocPreWriteOperation,
      PocPostWriteOperation },

    { IRP_MJ_QUERY_INFORMATION,
      0,
      PocPreQueryInformationOperation,
      PocPostQueryInformationOperation },

    { IRP_MJ_SET_INFORMATION,
      0,
      PocPreSetInformationOperation,
      PocPostSetInformationOperation },
          
    { IRP_MJ_CLOSE,
      0,
      PocPreCloseOperation,
      PocPostCloseOperation },

    { IRP_MJ_OPERATION_END }
};

const FLT_CONTEXT_REGISTRATION ContextRegistration[] = {

    { FLT_STREAM_CONTEXT,
      0,
      PocContextCleanup,
      POC_STREAM_CONTEXT_SIZE,
      POC_STREAM_CONTEXT_TAG },

    { FLT_VOLUME_CONTEXT,
      0,
      PocContextCleanup,
      POC_VOLUME_CONTEXT_SIZE,
      POC_VOLUME_CONTEXT_TAG },

    { FLT_CONTEXT_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    ContextRegistration,                //  Context
    Callbacks,                          //  Operation callbacks

    PocUnload,                          //  MiniFilterUnload

    PocInstanceSetup,                   //  InstanceSetup
    NULL,                               //  InstanceQueryTeardown
    NULL,                               //  InstanceTeardownStart
    NULL,                               //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};


NTSTATUS
PocInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
/*++

Routine Description:

    This routine is called whenever a new instance is created on a volume. This
    gives us a chance to decide if we need to attach to this volume or not.

    If this routine is not defined in the registration structure, automatic
    instances are always created.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Flags describing the reason for this attach request.

Return Value:

    STATUS_SUCCESS - attach
    STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );
    UNREFERENCED_PARAMETER( VolumeFilesystemType );

    NTSTATUS Status = 0;

    Status = PocDoCompletionProcessingWhenSafe(
        (PVOID)PocInstanceSetupWhenSafe, 
        FltObjects->Volume,
        NULL);
    
    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, 
            ("%s->PocDoCompletionProcessingWhenSafe failed. Status = 0x%x.\n", __FUNCTION__, Status));
        goto EXIT;
    }

EXIT:

    return Status;
}

/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the initialization routine for this miniFilter driver.  This
    registers with FltMgr and initializes all global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Routine can return non success error codes.

--*/
{
    
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("Poc->DriverEntry: Entered.\n\nPlease configure processes and paths in Config.c.\n\n") );


    status = IoCreateDevice(
        DriverObject,
        sizeof(POC_DEVICE_EXTENSION),
        NULL,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &gDeviceObject);

    if (!NT_SUCCESS(status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->IoCreateDevice failed. Status = 0x%x.\n", __FUNCTION__, status));
        goto EXIT;
    }

    RtlZeroMemory(gDeviceObject->DeviceExtension, sizeof(POC_DEVICE_EXTENSION));

    PocInitDpcRoutine();


    status = PocInitProcess();

    if (STATUS_SUCCESS != status)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocInitProcess failed. Status = 0x%x.\n", __FUNCTION__, status));
        goto EXIT;
    }

    status = PocInitAesECBKey();

    if (STATUS_SUCCESS != status)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocInitAesKey failed. Status = 0x%x.\n", __FUNCTION__, status));
        goto EXIT;
    }

    status = PocInitFolderAndExt();

    if (STATUS_SUCCESS != status)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocInitFolderAndExt failed. Status = 0x%x.\n", __FUNCTION__, status));
        goto EXIT;
    }

    RtlMoveMemory(EncryptionTailer.Flag, POC_ENCRYPTION_HEADER_FLAG, strlen(POC_ENCRYPTION_HEADER_FLAG));

    RtlMoveMemory(
        EncryptionTailer.EncryptionAlgorithmType,
        POC_ENCRYPTION_HEADER_EA_TYPE, 
        strlen(POC_ENCRYPTION_HEADER_EA_TYPE));

    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter(DriverObject,
        &FilterRegistration,
        &gFilterHandle);

    FLT_ASSERT(NT_SUCCESS(status));

    if (!NT_SUCCESS(status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->FltRegisterFilter failed. Status = 0x%x.\n", __FUNCTION__, status));
        goto EXIT;
    }

    //
    //  Start filtering i/o
    //

    status = FltStartFiltering(gFilterHandle);

    if (!NT_SUCCESS(status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->FltStartFiltering failed. Status = 0x%x.\n", __FUNCTION__, status));
        goto EXIT;
    }


    status = PocInitCommPort();

    if (STATUS_SUCCESS != status)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocInitCommPort failed. Status = 0x%x.\n", __FUNCTION__, status));
        goto EXIT;
    }


    return status;

EXIT:

    if (NULL != gFilterHandle)
    {
        FltUnregisterFilter(gFilterHandle);
        gFilterHandle = NULL;
    }

    PocCloseCommPort();

    PocProcessCleanup();

    PocAesCleanup();

    if (NULL != gDeviceObject)
    {
        if (NULL != gDeviceObject->DeviceExtension)
            KeCancelTimer(&((PPOC_DEVICE_EXTENSION)(gDeviceObject->DeviceExtension))->Timer);

        if (NULL != gDeviceObject->DeviceExtension)
            IoFreeWorkItem(((PPOC_DEVICE_EXTENSION)(gDeviceObject->DeviceExtension))->IoWorkItem);

        IoDeleteDevice(gDeviceObject);
    }

    return status;
}


NTSTATUS
PocUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is the unload routine for this miniFilter driver. This is called
    when the minifilter is about to be unloaded. We can fail this unload
    request if this is not a mandatory unload indicated by the Flags
    parameter.

Arguments:

    Flags - Indicating if this is a mandatory unload.

Return Value:

    Returns STATUS_SUCCESS.

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("Poc->PocUnload: Entered\n") );

    /*
    * Can new invocations of the IRP_MJ_XXX callbacks begin(in other
    * threads) while the unload procedure is in the process of executing ?
    *
    * No.But the post callbacks will come for which you have returned
    * FLT_PREOP_SUCCESS_WITH_CALLBACK in pre callback.
    */ 

    /*
    * �ӽ���minifilter��Ҫ֧���Ȳ�λ��ǱȽ��ѵģ���Ϊ�����Unload������������ص�IRP����;��
    * Unloadʱ��û����Pre�����޷�����ģ�����Щ��Pre����WITH_CALLBACK�Ļ��ܴ���
    * �޷��������ЩIRP�ᵼ�����ݵ��𻵣��������ﲻ�ͷ�����ʹ�õ����Ļ���
    * ��Ϊ��ʹ��IoBuildSynchronousFsdRequest֮��ĺ�������IRP����Cache��Purge
    * �޷���Write����ֹ���ĵ��·�
    * ͬ������������Ļ�����޷����ܣ���������й¶
    * ���ԣ�����Ͳ�ʵ����
    */

    PocCloseCommPort();

    PocProcessCleanup();

    PocAesCleanup();

    if (NULL != gFilterHandle)
    {
        FltUnregisterFilter(gFilterHandle);
        gFilterHandle = NULL;
    }


    if (NULL != gDeviceObject)
    {
        if (NULL != gDeviceObject->DeviceExtension)
            KeCancelTimer(&((PPOC_DEVICE_EXTENSION)(gDeviceObject->DeviceExtension))->Timer);

        if (NULL != gDeviceObject->DeviceExtension)
            IoFreeWorkItem(((PPOC_DEVICE_EXTENSION)(gDeviceObject->DeviceExtension))->IoWorkItem);

        PocWorkItemListCleanup();

        IoDeleteDevice(gDeviceObject);
    }

    return STATUS_SUCCESS;
}

/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/
FLT_PREOP_CALLBACK_STATUS
PocPreCreateOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

    NTSTATUS Status;

    WCHAR FileName[POC_MAX_NAME_LENGTH] = { 0 };

    Status = PocGetFileNameOrExtension(Data, NULL, FileName);

    if (STATUS_SUCCESS != Status)
    {
        Status = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto EXIT;
    }

    /*
    * ���˵���Ŀ����չ���ļ���Create
    */
    Status = PocBypassIrrelevantBy_PathAndExtension(
        Data);

    /*
    * ��Ȩ�����Ժ���ļ������㲻���ڻ����ļ����ڣ�Ҳ�ᱻ�������ƣ�
    */
    if (POC_IRRELEVENT_FILE_EXTENSION == Status)
    {

        if (PocFindOrCreateStreamContextOutsite(
            Data->Iopb->TargetInstance,
            FileName,
            FALSE) == STATUS_SUCCESS)
        {
            
        }
        else
        {
            Status = FLT_PREOP_SUCCESS_NO_CALLBACK;
            goto EXIT;
        }
    }

    //Status = PocBypassIrrelevantFileExtension(FileExtension);

    //if (POC_IRRELEVENT_FILE_EXTENSION == Status)
    //{
    //    Status = FLT_PREOP_SUCCESS_NO_CALLBACK;
    //    goto EXIT;
    //}

    /*
    * FltDoCompletionProcessingWhenSafeҪ�������IRP Operation
    */
    if (!FLT_IS_IRP_OPERATION(Data))
    {
        Status = FLT_PREOP_DISALLOW_FASTIO;
        goto EXIT;
    }

    Status = FLT_PREOP_SUCCESS_WITH_CALLBACK;

EXIT:

    return Status;
}


FLT_POSTOP_CALLBACK_STATUS
PocPostCreateOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );

    FLT_POSTOP_CALLBACK_STATUS Status = FLT_POSTOP_FINISHED_PROCESSING;

    /*
    * ���FO����ʧ�ܣ�������PocFindOrCreateStreamContext
    */
    if (STATUS_SUCCESS != Data->IoStatus.Status)
    {
        Status = FLT_POSTOP_FINISHED_PROCESSING;
        goto EXIT;
    }

   
    if (!FltDoCompletionProcessingWhenSafe(Data,
        FltObjects,
        CompletionContext,
        Flags,
        PocPostCreateOperationWhenSafe,
        &Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->FltDoCompletionProcessingWhenSafe failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));
    }

EXIT:

    return Status;
}


FLT_POSTOP_CALLBACK_STATUS
PocPostCreateOperationWhenSafe(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    NTSTATUS Status, ProcessType;

    PPOC_STREAM_CONTEXT StreamContext = NULL;
    BOOLEAN ContextCreated = FALSE;

    WCHAR ProcessName[POC_MAX_NAME_LENGTH] = { 0 };

    WCHAR FileName[POC_MAX_NAME_LENGTH] = { 0 };
    WCHAR FileExtension[POC_MAX_NAME_LENGTH] = { 0 };

    ULONG CreateDisposition = 0;

    /*
    * ����StreamContext����Ҳ������Ψ�����Դ���StreamContext�ĵط�֮һ��
    * �����ط����ǲ���
    */
    Status = PocFindOrCreateStreamContext(
        Data->Iopb->TargetInstance,
        Data->Iopb->TargetFileObject,
        TRUE,
        &StreamContext,
        &ContextCreated);

    if (STATUS_SUCCESS != Status)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocFindOrCreateStreamContext failed. Status = 0x%x.\n",
            __FUNCTION__, Status));
        Status = FLT_POSTOP_FINISHED_PROCESSING;
        goto EXIT;
    }


    Status = PocGetProcessName(Data, ProcessName);

    /*
    * ��¼�������ļ�����Ȩ���̣��Ա���PostClose�������߳������н��̶������Ժ�д���ļ�����
    */
    PocUpdateStreamContextProcessInfo(Data, StreamContext);


    Status = PocGetFileNameOrExtension(Data, FileExtension, FileName);

    if (STATUS_SUCCESS != Status)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocGetFileNameOrExtension failed. Status = 0x%x ProcessName = %ws\n",
            __FUNCTION__, Status, ProcessName));
        Status = FLT_POSTOP_FINISHED_PROCESSING;
        goto EXIT;
    }

    if (ContextCreated || 0 == wcslen(StreamContext->FileName))
    {

        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->ContextCreated Fcb = %p FileName = %ws ProcessName = %ws.\n",
            __FUNCTION__,
            FltObjects->FileObject->FsContext,
            FileName,
            ProcessName));


        ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

        RtlZeroMemory(StreamContext->FileName, POC_MAX_NAME_LENGTH * sizeof(WCHAR));

        if (wcslen(FileName) < POC_MAX_NAME_LENGTH)
            RtlMoveMemory(StreamContext->FileName, FileName, wcslen(FileName) * sizeof(WCHAR));

        StreamContext->OriginSectionObjectPointers = FltObjects->FileObject->SectionObjectPointer;

        StreamContext->Volume = FltObjects->Volume;
        StreamContext->Instance = FltObjects->Instance;

        ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);

    }

    /*PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
        ("\n%s->enter ProcessName = %ws FileName = %ws.\n", __FUNCTION__, ProcessName, FileName));*/


    ProcessType = PocIsUnauthorizedProcess(ProcessName);


    if (POC_IS_AUTHORIZED_PROCESS == ProcessType &&
        FlagOn(Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess,
            (FILE_WRITE_DATA | FILE_APPEND_DATA)) &&
        NULL == StreamContext->FlushFileObject)
    {
        Status = PocInitFlushFileObject(
            StreamContext->FileName,
            &StreamContext->FlushFileObject);
    }


    if (FlagOn(Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess, (FILE_READ_DATA)) &&
        POC_IS_AUTHORIZED_PROCESS != ProcessType)
    {
        if (NULL == StreamContext->FlushFileObject)
        {
            Status = PocInitFlushFileObject(
                StreamContext->FileName,
                &StreamContext->FlushFileObject);
        }

        Status = PocFlushOriginalCache(
            FltObjects->Instance,
            StreamContext->FileName);
    }


    /*
    * �ж��Ƿ��м��ܱ�ʶβ�ĵط�
    * ����������ܱ�ʶβ�ڵ�FileName���ˣ����һ�£���PostClose�����
    * ��֮���Դ�������Ϊ���ļ������������������������ƶ���λ�ã�
    */
    if (FALSE == StreamContext->IsCipherText)
    {

        Status = PocCreateFileForEncTailer(FltObjects, StreamContext, ProcessName);

        if (POC_FILE_HAS_ENCRYPTION_TAILER == Status ||
            POC_TAILER_WRONG_FILE_NAME == Status)
        {
            PocUpdateFlagInStreamContext(StreamContext, Status);
        }
        else
        {
            /*
            * �������˵���ļ���δ���ܹ��ġ�
            *
            * ppt��xls�ļ���WPS��Office�ڴ��ļ�ʱ����ʹ�û�û��д�������
            * Ҳ���Զ�д��һ���ֵ����ݣ���ᵼ���ļ������ּ��ܣ�
            * ������������δ���ܵ�ppt��xls�ļ���ֻҪ����д���������POC_RENAME_TO_ENCRYPT��
            * POC_RENAME_TO_ENCRYPT����Write�в��������ݣ�ֱ����ΪPOC_BEING_DIRECT_ENCRYPTING��
            * �����ļ��Ͳ������һ�������ģ�һ�������ĵ�����ˡ�
            */

            if (_wcsnicmp(FileExtension, L"ppt",
                wcslen(FileExtension) > wcslen(L"ppt") ? wcslen(FileExtension) : wcslen(L"ppt")) == 0 ||
                _wcsnicmp(FileExtension, L"xls",
                    wcslen(FileExtension) > wcslen(L"xls") ? wcslen(FileExtension) : wcslen(L"xls")) == 0)
            {
                if (POC_IS_AUTHORIZED_PROCESS == ProcessType &&
                    POC_BEING_DIRECT_ENCRYPTING != StreamContext->Flag &&
                    FlagOn(Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess,
                        (FILE_WRITE_DATA | FILE_APPEND_DATA)))
                {
                    PocUpdateFlagInStreamContext(StreamContext, POC_RENAME_TO_ENCRYPT);
                }
            }

            Status = FLT_POSTOP_FINISHED_PROCESSING;
            goto EXIT;
        }
    }


    /*
    * ���������ļ����ڿ���һ���ļ������ļ�������ļ�������ĳ���Ѽ����Ҵ򿪹����ļ�������ͬ��
    * �����ñ��ݽ���ָ�����Ļ��壬���������ļ�д�롣
    */
    CreateDisposition = (Data->Iopb->Parameters.Create.Options >> 24) & 0x000000ff;

    if (POC_IS_BACKUP_PROCESS == ProcessType &&
        (FILE_SUPERSEDE == CreateDisposition ||
            FILE_OVERWRITE == CreateDisposition ||
            FILE_OVERWRITE_IF == CreateDisposition))
    {
        StreamContext->IsCipherText = FALSE;
        goto EXIT;
    }

    if (TRUE == StreamContext->IsReEncrypted)
    {
        goto EXIT;
    }

    /*
    * ���Ļ��彨��������Ѿ����ˣ���ֱ����
    * Ӧ����DataSectionObject�Ƿ񴴽�Ϊ׼
    */
    if (FlagOn(Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess,
        (FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA)) &&
        POC_IS_AUTHORIZED_PROCESS != ProcessType)
    {

        /*PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("\n%s->SectionObjectPointers operation enter Process = %ws File = %ws.\n",
                __FUNCTION__, ProcessName, FileName));*/

        if (NULL == StreamContext->ShadowSectionObjectPointers->DataSectionObject)
        {
            Status = PocInitShadowSectionObjectPointers(FltObjects, StreamContext);

            if (STATUS_SUCCESS != Status)
            {
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocInitShadowSectionObjectPointers failed. Status = 0x%x\n",
                    __FUNCTION__, Status));
                Status = FLT_POSTOP_FINISHED_PROCESSING;
                goto EXIT;
            }

        }
        else
        {

            Status = PocChangeSectionObjectPointerSafe(
                FltObjects->FileObject,
                StreamContext->ShadowSectionObjectPointers);

            if (STATUS_SUCCESS != Status)
            {
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocChangeSectionObjectPointerSafe failed.\n", __FUNCTION__));
                Status = FLT_POSTOP_FINISHED_PROCESSING;
                goto EXIT;
            }



            //PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->%ws already has ciphertext cache map. Change FO->SOP to chiphertext SOP.\n",
            //    __FUNCTION__, StreamContext->FileName));
        }


        //PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("\n"));

    }

    Status = FLT_POSTOP_FINISHED_PROCESSING;

EXIT:

    if (NULL != StreamContext)
    {
        FltReleaseContext(StreamContext);
        StreamContext = NULL;
    }

    return FLT_POSTOP_FINISHED_PROCESSING;
}


FLT_PREOP_CALLBACK_STATUS
PocPreCloseOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    NTSTATUS Status;

    PPOC_STREAM_CONTEXT StreamContext = NULL;

    BOOLEAN ContextCreated = FALSE;

    WCHAR ProcessName[POC_MAX_NAME_LENGTH] = { 0 };

    Status = PocFindOrCreateStreamContext(
        Data->Iopb->TargetInstance, 
        Data->Iopb->TargetFileObject, 
        FALSE, 
        &StreamContext, 
        &ContextCreated);

    if (STATUS_SUCCESS != Status)
    {
        if (STATUS_NOT_FOUND != Status && !FsRtlIsPagingFile(Data->Iopb->TargetFileObject))
            /*
            * ˵������Ŀ����չ�ļ�����Create��û�д���StreamContext������Ϊ�Ǹ�����
            * ������һ��Paging file������᷵��0xc00000bb��
            * ԭ����Fcb->Header.Flags2, FSRTL_FLAG2_SUPPORTS_FILTER_CONTEXTS�������
            *
            //
            //  To make FAT match the present functionality of NTFS, disable
            //  stream contexts on paging files
            //

            if (IsPagingFile) {
                SetFlag( Fcb->Header.Flags2, FSRTL_FLAG2_IS_PAGING_FILE );
                ClearFlag( Fcb->Header.Flags2, FSRTL_FLAG2_SUPPORTS_FILTER_CONTEXTS );
            }
            */
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("%s->PocFindOrCreateStreamContext failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));
        }

        Status = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto EXIT;
    }

    if (!FLT_IS_IRP_OPERATION(Data))
    {
        Status = FLT_PREOP_DISALLOW_FASTIO;
        goto EXIT;
    }

    Status = PocGetProcessName(Data, ProcessName);


    /*PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("\nPocPreCloseOperation->enter ProcessName = %ws File = %ws StreamContext->Flag = 0x%x.\n",
        ProcessName, StreamContext->FileName, StreamContext->Flag));*/


    *CompletionContext = StreamContext;
    Status = FLT_PREOP_SUCCESS_WITH_CALLBACK;
    return Status;

EXIT:

    if (NULL != StreamContext)
    {
        FltReleaseContext(StreamContext);
        StreamContext = NULL;
    }
    
    return Status;
}


FLT_POSTOP_CALLBACK_STATUS
PocPostCloseOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    ASSERT(CompletionContext != NULL);

    FLT_POSTOP_CALLBACK_STATUS Status = FLT_POSTOP_FINISHED_PROCESSING;
    PPOC_STREAM_CONTEXT StreamContext = NULL;
    StreamContext = CompletionContext;


    if (!FltDoCompletionProcessingWhenSafe(Data,
        FltObjects,
        CompletionContext,
        Flags,
        PocPostCloseOperationWhenSafe,
        &Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->FltDoCompletionProcessingWhenSafe failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));

        if (NULL != StreamContext)
        {
            FltReleaseContext(StreamContext);
            StreamContext = NULL;
        }
    }


    return Status;
}


FLT_POSTOP_CALLBACK_STATUS
PocPostCloseOperationWhenSafe(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    ASSERT(NULL != CompletionContext);

    NTSTATUS Status = 0;
    PPOC_STREAM_CONTEXT StreamContext = NULL;
    StreamContext = CompletionContext;

    HANDLE ThreadHandle = NULL;


    /*
    * ��PostClose�����߳�ȥд���ļ���ʶβ��������ܣ����docx��������
    */
    ExAcquireResourceSharedLite(StreamContext->Resource, TRUE);

    if ((POC_TO_APPEND_ENCRYPTION_TAILER == StreamContext->Flag ||
        POC_TAILER_WRONG_FILE_NAME == StreamContext->Flag ||
        POC_RENAME_TO_ENCRYPT == StreamContext->Flag ||
        POC_TO_DECRYPT_FILE == StreamContext->Flag) &&
        StreamContext->AppendTailerThreadStart == FALSE)
    {
        ExReleaseResourceLite(StreamContext->Resource);

        ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

        StreamContext->AppendTailerThreadStart = TRUE;

        ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);

        Status = PsCreateSystemThread(
            &ThreadHandle,
            THREAD_ALL_ACCESS,
            NULL,
            NULL,
            NULL,
            PocAppendEncTailerThread,
            StreamContext);

        if (STATUS_SUCCESS != Status)
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->PsCreateSystemThread PocAppendEncTailerThread failed. Status = 0x%x.\n",
                    __FUNCTION__,
                    Status));

            ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

            StreamContext->AppendTailerThreadStart = FALSE;

            ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);


            goto EXIT;
        }

        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->PsCreateSystemThread PocAppendEncTailerThread %ws init success. FileSize = %I64d.\n",
                __FUNCTION__,
                StreamContext->FileName,
                StreamContext->FileSize));

        if (NULL != ThreadHandle)
        {
            ZwClose(ThreadHandle);
            ThreadHandle = NULL;
        }

        goto EXIT;
    }
    else
    {
        ExReleaseResourceLite(StreamContext->Resource);

        if (NULL != StreamContext)
        {
            FltReleaseContext(StreamContext);
            StreamContext = NULL;
        }
    }


EXIT:

    return FLT_POSTOP_FINISHED_PROCESSING;
}
