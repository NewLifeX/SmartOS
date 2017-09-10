var build = Builder.Create("MDK6");
build.Init(false);
build.CPU = "Cortex-M3";
build.Output = "CLang";
build.Defines.Add("STM32F1");
//include=_Files.cs
build.Libs.Clear();
build.CompileAll();
build.BuildLib("..\\SmartOS_M3");

build.Debug = true;
build.CompileAll();
build.BuildLib("..\\SmartOS_M3");