
#include "write.h"
#include "context.h"
#include "utils.h"
#include "cipher.h"
#include "filefuncs.h"
#include "process.h"

FLT_PREOP_CALLBACK_STATUS
PocPreWriteOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    NTSTATUS Status;

    WCHAR ProcessName[POC_MAX_NAME_LENGTH] = { 0 };

    PPOC_STREAM_CONTEXT StreamContext = NULL;
    BOOLEAN ContextCreated = FALSE;

    BOOLEAN NonCachedIo = FALSE;
    BOOLEAN PagingIo = FALSE;

    PCHAR OrigBuffer = NULL, NewBuffer = NULL;
    PMDL NewMdl = NULL;
    LONGLONG NewBufferLength = 0;

    LONGLONG FileSize = 0, StartingVbo = 0, ByteCount = 0, LengthReturned = 0;

    PPOC_VOLUME_CONTEXT VolumeContext = NULL;
    ULONG SectorSize = 0;
    
    PPOC_SWAP_BUFFER_CONTEXT SwapBufferContext = NULL;
    
    ByteCount = Data->Iopb->Parameters.Write.Length;
    StartingVbo = Data->Iopb->Parameters.Write.ByteOffset.QuadPart;

    NonCachedIo = BooleanFlagOn(Data->Iopb->IrpFlags, IRP_NOCACHE);
    PagingIo = BooleanFlagOn(Data->Iopb->IrpFlags, IRP_PAGING_IO);


    if (FLT_IS_FASTIO_OPERATION(Data))
    {
        Status = FLT_PREOP_DISALLOW_FASTIO;
        goto ERROR;
    }

    if (0 == ByteCount)
    {
        Status = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto ERROR;
    }


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
        goto ERROR;
    }

    Status = PocGetProcessName(Data, ProcessName);


    if (PagingIo && 0 != StreamContext->WriteThroughFileSize)
    {
        FileSize = StreamContext->WriteThroughFileSize;
    }
    else
    {
        FileSize = ((PFSRTL_ADVANCED_FCB_HEADER)FltObjects->FileObject->FsContext)->FileSize.QuadPart;
    }


    //PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, 
    //    ("\nPocPreWriteOperation->enter StartingVbo = %I64d Length = %d FileSize = %I64d ProcessName = %ws File = %ws.\n NonCachedIo = %d PagingIo = %d\n",
    //    Data->Iopb->Parameters.Write.ByteOffset.QuadPart,
    //    Data->Iopb->Parameters.Write.Length,
    //    FileSize,
    //    ProcessName, StreamContext->FileName,
    //    NonCachedIo,
    //    PagingIo));

    if (POC_RENAME_TO_ENCRYPT == StreamContext->Flag && NonCachedIo)
    {
        /*
        * δ���ܵ�doc,docx,ppt,pptx,xls,xlsx�ļ�������ֱ��д�������ļ�ʱ�����Զ����ܣ�
        * ���ǻ��ڸý��̹ر��Ժ�����ȥ�ж��Ƿ�Ӧ�ü��ܸ����ļ���
        */
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, 
            ("%s->Leave PostClose will encrypt the file. StartingVbo = %I64d Length = %I64d ProcessName = %ws File = %ws.\n",
                __FUNCTION__,
                Data->Iopb->Parameters.Write.ByteOffset.QuadPart,
                ByteCount,
                ProcessName, 
                StreamContext->FileName));

        Status = FLT_PREOP_SUCCESS_NO_CALLBACK;
        goto ERROR;
    }


    if (FltObjects->FileObject->SectionObjectPointer == 
        StreamContext->ShadowSectionObjectPointers)
    {
        /*
        * ������д�����Ļ��壬������NonCachedIo����������
        */
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->Block NonCachedIo = %d chipertext cachemap StartingVbo = %I64d Length = %I64d ProcessName = %ws File = %ws.",
                __FUNCTION__,
                NonCachedIo ? 1 : 0,
                Data->Iopb->Parameters.Write.ByteOffset.QuadPart,
                ByteCount,
                ProcessName,
                StreamContext->FileName));

        Data->IoStatus.Status = STATUS_SUCCESS;
        Data->IoStatus.Information = Data->Iopb->Parameters.Write.Length;

        Status = FLT_PREOP_COMPLETE;
        goto ERROR;
    }


    SwapBufferContext = ExAllocatePoolWithTag(NonPagedPool, sizeof(POC_SWAP_BUFFER_CONTEXT), WRITE_BUFFER_TAG);

    if (NULL == SwapBufferContext)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPreWriteOperation->ExAllocatePoolWithTag SwapBufferContext failed.\n"));
        Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        Data->IoStatus.Information = 0;
        Status = FLT_PREOP_COMPLETE;
        goto ERROR;
    }

    RtlZeroMemory(SwapBufferContext, sizeof(POC_SWAP_BUFFER_CONTEXT));


    if (!NonCachedIo)
    {
        /*
        * 16���ֽ�������չ�ļ���С������һ����PreSetInfo��������Ӧ����if (!PagingIo)����
        * NonCachedIoҪ��Length > SectorSize������if (!NonCachedIo)���С�
        */

        if (FileSize < AES_BLOCK_SIZE)
        {
            /*
            * FSD��Write���������չ�ļ���С��������FastFat��Write������ExtendingFile
            */
            if (StartingVbo + Data->Iopb->Parameters.Write.Length < AES_BLOCK_SIZE)
            {
                SwapBufferContext->OriginalLength = Data->Iopb->Parameters.Write.Length;

                ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

                StreamContext->FileSize = StartingVbo + Data->Iopb->Parameters.Write.Length;
                StreamContext->LessThanAesBlockSize = TRUE;

                ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);

                Data->Iopb->Parameters.Write.Length = AES_BLOCK_SIZE - (ULONG)StartingVbo;

                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                    ("%s->ExtendingFile success. Filesize = %I64d StartingVbo = %I64d OriginWriteLength = %d NewLength = %d.\n",
                        __FUNCTION__,
                        FileSize,
                        StartingVbo,
                        StreamContext->FileSize,
                        Data->Iopb->Parameters.Write.Length));

                FltSetCallbackDataDirty(Data);

            }
        }
        else if (AES_BLOCK_SIZE == FileSize)
        {
            /*
            * PostWrite����CurrentByteOffset
            */
            if (StartingVbo + Data->Iopb->Parameters.Write.Length < AES_BLOCK_SIZE)
            {
                ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

                StreamContext->FileSize = StartingVbo + Data->Iopb->Parameters.Write.Length;
                StreamContext->LessThanAesBlockSize = TRUE;

                ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);
            }
        }

        /*
        * ��CachedIo WRITE_THROUGHʱ�ݴ棬��PagingIoʱȡ�����滻Fcb->FileSize
        */
        if (FlagOn(FltObjects->FileObject->Flags, FO_WRITE_THROUGH))
        {
            StreamContext->WriteThroughFileSize = StartingVbo + Data->Iopb->Parameters.Write.Length;
        }
    }


    if (!PagingIo)
    {
        /*
        * ��Ҫ��PostWrite�޸����Ļ���Ĵ�С
        */
        if (StartingVbo + ByteCount > FileSize)
        {
            SwapBufferContext->IsCacheExtend = TRUE;
        }
    }


    if (NonCachedIo)
    {
        Status = FltGetVolumeContext(FltObjects->Filter, FltObjects->Volume, &VolumeContext);

        if (!NT_SUCCESS(Status) || 0 == VolumeContext->SectorSize)
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPostReadOperation->FltGetVolumeContext failed. Status = 0x%x\n", Status));
            Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            Data->IoStatus.Information = 0;
            Status = FLT_PREOP_COMPLETE;
            goto ERROR;
        }

        SectorSize = VolumeContext->SectorSize;

        if (NULL != VolumeContext)
        {
            FltReleaseContext(VolumeContext);
            VolumeContext = NULL;
        }


        //LengthReturned�Ǳ���Write������Ҫд������
        if (!PagingIo || FileSize >= StartingVbo + ByteCount)
        {
            LengthReturned = ByteCount;
        }
        else
        {
            LengthReturned = FileSize - StartingVbo;
        }

        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPreWriteOperation->RealToWrite = %I64d.\n", LengthReturned));
        
        if (Data->Iopb->Parameters.Write.MdlAddress != NULL) 
        {

            FLT_ASSERT(((PMDL)Data->Iopb->Parameters.Write.MdlAddress)->Next == NULL);

            OrigBuffer = MmGetSystemAddressForMdlSafe(Data->Iopb->Parameters.Write.MdlAddress,
                NormalPagePriority | MdlMappingNoExecute);

            if (OrigBuffer == NULL) 
            {
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPreWriteOperation->Failed to get system address for MDL: %p\n",
                    Data->Iopb->Parameters.Write.MdlAddress));

                Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                Data->IoStatus.Information = 0;
                Status = FLT_PREOP_COMPLETE;
                goto ERROR;
            }

        }
        else
        {
            OrigBuffer = Data->Iopb->Parameters.Write.WriteBuffer;
        }





        if (FALSE == StreamContext->IsCipherText &&
            FileSize % SectorSize == 0 &&
            FileSize > PAGE_SIZE &&
            NonCachedIo)
        {
            /*
            * �����ļ����ظ�������
            */
            if (StartingVbo <= FileSize - PAGE_SIZE &&
                StartingVbo + ByteCount >= FileSize - PAGE_SIZE + SectorSize)
            {
                if (strncmp(
                    ((PPOC_ENCRYPTION_TAILER)(OrigBuffer + FileSize - PAGE_SIZE - StartingVbo))->Flag, 
                    EncryptionTailer.Flag,
                    strlen(EncryptionTailer.Flag)) == 0)
                {

                    ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

                    StreamContext->IsReEncrypted = TRUE;

                    ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);

                    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                        ("%s->File has been repeatedly encrypted. StartingVbo = %I64d Length = %I64d ProcessName = %ws File = %ws.",
                            __FUNCTION__,
                            Data->Iopb->Parameters.Write.ByteOffset.QuadPart,
                            ByteCount,
                            ProcessName,
                            StreamContext->FileName));

                }
            }
        }


        


        if (FileSize > AES_BLOCK_SIZE &&
            LengthReturned < AES_BLOCK_SIZE)
        {
            NewBufferLength = SectorSize + ByteCount;
        }
        else
        {
            NewBufferLength = ByteCount;
        }

        NewBuffer = FltAllocatePoolAlignedWithTag(FltObjects->Instance, NonPagedPool, NewBufferLength, WRITE_BUFFER_TAG);

        if (NULL == NewBuffer)
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPreWriteOperation->FltAllocatePoolAlignedWithTag NewBuffer failed.\n"));
            Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            Data->IoStatus.Information = 0;
            Status = FLT_PREOP_COMPLETE;
            goto ERROR;
        }

        RtlZeroMemory(NewBuffer, NewBufferLength);

        if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_IRP_OPERATION)) 
        {

            NewMdl = IoAllocateMdl(NewBuffer, (ULONG)NewBufferLength, FALSE, FALSE, NULL);

            if (NewMdl == NULL) 
            {
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPreWriteOperation->IoAllocateMdl NewMdl failed.\n"));
                Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                Data->IoStatus.Information = 0;
                Status = FLT_PREOP_COMPLETE;
                goto ERROR;
            }

            MmBuildMdlForNonPagedPool(NewMdl);
        }
        


        try 
        {
            if (FileSize < AES_BLOCK_SIZE)
            {
                /*
                * �ļ���СС�ڿ��С
                */

                LengthReturned = AES_BLOCK_SIZE;

                Status = PocAesECBEncrypt(
                    OrigBuffer,
                    (ULONG)LengthReturned,
                    NewBuffer,
                    &(ULONG)LengthReturned);

                if (STATUS_SUCCESS != Status)
                {
                    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPreWriteOperation->PocAesECBEncrypt2 failed.\n"));
                    Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
                    Data->IoStatus.Information = 0;
                    Status = FLT_PREOP_COMPLETE;
                    goto ERROR;
                }

            }
            else if ((FileSize > StartingVbo + ByteCount) && 
                    (FileSize - (StartingVbo + ByteCount) < AES_BLOCK_SIZE))
            {
                /*
                * ���ļ�����һ���飬Cache Manager�����ݷֶ��д����̣�
                * ���һ��д������С��һ���������£������ڵ����ڶ�������һ�´���
                */

                if (SectorSize == ByteCount)
                {
                    ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

                    RtlMoveMemory(StreamContext->PageNextToLastForWrite.Buffer, OrigBuffer, SectorSize);
                    StreamContext->PageNextToLastForWrite.StartingVbo = StartingVbo;
                    StreamContext->PageNextToLastForWrite.ByteCount = ByteCount;

                    ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);

                    Data->IoStatus.Status = STATUS_SUCCESS;
                    Data->IoStatus.Information = Data->Iopb->Parameters.Write.Length;

                    Status = FLT_PREOP_COMPLETE;
                    goto ERROR;
                }
                else if(ByteCount > SectorSize)
                {

                    ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

                    RtlMoveMemory(StreamContext->PageNextToLastForWrite.Buffer, OrigBuffer + ByteCount - SectorSize, SectorSize);
                    StreamContext->PageNextToLastForWrite.StartingVbo = StartingVbo + ByteCount - SectorSize;
                    StreamContext->PageNextToLastForWrite.ByteCount = SectorSize;

                    ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);

                    LengthReturned = ByteCount - SectorSize;

                    Status = PocAesECBEncrypt(
                        OrigBuffer, 
                        (ULONG)LengthReturned,
                        NewBuffer, 
                        &(ULONG)LengthReturned);

                    if (STATUS_SUCCESS != Status)
                    {
                        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPreWriteOperation->PocAesECBEncrypt1 failed.\n"));
                        Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
                        Data->IoStatus.Information = 0;
                        Status = FLT_PREOP_COMPLETE;
                        goto ERROR;
                    }

                    Data->Iopb->Parameters.Write.Length -= SectorSize;
                    FltSetCallbackDataDirty(Data);
                    SwapBufferContext->OriginalLength = (ULONG)ByteCount;
                }

            }
            else if (FileSize > AES_BLOCK_SIZE && 
                    LengthReturned < AES_BLOCK_SIZE)
            {
                /*
                * ���ļ�����һ���飬Cache Manager�����ݷֶ��д����̣����һ��д������С��һ����ʱ
                */
                ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

                RtlMoveMemory(
                    StreamContext->PageNextToLastForWrite.Buffer + 
                    StreamContext->PageNextToLastForWrite.ByteCount, 
                    OrigBuffer, LengthReturned);

                ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);

                LengthReturned = StreamContext->PageNextToLastForWrite.ByteCount + LengthReturned;

                Status = PocAesECBEncrypt_CiphertextStealing(
                    StreamContext->PageNextToLastForWrite.Buffer, 
                    (ULONG)LengthReturned,
                    NewBuffer);

                if (STATUS_SUCCESS != Status)
                {
                    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPreWriteOperation->PocAesECBEncrypt_CiphertextStealing1 failed.\n"));
                    Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
                    Data->IoStatus.Information = 0;
                    Status = FLT_PREOP_COMPLETE;
                    goto ERROR;
                }

                Data->Iopb->Parameters.Write.ByteOffset.QuadPart = StreamContext->PageNextToLastForWrite.StartingVbo;
                Data->Iopb->Parameters.Write.Length = (ULONG)(SectorSize + ByteCount);
                FltSetCallbackDataDirty(Data);

                SwapBufferContext->OriginalLength = (ULONG)ByteCount;

            }
            else if (LengthReturned % AES_BLOCK_SIZE != 0)
            {
                /*
                * ����Ҫд�����ݴ���һ����ʱ���ҺͿ��С������ʱ������������Ų�õķ�ʽ������Ҫ�����ļ���С
                */

                Status = PocAesECBEncrypt_CiphertextStealing(
                    OrigBuffer, 
                    (ULONG)LengthReturned,
                    NewBuffer);

                if (STATUS_SUCCESS != Status)
                {
                    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPreWriteOperation->PocAesECBEncrypt_CiphertextStealing2 failed.\n"));
                    Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
                    Data->IoStatus.Information = 0;
                    Status = FLT_PREOP_COMPLETE;
                    goto ERROR;
                }

            }
            else
            {
                /*
                * ����Ҫд�����ݱ���ͺͿ��С����ʱ��ֱ�Ӽ���
                */

                Status = PocAesECBEncrypt(
                    OrigBuffer, 
                    (ULONG)LengthReturned,
                    NewBuffer, 
                    &(ULONG)LengthReturned);

                if (STATUS_SUCCESS != Status)
                {
                    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPreWriteOperation->PocAesECBEncrypt2 failed.\n"));
                    Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
                    Data->IoStatus.Information = 0;
                    Status = FLT_PREOP_COMPLETE;
                    goto ERROR;
                }

            }

        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            Data->IoStatus.Status = GetExceptionCode();
            Data->IoStatus.Information = 0;
            Status = FLT_PREOP_COMPLETE;
            goto ERROR;
        }



        SwapBufferContext->NewBuffer = NewBuffer;
        SwapBufferContext->NewMdl = NewMdl;
        SwapBufferContext->StreamContext = StreamContext;
        *CompletionContext = SwapBufferContext;

        Data->Iopb->Parameters.Write.WriteBuffer = NewBuffer;
        Data->Iopb->Parameters.Write.MdlAddress = NewMdl;
        FltSetCallbackDataDirty(Data);


        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("PocPreWriteOperation->Encrypt success. StartingVbo = %I64d Length = %d ProcessName = %ws File = %ws.\n\n",
            Data->Iopb->Parameters.Write.ByteOffset.QuadPart,
            (ULONG)LengthReturned,
            ProcessName,
            StreamContext->FileName));


        Status = FLT_PREOP_SUCCESS_WITH_CALLBACK;
        goto EXIT;
    }



    *CompletionContext = SwapBufferContext;
    SwapBufferContext->StreamContext = StreamContext;
    Status = FLT_PREOP_SUCCESS_WITH_CALLBACK;
    goto EXIT;

ERROR:

    if (NULL != StreamContext)
    {
        FltReleaseContext(StreamContext);
        StreamContext = NULL;
    }

    if (NULL != NewBuffer)
    {
        FltFreePoolAlignedWithTag(FltObjects->Instance, NewBuffer, WRITE_BUFFER_TAG);
        NewBuffer = NULL;
    }

    if (NULL != NewMdl)
    {
        IoFreeMdl(NewMdl);
        NewMdl = NULL;
    }

    if (NULL != SwapBufferContext)
    {
        ExFreePoolWithTag(SwapBufferContext, WRITE_BUFFER_TAG);
        SwapBufferContext = NULL;
    }

EXIT:

    return Status;
}


FLT_POSTOP_CALLBACK_STATUS
PocPostWriteOperation(
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
    ASSERT(((PPOC_SWAP_BUFFER_CONTEXT)CompletionContext)->StreamContext != NULL);

    PPOC_SWAP_BUFFER_CONTEXT SwapBufferContext = NULL;
    PPOC_STREAM_CONTEXT StreamContext = NULL;

    LONGLONG FileSize = 0;

    SwapBufferContext = CompletionContext;
    StreamContext = SwapBufferContext->StreamContext;


    if (0 != StreamContext->WriteThroughFileSize)
    {
        FileSize = StreamContext->WriteThroughFileSize;
    }
    else
    {
        FileSize = ((PFSRTL_ADVANCED_FCB_HEADER)FltObjects->FileObject->FsContext)->FileSize.QuadPart;
    }


    if (BooleanFlagOn(Data->Iopb->IrpFlags, IRP_NOCACHE))
    {
        /*
        * �ļ����޸Ĺ����һ�δд���ļ���ʶβ����ֹ���ݽ��̶��ļ�
        */
        ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

        StreamContext->IsDirty = TRUE;

        ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);
    }

    if (!BooleanFlagOn(Data->Iopb->IrpFlags, IRP_PAGING_IO) &&
        FileSize <= AES_BLOCK_SIZE)
    {
        /*
        * WriteFile֮��ĺ�����
        * This function writes data to a file, starting at the position indicated by the file pointer. 
        * After the write operation has been completed, 
        * the file pointer is adjusted by the number of bytes written.
        */
        FltObjects->FileObject->CurrentByteOffset.QuadPart = StreamContext->FileSize;
    }

    if (BooleanFlagOn(Data->Iopb->IrpFlags, IRP_NOCACHE) &&
        (TRUE != StreamContext->LessThanAesBlockSize || 
            FileSize > AES_BLOCK_SIZE))
    {
        /*
        * ��¼�ļ������Ĵ�С��С��16���ֽڵ�StreamContext->FileSize�Ѿ������������¹��ˣ�
        * ���ﲻ���ٸ����ˣ���Ϊ�����FileSize�Ѿ���16���ֽ��ˡ�
        */
        ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

        StreamContext->FileSize = FileSize;

        ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);
    }

    
    /*
    * ��չ���Ļ���Ĵ�С����PostWrite����Ϊ��������Ҫ�������ļ�ϵͳ������Writeȥ��չAllocationSize��ֵ��
    * ����Щֵ��չ�Ժ����ǲ����������Ļ���Ĵ�С��
    */
    if (TRUE == SwapBufferContext->IsCacheExtend && 
        NULL != StreamContext->ShadowSectionObjectPointers &&
        NULL != StreamContext->ShadowSectionObjectPointers->SharedCacheMap &&
        NULL != StreamContext->ShadowFileObject)
    {
        ExAcquireResourceExclusiveLite(((PFSRTL_ADVANCED_FCB_HEADER)(FltObjects->FileObject->FsContext))->Resource, TRUE);

        CcSetFileSizes(StreamContext->ShadowFileObject, 
            (PCC_FILE_SIZES) & ((PFSRTL_ADVANCED_FCB_HEADER)(FltObjects->FileObject->FsContext))->AllocationSize);

        ExReleaseResourceLite(((PFSRTL_ADVANCED_FCB_HEADER)(FltObjects->FileObject->FsContext))->Resource);
    }


    if (0 != SwapBufferContext->OriginalLength)
    {
        /*
        * д�볤�ȱ��޸Ĺ���������ԭ
        */
        Data->IoStatus.Information = SwapBufferContext->OriginalLength;
    }


    if (Data->Iopb->Parameters.Write.ByteOffset.QuadPart +
        Data->Iopb->Parameters.Write.Length >=
        FileSize
        && BooleanFlagOn(Data->Iopb->IrpFlags, IRP_NOCACHE))
    {
        if (TRUE == StreamContext->IsReEncrypted)
        {
            /*
            * �ļ����ظ������ˣ�������PostClose��������һ��
            */
            PocUpdateFlagInStreamContext(StreamContext, POC_TO_DECRYPT_FILE);
        }
        else
        {
            /*
            * �ļ������ܣ�������PostClose����д���ļ���ʶβ
            */
            PocUpdateFlagInStreamContext(StreamContext, POC_TO_APPEND_ENCRYPTION_TAILER);
        }

        /*
        * �����ļ��ѱ����ܣ�����Read�Ż����
        */
        ExEnterCriticalRegionAndAcquireResourceExclusive(StreamContext->Resource);

        StreamContext->IsCipherText = TRUE;

        StreamContext->LessThanAesBlockSize = FALSE;

        ExReleaseResourceAndLeaveCriticalRegion(StreamContext->Resource);

        if (NULL != StreamContext->FlushFileObject)
        {
            ObDereferenceObject(StreamContext->FlushFileObject);
            StreamContext->FlushFileObject = NULL;
        }
    }


    if (FlagOn(FltObjects->FileObject->Flags, FO_WRITE_THROUGH))
    {
        StreamContext->WriteThroughFileSize = 0;
    }


    if (NULL != SwapBufferContext->NewBuffer)
    {
        FltFreePoolAlignedWithTag(FltObjects->Instance, SwapBufferContext->NewBuffer, WRITE_BUFFER_TAG);
        SwapBufferContext->NewBuffer = NULL;
    }

    if (NULL != SwapBufferContext)
    {
        ExFreePoolWithTag(SwapBufferContext, WRITE_BUFFER_TAG);
        SwapBufferContext = NULL;
    }

    if (NULL != StreamContext)
    {
        FltReleaseContext(StreamContext);
        StreamContext = NULL;
    }

    return FLT_POSTOP_FINISHED_PROCESSING;
}
