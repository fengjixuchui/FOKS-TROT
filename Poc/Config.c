#include <fltKernel.h>

#define MAX_SECURE_EXTENSION_COUNT  256

WCHAR secure_extension[MAX_SECURE_EXTENSION_COUNT][32];
size_t secure_extension_count = 0;

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
		/*L"png",
		L"jpg",*/
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
* ֻ����Ȩ���̲����������ܽ����ļ�
*/
const PWCHAR secure_process[] = {
	/*
	* ϵͳ����
	* PocUser.exe��������Ȩ���̣�explorer.exe����.doc�ļ��Ǳ����
	*/
	L"C:\\Windows\\explorer.exe",
	L"C:\\Desktop\\PocUser.exe",

	
	/*
	* �û�����
	*/
	L"C:\\Windows\\System32\\notepad.exe",
	L"C:\\Desktop\\npp.7.8.1.bin\\notepad++.exe",

	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11365\\office6\\wps.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11365\\office6\\wpp.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11365\\office6\\et.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11744\\office6\\wps.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11744\\office6\\wpp.exe",
	L"C:\\Users\\hkx3upper\\AppData\\Local\\Kingsoft\\WPS Office\\11.1.0.11744\\office6\\et.exe",

	L"C:\\Program Files\\Microsoft Office\\root\\Office16\\WINWORD.exe",
	L"C:\\Program Files\\Microsoft Office\\root\\Office16\\EXCEL.exe",
	L"C:\\Program Files\\Microsoft Office\\root\\Office16\\POWERPNT.exe",

	L"C:\\Program Files\\Microsoft VS Code\\Code.exe",
	L"C:\\Program Files\\Autodesk\\AutoCAD 2020\\acad.exe",
	L"C:\\WINDOWS\\system32\\certutil.exe",
	L"C:\\Program Files\\VideoLAN\\VLC\\vlc.exe",
	NULL };
