using NewLife.Build;

var build = Builder.Create("MDK");
build.Init(false);
build.CPU = "Cortex-M3";
build.Output = "Linux";
build.Linux = true;
build.Defines.Add("STM32F1");
//include=_Files.cs
build.Libs.Clear();
//build.ExtCompiles.Add("--enum_is_int");
//build.ExtCompiles.Add("--signed_chars");
//build.ExtCompiles.Add("--wchar32");
build.CompileAll();
build.BuildLib("..\\SmartOS_M3");

build.Debug = true;
build.CompileAll();
build.BuildLib("..\\SmartOS_M3");