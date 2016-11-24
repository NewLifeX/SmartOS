var build = Builder.Create("MDK");
build.Init();
build.CPU = "Cortex-M0+";
build.Defines.Add("STM32F0");
build.AddIncludes("..\\..\\..\\Lib\\CMSIS");
build.AddIncludes("..\\..\\..\\Lib\\Inc");
build.AddIncludes("..\\..\\", false);
build.AddFiles(".", "*.c;*.cpp;*.s");
build.AddFiles("..\\CortexM", "*.c;*.cpp;*.s");
build.Libs.Clear();
build.CompileAll();
build.BuildLib("..\\..\\SmartOS_F0");

build.Debug = true;
build.CompileAll();
build.BuildLib("..\\..\\SmartOS_F0");