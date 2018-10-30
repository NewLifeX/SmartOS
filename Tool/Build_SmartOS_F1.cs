using NewLife.Build;

var build = Builder.Create("MDK");
build.Init(false);
build.CPU = "Cortex-M3";
build.Output = "F1";

build.Defines.Add("STM32F1");
//include=_Files.cs

build.AddIncludes("..\\..\\LibF1\\CMSIS");
build.AddIncludes("..\\..\\LibF1\\Inc");
build.AddIncludes("..\\Platform", false);
build.AddFiles("..\\Platform\\STM32F1", "*.c;*.cpp;startup_stm32f10x.s");
build.AddFiles("..\\Platform\\CortexM", "*.c;*.cpp");

build.AddFiles("..\\..\\LibF1", "*.c;*.cpp", true, "system_stm32f10x");
build.AddIncludes("..\\..\\LibF1\\cmsis", false);
build.AddIncludes("..\\..\\LibF1\\inc", false);
build.Defines.Add("USE_STDPERIPH_DRIVER");

build.Libs.Clear();
build.CompileAll();
build.BuildLib("..\\SmartOS_F1");

build.Debug = true;
build.CompileAll();
build.BuildLib("..\\SmartOS_F1");