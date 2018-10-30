using NewLife.Build;

var build = Builder.Create("ICC");
build.Init(false);
build.CPU = "Cortex-M3";
build.Output = "ICC";
build.Defines.Add("STM32F1");
//include=_Files.cs
build.Libs.Clear();
build.CompileAll();
build.BuildLib("..\\SmartOS_M3");

build.Debug = true;
build.CompileAll();
build.BuildLib("..\\SmartOS_M3");