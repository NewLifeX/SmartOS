using NewLife.Build;

var build = Builder.Create("MDK");
build.Init(false);
build.CPU = "Cortex-M3";
build.Defines.Add("STM32F1");
build.AddIncludes("..\\..\\..\\LibF1\\CMSIS");
build.AddIncludes("..\\..\\..\\LibF1\\Inc");
build.AddIncludes("..\\", false);
build.AddIncludes("..\\..\\", false);
build.AddFiles(".", "*.c;*.cpp;startup_stm32f10x.s");
build.AddFiles("..\\CortexM", "*.c;*.cpp");
build.Libs.Clear();
build.CompileAll();
build.BuildLib("..\\..\\SmartOS_F1");

build.Debug = true;
build.CompileAll();
build.BuildLib("..\\..\\SmartOS_F1");