var build = Builder.Create("MDK");
build.Init();
build.CPU = "Cortex-M3";
build.Defines.Add("STM32F1");
build.AddIncludes("..\\", false);
build.AddFiles("..\\Core");
build.AddFiles("..\\Kernel");
build.AddFiles("..\\Device");
build.AddFiles("..\\", "*.c;*.cpp", false);
build.AddFiles("..\\Net");
build.AddFiles("..\\Message");
build.AddFiles("..\\Security", "*.cpp");
build.AddFiles("..\\Board");
build.AddFiles("..\\Storage");
build.AddFiles("..\\App");
build.AddFiles("..\\Drivers");
build.AddFiles("..\\Test");
build.AddFiles("..\\TinyIP", "*.c;*.cpp", false, "HttpClient");
build.AddFiles("..\\TinyNet");
build.AddFiles("..\\TokenNet");
build.Libs.Clear();
//build.Preprocess = true;
build.CompileAll();
build.BuildLib("..\\SmartOS_M3");

build.Debug = true;
build.CompileAll();
build.BuildLib("..\\SmartOS_M3");

/*build.Debug = false;
build.Tiny = true;
build.CompileAll();
build.BuildLib("..\\SmartOS_M3");*/
