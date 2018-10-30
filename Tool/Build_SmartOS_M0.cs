using NewLife.Build;

var build = Builder.Create("MDK");
build.Init(false);
build.CPU = "Cortex-M0+";

build.Defines.Add("STM32F0");
//include=_Files.cs
build.Libs.Clear();
build.CompileAll();
build.BuildLib("..\\SmartOS_M0");

build.Debug = true;
build.CompileAll();
build.BuildLib("..\\SmartOS_M0");