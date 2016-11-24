var build = Builder.Create("MDK");
build.Init();
build.CPU = "Cortex-M3";
build.Defines.Add("STM32F1");
build.AddIncludes("..\\..\\..\\Lib\\CMSIS");
build.AddIncludes("..\\..\\..\\Lib\\Inc");
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

/*build.Tiny = true;
build.CompileAll();
build.BuildLib("..\\");*/
