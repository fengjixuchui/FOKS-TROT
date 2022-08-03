#include <fltKernel.h>

#define POC_MAX_NAME_LENGTH				320
#define MAX_SECURE_EXTENSION_COUNT		256
#define POC_EXTENSION_SIZE			    32

WCHAR secure_extension[MAX_SECURE_EXTENSION_COUNT][POC_EXTENSION_SIZE];
size_t secure_extension_count = 0;

WCHAR RelevantPath[256][POC_MAX_NAME_LENGTH] = { 0 };
ULONG current_relevant_path_inx = 0;

/*
* �ڱȽ�ʱ�����õ��Ǵ�Сд�޹صıȽϷ�ʽ
* �����������Ҫ����͸�����ܵ��ļ�����չ��
*/
PWCHAR allowed_extension[MAX_SECURE_EXTENSION_COUNT] = {
		L"docx",
		L"doc",
		L"xlsx",
		L"xls",
		L"pptx",
		L"ppt",
		L"txt",
		L"png",
		L"jpg",
		L"mp4",
		L"dwg",
		L"iso",
		NULL };

/*
* ���ҽ����ļ�λ�������ļ�����ʱ�Ż����͸������
*/
const PWCHAR allowed_path[] = {
	L"C:\\Users\\wangzhankun\\Desktop\\testdata",
	L"C:\\Users\\hkx3upper\\Desktop",
	L"C:\\Desktop",
	NULL };


/*
* ���̷�Ϊ���࣬��Ȩ���̣�����Ȩ���̺ͱ��ݽ��̣�
* ���н���Ĭ��Ϊ����Ȩ���̣�����Ȩ���̶����ģ������ļ����ȣ����������޸����ģ�
* ��Ȩ���̾��Ǹ��������İ칫�������д���ģ������ļ����ȣ���
* ���ݽ�����Ҫ�Ǳ�����Դ���������ļ��ӻ����ļ������Ƴ��������ܽ����ļ���
* ������Ҫ�����������ļ���ʶβ�������Ƴ����������ģ�ȫ���ļ����ȣ��������޸����ġ�
* 
* ���ջ�����࣬��Ȩ���������Ļ��壬����Ȩ���̺ͱ��ݽ��������Ļ��塣
*/

/*
* ֻ����Ȩ���̲����������ܽ����ļ�
*/
const PWCHAR secure_process[] = {
	/*
	* PocUserPanel��������Ȩ���̣�Ĭ�ϰ�װ·��
	*/
	L"C:\\Program Files\\hkx3upper\\PocUserPanel.exe",
	
	/*
	* �û�����
	*/
	L"C:\\Windows\\System32\\notepad.exe",
	L"C:\\Desktop\\npp.7.8.1.bin\\notepad++.exe",
	L"C:\\Desktop\\Test.exe",

	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11365\\office6\\wps.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11365\\office6\\wpp.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11365\\office6\\et.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11744\\office6\\wps.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11744\\office6\\wpp.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11744\\office6\\et.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11805\\office6\\wps.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11805\\office6\\wpp.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11805\\office6\\et.exe",

	L"C:\\Program Files\\Microsoft Office\\root\\Office16\\WINWORD.exe",
	L"C:\\Program Files\\Microsoft Office\\root\\Office16\\EXCEL.exe",
	L"C:\\Program Files\\Microsoft Office\\root\\Office16\\POWERPNT.exe",

	L"C:\\Program Files\\Microsoft VS Code\\Code.exe",
	L"C:\\Program Files\\Autodesk\\AutoCAD 2020\\acad.exe",
	L"C:\\WINDOWS\\system32\\certutil.exe",
	L"C:\\Program Files\\VideoLAN\\VLC\\vlc.exe",
	NULL };


/*
* ���ݽ��̿��Զ��������������ļ�
*/
const PWCHAR backup_process[] = {

	L"C:\\Program Files\\VMware\\VMware Tools\\vmtoolsd.exe",
	L"C:\\Windows\\explorer.exe",
	L"C:\\Windows\\System32\\dllhost.exe",

	NULL };
