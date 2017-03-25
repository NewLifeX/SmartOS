var dst = "..\\..\\SmartSDK\\";
"..\\".AsDirectory().CopyTo(dst, "*.h;*.exe;*.dll", true);
Directory.Delete(dst + "Platform", true);
Directory.Delete(dst + "XX", true);
