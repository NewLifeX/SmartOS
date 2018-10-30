using NewLife.Build;

var build = Builder.Create("MDK");
build.Init(false);
build.CPU = "Cortex-M0+";
build.Defines.Add("STM32F0");
build.AddIncludes("..\\..\\..\\LibF0\\CMSIS");
build.AddIncludes("..\\..\\..\\LibF0\\Inc");
build.AddIncludes("..\\..\\", false);
build.AddFiles(".", "*.c;*.cpp;startup_stm32f0xx.s");
build.AddFiles("..\\CortexM", "*.c;*.cpp;*.s");
build.Libs.Clear();
build.CompileAll();
build.BuildLib("..\\..\\SmartOS_F0");

build.Debug = true;
build.CompileAll();
build.BuildLib("..\\..\\SmartOS_F0");