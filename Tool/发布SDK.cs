var dst = "..\\..\\SmartSDK\\";
"..\\".AsDirectory().CopyToIfNewer(dst, "*.h;*.exe;*.dll", true);
"..\\Test".AsDirectory().CopyToIfNewer(dst + "Test", "*.cpp");
//Directory.Delete(dst + "Platform", true);
//Directory.Delete(dst + "XX", true);

// 压缩库文件
foreach(var item in "..\\".AsDirectory().GetAllFiles("*.lib;*.a"))
{
	var dz = (dst + item.Name + ".7z").AsFile();
	if(!dz.Exists || dz.LastWriteTime < item.LastWriteTime)
	{
		Console.WriteLine("压缩 {0}", item.Name);
		item.Compress(dz.FullName);
	}
}
foreach(var item in "..\\..\\Lib".AsDirectory().GetAllFiles("*.lib;*.a"))
{
	var dz = (dst + item.Name + ".7z").AsFile();
	if(!dz.Exists || dz.LastWriteTime < item.LastWriteTime)
	{
		Console.WriteLine("压缩 {0}", item.Name);
		item.Compress(dz.FullName);
	}
}
