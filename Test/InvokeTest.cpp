#include "Sys.h"
#include "TokenNet/TokenClient.h"

bool InvokeFun(const BinaryPair& args, BinaryPair& result)
{
	byte rt;
	bool rs;

	rs = args.Get("Hello", rt);
	if (rt == 1 && rs)
		result.Set("Hello", (byte)0);
	else
		return false;

	rs = args.Get("Rehello", rt);
	if (rt == 0 && rs)
		result.Set("Rehello", (byte)1);
	else
		return false;

	return true;
}


void InvokeTest(TokenClient * client)
{
	debug_printf("Reg Invoke \r\n");
	client->Register("Test", InvokeFun);

	// 写入信息
	MemoryStream ms1;
	BinaryPair bp(ms1);
	bp.Set("Hello", (byte)1);
	bp.Set("Rehello", (byte)0);
	ms1.SetPosition(0);
	// 封装成所需数据格式
	BinaryPair args(ms1);
	// 准备返回数据的容器
	MemoryStream ms2;
	BinaryPair result(ms2);
	// 调用
	client->OnInvoke("Test", args, result);

	bool isOk = true;

	byte rt = 0;
	bool rs = result.Get("Hello", rt);
	if (!rs)
	{
		debug_printf("not got Hello");
		isOk = false;
	}
	if (rt != 0)
	{
		debug_printf("test error");
		isOk = false;
	}

	rs = result.Get("Rehello", rt);
	if (!rs)
	{
		debug_printf("not got Rehello");
		isOk = false;
	}
	if (rt != 1)
	{
		debug_printf("test error");
		isOk = false;
	}

	if(isOk)
		debug_printf("Invoke测试通过\r\n");
	else
		debug_printf("Invoke测试未通过\r\n");

}
