var dst = "..\\..\\SmartSDK\\";
var dsos = dst + "SmartOS\\";
var dlib = dst + "Lib\\";

"..\\".AsDirectory().CopyTo(dsos, "*.h;*.exe;*.dll", true);
"..\\Test".AsDirectory().CopyTo(dsos + "Test", "*.cpp");
Directory.Delete(dsos + "Platform", true);
Directory.Delete(dsos + "XX", true);

// 压缩库文件
foreach(var item in "..\\".AsDirectory().GetAllFiles("*.lib;*.a"))
{
	var dz = (dsos + item.Name + ".7z").AsFile();
	if(!dz.Exists || dz.LastWriteTime < item.LastWriteTime)
	{
		Console.WriteLine("压缩 {0}", item.Name);
		item.Compress(dz.FullName);
	}
}
foreach(var item in "..\\..\\Lib".AsDirectory().GetAllFiles("*.lib;*.a"))
{
	var dz = (dlib + item.Name + ".7z").AsFile();
	if(!dz.Exists || dz.LastWriteTime < item.LastWriteTime)
	{
		Console.WriteLine("压缩 {0}", item.Name);
		item.Compress(dz.FullName);
	}
}
