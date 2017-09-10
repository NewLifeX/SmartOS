var build = Builder.Create("MDK");
build.Init(false);
build.CPU = "Cortex-M4F";
build.Defines.Add("STM32F4");
//include=_Files.cs
build.Libs.Clear();
build.CompileAll();
build.BuildLib("..\\SmartOS_M4");

build.Debug = true;
build.CompileAll();
build.BuildLib("..\\SmartOS_M4");