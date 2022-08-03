#pragma once

#include "global.h"
#include "process.h"


typedef struct _POC_PAGE_TEMP_BUFFER
{
    LONGLONG StartingVbo;
    LONGLONG ByteCount;
    PCHAR Buffer;

}POC_PAGE_TEMP_BUFFER, * PPOC_PAGE_TEMP_BUFFER;


//
//  Stream context data structure
//

typedef struct _POC_STREAM_CONTEXT
{

    ULONG Flag;
    PWCHAR FileName;

    PFLT_VOLUME Volume;
    PFLT_INSTANCE Instance;

    /*
    * FileSize�д�������==���Ĵ�С����Ϊд��ȥ��β��NonCachedIo���������������룬���ǽ���������д��
    * FileSize��Ҫ����������β������PostQueryInformation��PreRead��PostRead��ʹ��
    * FileSize����PostWrite�и��£�����PostClose��д��β�����Ա������������һ�δ��ļ�ʱ����β����ȡ��
    */
    LONGLONG FileSize;
    BOOLEAN LessThanAesBlockSize;


    /*
    * ��Ntfs 10.0.17763.2686�У�WRITE_THROUGH��ʶ����������PagingIo֮ǰ����Fcb->FileSize��
    * Ҳ���ǣ�����PagingIoʱ��ʹ�ñ�ı����ض����ݣ�TopLevelIrpContext + 184����
    * ����������ʹ��WriteThroughFileSize���Fcb->FileSize
    */
    LONGLONG WriteThroughFileSize;

    /*
    * ���Ļ��壬���Ļ����Լ����Ļ���������FileObject
    */
    PSECTION_OBJECT_POINTERS OriginSectionObjectPointers;
    PSECTION_OBJECT_POINTERS ShadowSectionObjectPointers;
    PFILE_OBJECT ShadowFileObject;

    /*
    * ˵���ļ���������
    */
    BOOLEAN IsCipherText;

    /*
    * �������ڶ���������С�Ŀ����StreamContext->PageNextToLastForWrite��
    */
    POC_PAGE_TEMP_BUFFER PageNextToLastForWrite;

    /*
    * �ȴ���������Ȩ���̽����Ժ�д���ļ���ʶβ��
    */
    HANDLE ProcessId[POC_MAX_AUTHORIZED_PROCESS_COUNT];
    BOOLEAN AppendTailerThreadStart;

    /*
    * ���FO����Write����Ժ�ObDereferenceObject��
    * ��������������һ��Close�����ǵ��߳����Ͽ��Կ���
    */
    PFILE_OBJECT FlushFileObject;


    /*
    * IsDirty��ʶ�Ƿ�ֹ���ݽ��̽���ʱδд���ʶβ�����Ŀ����������ļ���
    */
    BOOLEAN IsDirty;

    /*
    * ���ݽ���������Ѽ��ܵ��ļ�д������ļ��У����ǻ��ٴμ�����������Write�л������ʶβ��
    * �����Ժ������ٽ������ܣ����һ���ͽ�����ظ����ܵ����⡣
    */
    BOOLEAN IsReEncrypted;


    PERESOURCE Resource;

} POC_STREAM_CONTEXT, * PPOC_STREAM_CONTEXT;

#define POC_STREAM_CONTEXT_SIZE         sizeof(POC_STREAM_CONTEXT)
#define POC_RESOURCE_TAG                      'cRxC'
#define POC_STREAM_CONTEXT_TAG                'cSxC'

typedef struct _POC_VOLUME_CONTEXT 
{

    //
    //  Holds the sector size for this volume.
    //

    ULONG SectorSize;

} POC_VOLUME_CONTEXT, * PPOC_VOLUME_CONTEXT;

#define MIN_SECTOR_SIZE 0x200
#define POC_VOLUME_CONTEXT_TAG                  'cVxC'
#define POC_VOLUME_CONTEXT_SIZE                 sizeof(POC_VOLUME_CONTEXT)


NTSTATUS PocCreateStreamContext(
    _In_ PFLT_FILTER FilterHandle, 
    _Outptr_ PPOC_STREAM_CONTEXT* StreamContext);

NTSTATUS
PocFindOrCreateStreamContext(
    IN PFLT_INSTANCE Instance,
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN CreateIfNotFound,
    OUT PPOC_STREAM_CONTEXT* StreamContext,
    OUT PBOOLEAN ContextCreated);

VOID PocContextCleanup(
    _In_ PFLT_CONTEXT Context, 
    _In_ FLT_CONTEXT_TYPE ContextType);

NTSTATUS PocUpdateNameInStreamContext(
    IN PPOC_STREAM_CONTEXT StreamContext,
    IN PWCHAR NewFileName);

VOID PocUpdateFlagInStreamContext(
    IN PPOC_STREAM_CONTEXT StreamContext,
    IN ULONG Flag);

NTSTATUS PocUpdateStreamContextProcessInfo(
    IN PFLT_CALLBACK_DATA Data,
    IN OUT PPOC_STREAM_CONTEXT StreamContext);

VOID PocInstanceSetupWhenSafe(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context);
