#include "Sys.h"
#include "Message/BinaryPair.h"
#include "TokenNet/TokenClient.h"

bool InvokeFun(void* param, const Pair& args, Stream& result)
{
	byte rt;
	bool rs;
	BinaryPair res(result);

	rs = args.Get("Hello", rt);
	if(!rs || rt != 1) return false;

	res.Set("Hello", (byte)0);

	rs = args.Get("Rehello", rt);
	if(!rs || rt != 0) return false;

	res.Set("Rehello", (byte)1);

	return true;
}

void InvokeTest(TokenClient * client)
{
	debug_printf("\r\nReg Invoke \r\n");
	client->Register("Test", InvokeFun, nullptr);

	// 写入信息
	MemoryStream ms1;
	BinaryPair bp(ms1);
	bp.Set("Hello", (byte)1);
	bp.Set("Rehello", (byte)0);
	ms1.SetPosition(0);
	// 封装成所需数据格式
	BinaryPair args(ms1);
	// 准备返回数据的容器
	MemoryStream result;

#if DEBUG
	// 调用
	//client->OnInvoke("Test", args, result);
#endif

	bool isOk = true;

	byte rt = 0;
	BinaryPair resxx(result);

	bool rs = resxx.Get("Hello", rt);
	if (!rs)
	{
		debug_printf("not got Hello\r\n");
		isOk = false;
	}
	if (rt != 0)
	{
		debug_printf("test error\r\n");
		isOk = false;
	}

	rs = resxx.Get("Rehello", rt);
	if (!rs)
	{
		debug_printf("not got Rehello\r\n");
		isOk = false;
	}
	if (rt != 1)
	{
		debug_printf("test error\r\n");
		isOk = false;
	}

	if(isOk)
		debug_printf("Invoke测试通过\r\n");
	else
		debug_printf("Invoke测试未通过\r\n");

}
