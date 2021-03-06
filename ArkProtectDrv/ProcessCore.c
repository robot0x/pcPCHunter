#include "ProcessCore.h"


extern DYNAMIC_DATA	g_DynamicData;

typedef
UINT_PTR
(*pfnObGetObjectType)(PVOID Object);


/************************************************************************
*  Name : APGetPsIdleProcess
*  Param: void
*  Ret  : UINT_PTR			PsIdleProcess
*  获得System Idle Process 的EProcess地址
************************************************************************/
UINT_PTR
APGetPsIdleProcess()
{
	UINT_PTR PsIdleProcess = 0;
	UINT_PTR PsInitialSystemProcessAddress = (UINT_PTR)&PsInitialSystemProcess;

	if (PsInitialSystemProcessAddress && MmIsAddressValid((PVOID)((PUINT8)PsInitialSystemProcessAddress + 0xA0)))
	{
		PsIdleProcess = *(PUINT_PTR)((PUINT8)PsInitialSystemProcessAddress + 0xA0);
		if (PsIdleProcess <= 0xffff)
		{
			PsIdleProcess = *(PUINT_PTR)((PUINT8)PsInitialSystemProcessAddress + 0xB0);
		}
	}
	return PsIdleProcess;
}


/************************************************************************
*  Name : APGetObjectType
*  Param: Object				对象体首地址
*  Ret  : UINT_PTR				（对象类型）
*  x64：通过ObGetObjectType获得对象类型
************************************************************************/
UINT_PTR
APGetObjectType(IN PVOID Object)
{
	UINT_PTR	ObjectType = 0;

	pfnObGetObjectType	ObGetObjectType = NULL;
	if (MmIsAddressValid && Object && MmIsAddressValid(Object))
	{
		APGetNtosExportVariableAddress(L"ObGetObjectType", (PVOID*)&ObGetObjectType);
		if (ObGetObjectType)
		{
			ObjectType = ObGetObjectType(Object);
		}
	}

	return ObjectType;
}


/************************************************************************
*  Name : APIsActiveProcess
*  Param: Object				对象体首地址
*  Ret  : BOOLEAN
*  通过是否存在句柄表判断进程是否存活 TRUE存活/ FALSE死进程
************************************************************************/
BOOLEAN
APIsActiveProcess(IN PEPROCESS EProcess)
{
	BOOLEAN bOk = FALSE;

	if (EProcess &&
		MmIsAddressValid(EProcess) &&
		MmIsAddressValid((PVOID)((PUINT8)EProcess + g_DynamicData.ObjectTable)))
	{
		PVOID ObjectTable = *(PVOID*)((PUINT8)EProcess + g_DynamicData.ObjectTable);

		if (ObjectTable &&
			MmIsAddressValid(ObjectTable))
		{
			bOk = TRUE;
		}
	}

	return bOk;
}


/************************************************************************
*  Name : IsValidProcess
*  Param: EProcess				进程体对象
*  Ret  : BOOLEAN
*  判断是否为合法进程 TRUE合法/ FALSE非法
************************************************************************/
BOOLEAN
APIsValidProcess(IN PEPROCESS EProcess)
{
	UINT_PTR    ObjectType;
	BOOLEAN		bOk = FALSE;

	UINT_PTR	ProcessType = ((UINT_PTR)*PsProcessType);		// 导出全局变量，进程对象类型

	if (ProcessType && EProcess && MmIsAddressValid((PVOID)(EProcess)))
	{
		ObjectType = APGetObjectType((PVOID)EProcess);   //*PsProcessType 

		if (ObjectType &&
			ObjectType == ProcessType &&
			APIsActiveProcess(EProcess))
		{
			bOk = TRUE;
		}
	}

	return bOk;
}


/************************************************************************
*  Name : APGetProcessCount
*  Param: OutputBuffer			Ring3缓冲区，存放进程个数
*  Ret  : NTSTATUS
*  获得当前进程个数
************************************************************************/
NTSTATUS
APGetProcessNum(OUT PVOID OutputBuffer)
{
	UINT32 ProcessCount = 0;
	UINT32 ProcessId = 0;
	for (  ProcessId = 0; ProcessId < MAX_PROCESS_COUNT; ProcessId += 4)
	{
		NTSTATUS  Status = STATUS_UNSUCCESSFUL;
		PEPROCESS EProcess = NULL;

		if (ProcessId == 0)
		{
			ProcessCount++;
			continue;
		}

		Status = PsLookupProcessByProcessId((HANDLE)ProcessId, &EProcess);
		if (NT_SUCCESS(Status))
		{
			if (APIsValidProcess(EProcess))
			{
				ProcessCount++;
			}

			ObDereferenceObject(EProcess);   // 解引用
		}
	}

	*(PUINT32)OutputBuffer = ProcessCount;

	return STATUS_SUCCESS;
}


/************************************************************************
*  Name : APGetParentProcessId
*  Param: EProcess			   进程体结构
*  Ret  : UINT_PTR
*  获得父进程Id
************************************************************************/
UINT_PTR
APGetParentProcessId(IN PEPROCESS EProcess)
{
	if (MmIsAddressValid &&
		EProcess &&
		MmIsAddressValid(EProcess) &&
		MmIsAddressValid((PVOID)((PUINT8)EProcess + g_DynamicData.ObjectTable)))
	{
		UINT_PTR  ParentProcessId = 0;

		ParentProcessId = *(PUINT_PTR)((PUINT8)EProcess + g_DynamicData.InheritedFromUniqueProcessId);

		return ParentProcessId;
	}

	return 0;
}


/************************************************************************
*  Name : GetProcessFullPathByProcessId
*  Param: ProcessId					进程Id				（IN）
*  Param: ProcessFullPath			进程完整路径		（OUT）
*  Ret  : NTSTATUS
*  通过FileObject获得进程完整路径
************************************************************************/
NTSTATUS
APGetProcessFullPath(IN PEPROCESS EProcess, OUT PWCHAR ProcessFullPath)
{
	NTSTATUS	Status = STATUS_UNSUCCESSFUL;

	if (APIsValidProcess(EProcess))
	{
		/*
		3: kd> dt _EPROCESS fffffa801ac21060
		nt!_EPROCESS
		+0x000 Pcb              : _KPROCESS
		......
		+0x268 SectionObject    : 0xfffff8a0`01bf2a50 Void
		*/
		PSECTION_OBJECT SectionObject = (PSECTION_OBJECT)(*(PUINT_PTR)((PUINT8)EProcess + g_DynamicData.SectionObject));

		if (SectionObject && MmIsAddressValid(SectionObject))
		{
			/*
			3: kd> dt _SECTION_OBJECT 0xfffff8a0`01bf2a50
			nt!_SECTION_OBJECT
			+0x000 StartingVa       : (null)
			......
			+0x028 Segment          : 0xfffff8a0`01deb000 _SEGMENT_OBJECT
			*/
			PSEGMENT Segment = SectionObject->Segment;

			if (Segment && MmIsAddressValid(Segment))
			{
				/*
				3: kd> dt _SEGMENT 0xfffff8a0`01deb000
				nt!_SEGMENT
				+0x000 ControlArea      : 0xfffffa80`1ac18800 _CONTROL_AREA
				......
				*/
				PCONTROL_AREA ControlArea = Segment->ControlArea;

				if (ControlArea && MmIsAddressValid(ControlArea))
				{
					/*
					3: kd> dt _CONTROL_AREA 0xfffffa80`1ac18800
					nt!_CONTROL_AREA
					+0x000 Segment          : 0xfffff8a0`01deb000 _SEGMENT
					......
					+0x040 FilePointer      : _EX_FAST_REF

					3: kd> dq 0xfffffa80`1ac18800+40
					fffffa80`1ac18840  fffffa80`1ac18d44 00000000`00000000
					*/
					//PFILE_OBJECT FileObject = (PFILE_OBJECT)((UINT_PTR)ControlArea->FilePointer & 0xFFFFFFFFFFFFFFF8);
					
#ifdef _WIN64
					PFILE_OBJECT FileObject = (PFILE_OBJECT)((UINT_PTR)ControlArea->FilePointer & ~0xf);
#else
					PFILE_OBJECT FileObject = (PFILE_OBJECT)((UINT_PTR)ControlArea->FilePointer & ~7);
#endif // _WIN64

					

					if (FileObject && MmIsAddressValid(FileObject))
					{
						POBJECT_NAME_INFORMATION    oni = NULL;
						/*
						3: kd> dt _FILE_OBJECT fffffa80`1ac18d40
						nt!_FILE_OBJECT
						+0x000 Type             : 0n5
						......
						+0x058 FileName         : _UNICODE_STRING "\Windows\explorer.exe"
						*/
						Status = IoQueryFileDosDeviceName(FileObject, &oni);
						if (NT_SUCCESS(Status))
						{
							UINT32 ProcessFullPathLength = 0;

							if (oni->Name.Length >= MAX_PATH)
							{
								ProcessFullPathLength = MAX_PATH - 1;
							}
							else
							{
								ProcessFullPathLength = oni->Name.Length;
							}

							//RtlCopyMemory(ProcessFullPath, oni->Name.Buffer, ProcessFullPathLength);

							RtlStringCchCopyW(ProcessFullPath, ProcessFullPathLength + 1, oni->Name.Buffer);

							Status = STATUS_SUCCESS;

							DbgPrint("%S\r\n", ProcessFullPath);
						}
					}
				}
			}
		}
	}

	return Status;
}


/************************************************************************
*  Name : APGetProcessInfo
*  Param: EThread			线程体对象
*  Param: EProcess			进程体对象
*  Param: pti
*  Param: ThreadCount
*  Ret  : NTSTATUS
*  通过FileObject获得进程完整路径
************************************************************************/
VOID
APGetProcessInfo(IN PEPROCESS EProcess, OUT PPROCESS_INFORMATION pi, IN UINT32 ProcessCount)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	if (EProcess && MmIsAddressValid((PVOID)EProcess) && APIsValidProcess(EProcess))
	{
		Status = ObReferenceObjectByPointer(EProcess, 0, NULL, KernelMode);    // 因为要操作该对象所以增加引用计数
		if (NT_SUCCESS(Status))
		{
			pi->ProcessEntry[pi->NumberOfProcesses].ProcessId = (UINT32)(UINT_PTR)PsGetProcessId(EProcess);
			pi->ProcessEntry[pi->NumberOfProcesses].EProcess = (UINT_PTR)EProcess;
			pi->ProcessEntry[pi->NumberOfProcesses].ParentProcessId = (UINT32)APGetParentProcessId(EProcess);
			APGetProcessFullPath(EProcess, pi->ProcessEntry[pi->NumberOfProcesses].wzFilePath);

			//DbgPrint("Process Id:%d\r\n", pi->ProcessEntry[pi->NumberOfProcesses].ProcessId);
			//DbgPrint("EProcess:%p\r\n", pi->ProcessEntry[pi->NumberOfProcesses].EProcess);
			//DbgPrint("EProcess:%d\r\n", pi->ProcessEntry[pi->NumberOfProcesses].ParentProcessId);
			//DbgPrint("EProcess:%s\r\n", pi->ProcessEntry[pi->NumberOfProcesses].wzFilePath);

			pi->NumberOfProcesses++;
			ObDereferenceObject(EProcess);
		}
	}
}


/************************************************************************
*  Name : APEnumProcessInfoByIterateFirstLevelHandleTable
*  Param: TableCode
*  Param: EProcess
*  Param: pti
*  Param: ThreadCount
*  Ret  : VOID
*  遍历一级表
************************************************************************/
VOID
APEnumProcessInfoByIterateFirstLevelHandleTable(IN UINT_PTR TableCode,
	OUT PPROCESS_INFORMATION pi, IN UINT32 ProcessCount)
{
	/*
	Win7 x64 过16字节
	1: kd> dq fffff8a0`00fc2000
	fffff8a0`00fc2000  00000000`00000000 00000000`fffffffe
	fffff8a0`00fc2010  fffffa80`1acb3041 fffff780`00000000
	fffff8a0`00fc2020  fffffa80`1a989b61 00000000`00000000
	fffff8a0`00fc2030  fffffa80`1a98a301 00000000`00000000
	fffff8a0`00fc2040  fffffa80`1a98d061 fffff880`00000000
	fffff8a0`00fc2050  fffffa80`1ab8a061 fffffa80`00000000
	fffff8a0`00fc2060  fffffa80`1a99a061 fffff8a0`00000000
	fffff8a0`00fc2070  fffffa80`1a99bb61 00000000`00000000

	Win7 x86 过8字节
	0: kd> dd 8b404000
	8b404000  00000000 fffffffe 863d08a9 00000000		// 过前8个字节
	8b404010  863d05d1 00000000 863efd49 00000000
	8b404020  863f3bb9 00000000 863eb8d9 00000000
	8b404030  863f7021 00000000 863f74a9 00000000
	8b404040  863f3021 00000000 863f34d1 00000000
	8b404050  863fb021 00000000 863fb919 00000000
	8b404060  863fb641 00000000 863fb369 00000000
	8b404070  863f5021 00000000 863f5d49 00000000
	*/
	UINT32 i = 0;
	PHANDLE_TABLE_ENTRY	HandleTableEntry = (PHANDLE_TABLE_ENTRY)(*(PUINT_PTR)TableCode + g_DynamicData.HandleTableEntryOffset);

	for (  i = 0; i < 0x200; i++)		// 512个表项
	{
		if (MmIsAddressValid((PVOID)&(HandleTableEntry->NextFreeTableEntry)))
		{
			if (HandleTableEntry->NextFreeTableEntry == 0 &&
				HandleTableEntry->Object != NULL &&
				MmIsAddressValid(HandleTableEntry->Object))
			{
				PVOID Object = (PVOID)(((UINT_PTR)HandleTableEntry->Object) & 0xFFFFFFFFFFFFFFF8);
				APGetProcessInfo((PEPROCESS)Object, pi, ProcessCount);
			}
		}
		HandleTableEntry++;
	}
}


/************************************************************************
*  Name : APEnumProcessInfoByIterateSecondLevelHandleTable
*  Param: TableCode
*  Param: EProcess
*  Param: pti
*  Param: ThreadCount
*  Ret  : VOID
*  遍历二级表
************************************************************************/
VOID
APEnumProcessInfoByIterateSecondLevelHandleTable(IN UINT_PTR TableCode,
	OUT PPROCESS_INFORMATION pi, IN UINT32 ProcessCount)
{
	/*
	Win7 x64
	2: kd> dq 0xfffff8a0`00fc5000
	fffff8a0`00fc5000  fffff8a0`00005000 fffff8a0`00fc6000
	fffff8a0`00fc5010  fffff8a0`0180b000 fffff8a0`02792000
	fffff8a0`00fc5020  00000000`00000000 00000000`00000000

	Win7 x86
	0: kd> dd 0xa4aaf000
	a4aaf000  8b404000 a4a56000 00000000 00000000
	*/

	do
	{
		APEnumProcessInfoByIterateFirstLevelHandleTable(TableCode, pi, ProcessCount);		// fffff8a0`00fc5000..../ fffff8a0`00fc5008....
		TableCode += sizeof(UINT_PTR);

	} while (*(PUINT_PTR)TableCode != 0 && MmIsAddressValid((PVOID)*(PUINT_PTR)TableCode));

}

/************************************************************************
*  Name : APEnumProcessInfoByIterateThirdLevelHandleTable
*  Param: TableCode
*  Param: EProcess
*  Param: pti
*  Param: ThreadCount
*  Ret  : VOID
*  遍历三级表
************************************************************************/
VOID
APEnumProcessInfoByIterateThirdLevelHandleTable(IN UINT_PTR TableCode,
	OUT PPROCESS_INFORMATION pi, IN UINT32 ProcessCount)
{
	do
	{
		APEnumProcessInfoByIterateSecondLevelHandleTable(TableCode, pi, ProcessCount);
		TableCode += sizeof(UINT_PTR);

	} while (*(PUINT_PTR)TableCode != 0 && MmIsAddressValid((PVOID)*(PUINT_PTR)TableCode));

}


NTSTATUS
APEnumProcessInfoByIteratePspCidTable(OUT PPROCESS_INFORMATION pi, IN UINT32 ProcessCount)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	// 保存之前的模式，转成KernelMode
	PETHREAD EThread = PsGetCurrentThread();
	UINT8    PreviousMode = APChangeThreadMode(EThread, KernelMode);

	UINT_PTR PspCidTable = APGetPspCidTableAddress();

	APChangeThreadMode(EThread, PreviousMode);

	pi->NumberOfProcesses = 1;    // 先把0号成员预留出来，存放Idle

	// EnumHandleTable
	if (PspCidTable)
	{
		PHANDLE_TABLE	HandleTable = NULL;

		HandleTable = (PHANDLE_TABLE)(*(PUINT_PTR)PspCidTable);  	// HandleTable = fffff8a0`00004910
		if (HandleTable && MmIsAddressValid((PVOID)HandleTable))
		{
			UINT8			TableLevel = 0;		// 指示句柄表层数
			UINT_PTR		TableCode = 0;			// 地址存放句柄表首地址

			TableCode = HandleTable->TableCode & 0xFFFFFFFFFFFFFFFC;	// TableCode = 0xfffff8a0`00fc5000
			TableLevel = HandleTable->TableCode & 0x03;	                // TableLevel = 0x01

			if (TableCode && MmIsAddressValid((PVOID)TableCode))
			{
				switch (TableLevel)
				{
				case 0:
				{
					// 一层表
					APEnumProcessInfoByIterateFirstLevelHandleTable(TableCode, pi, ProcessCount);
					break;
				}
				case 1:
				{
					// 二层表
					APEnumProcessInfoByIterateSecondLevelHandleTable(TableCode, pi, ProcessCount);
					break;
				}
				case 2:
				{
					// 三层表
					APEnumProcessInfoByIterateThirdLevelHandleTable(TableCode, pi, ProcessCount);
					break;
				}
				default:
					break;
				}
			}
		}
	}

	if (pi->NumberOfProcesses > 1)
	{
		// 填充Idle的信息

		pi->ProcessEntry[0].ProcessId = 0;
		pi->ProcessEntry[0].EProcess = (UINT_PTR)APGetPsIdleProcess();   // 全局导出
		pi->ProcessEntry[0].ParentProcessId = 0;

		Status = STATUS_SUCCESS;
	}

	DbgPrint("EnumProcessInfo by iterate PspCidTable\r\n");

	return Status;
}


/************************************************************************
*  Name : APEnumProcessInfo
*  Param: OutputBuffer			Ring3Buffer      （OUT）
*  Param: OutputLength			Ring3BufferLength（IN）
*  Ret  : NTSTATUS
*  通过FileObject获得进程完整路径
************************************************************************/
NTSTATUS
APEnumProcessInfo(OUT PVOID OutputBuffer, IN UINT32 OutputLength)
{
	NTSTATUS  Status = STATUS_UNSUCCESSFUL;

	PEPROCESS EProcess = NULL;
	PPROCESS_INFORMATION pi = (PPROCESS_INFORMATION)OutputBuffer;
	UINT32 ProcessCount = (OutputLength - sizeof(PROCESS_INFORMATION)) / sizeof(PROCESS_ENTRY_INFORMATION);

	// 遍历句柄表，枚举进程

	Status = APEnumProcessInfoByIteratePspCidTable(pi, ProcessCount);
	if (NT_SUCCESS(Status))
	{
		if (ProcessCount >= pi->NumberOfProcesses)
		{
			Status = STATUS_SUCCESS;
		}
		else
		{
			Status = STATUS_BUFFER_TOO_SMALL;	// 给ring3返回内存不够的信息
		}
	}

/*
	// 通过暴力id来枚举进程
	for (UINT32 ProcessId = 0; ProcessId < MAX_PROCESS_COUNT; ProcessId += 4)
	{
		if (ProcessesCount > pi->NumberOfProcesses)
		{
			if (ProcessId == 0)
			{
				// Idle
				pi->ProcessEntry[pi->NumberOfProcesses].ProcessId = 0;
				pi->ProcessEntry[pi->NumberOfProcesses].EProcess = (UINT_PTR)APGetPsIdleProcess();   // 全局导出
				pi->ProcessEntry[pi->NumberOfProcesses].ParentProcessId = 0;

				DbgPrint("Process Id:%d\r\n", pi->ProcessEntry[pi->NumberOfProcesses].ProcessId);
				DbgPrint("EProcess:%p\r\n", pi->ProcessEntry[pi->NumberOfProcesses].EProcess);
				DbgPrint("EProcess:%d\r\n", pi->ProcessEntry[pi->NumberOfProcesses].ParentProcessId);

				pi->NumberOfProcesses++;
			}
			else
			{
				// 其他进程
				Status = PsLookupProcessByProcessId((HANDLE)ProcessId, &EProcess);
				if (NT_SUCCESS(Status) && APIsValidProcess(EProcess))
				{
					pi->ProcessEntry[pi->NumberOfProcesses].ProcessId = ProcessId;
					pi->ProcessEntry[pi->NumberOfProcesses].EProcess = (UINT_PTR)EProcess;
					pi->ProcessEntry[pi->NumberOfProcesses].ParentProcessId = (UINT32)APGetParentProcessId(EProcess);
					APGetProcessFullPath(EProcess, pi->ProcessEntry[pi->NumberOfProcesses].wzFilePath);

					DbgPrint("Process Id:%d\r\n", pi->ProcessEntry[pi->NumberOfProcesses].ProcessId);
					DbgPrint("EProcess:%p\r\n", pi->ProcessEntry[pi->NumberOfProcesses].EProcess);
					DbgPrint("EProcess:%d\r\n", pi->ProcessEntry[pi->NumberOfProcesses].ParentProcessId);
					DbgPrint("EProcess:%s\r\n", pi->ProcessEntry[pi->NumberOfProcesses].wzFilePath);

					pi->NumberOfProcesses++;

					ObDereferenceObject(EProcess);
				}
			}
			Status = STATUS_SUCCESS;
		}
		else
		{
			DbgPrint("Not Enough Ring3 Memory\r\n");
			Status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
	}

	*/
	return Status;
}


/************************************************************************
*  Name : APTerminateProcess
*  Param: ProcessId
*  Ret  : NTSTATUS
*  结束进程
************************************************************************/
NTSTATUS
APTerminateProcess(IN UINT32 ProcessId)
{
	NTSTATUS  Status = STATUS_UNSUCCESSFUL;
	
	if (ProcessId <= 4)
	{
		Status = STATUS_ACCESS_DENIED;
	}
	else
	{
		PEPROCESS EProcess = NULL;

		Status = PsLookupProcessByProcessId((HANDLE)ProcessId, &EProcess);
		if (NT_SUCCESS(Status) && APIsValidProcess(EProcess))
		{
			Status = APTerminateProcessByIterateThreadListHead(EProcess);

			ObDereferenceObject(EProcess);
		}
	}

	return Status;
}


/************************************************************************
*  Name : APGetGuiProcess
*  Param: void
*  Ret  : PEPROCESS
*  遍历进程，获得csrss Eprocess
************************************************************************/
PEPROCESS
APGetGuiProcess()
{
	NTSTATUS  Status = STATUS_UNSUCCESSFUL;
	PEPROCESS EProcess = NULL;
	UINT32 i = 4;
	for (  i = 4; i < MAX_PROCESS_COUNT; i += 4)
	{
		Status = PsLookupProcessByProcessId((HANDLE)i, &EProcess);
		if (NT_SUCCESS(Status) && EProcess && MmIsAddressValid((PVOID)EProcess))
		{
			CHAR *ProcessImageName = (CHAR*)PsGetProcessImageFileName(EProcess);
			
			ObDereferenceObject(EProcess);

			if (!_strnicmp("csrss.exe", ProcessImageName, strlen("csrss.exe")))
			{
				DbgPrint("EProcess = %p ProcessId = %ld ImageName = %s\r\n", EProcess, PsGetProcessId(EProcess), PsGetProcessImageFileName(EProcess));
				return EProcess;
			}
		}
	}

	return NULL;
}