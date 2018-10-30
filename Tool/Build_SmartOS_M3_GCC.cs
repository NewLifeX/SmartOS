using NewLife.Build;

var build = Builder.Create("GCCArm");
build.Init(false);
build.CPU = "Cortex-M3";
build.Linux = true;
build.Output = "GCC";
//include=_Files.cs
build.Libs.Clear();
build.CompileAll();
build.BuildLib("..\\libSmartOS_M3.a");

build.Debug = true;
// 未使用参数和无符号整数比较太多了，过滤掉
//build.ExtCompiles = "-Wno-unused-parameter -Wno-sign-compare";
build.ExtCompiles = "-Wno-unused-parameter";
build.CompileAll();
build.BuildLib("..\\libSmartOS_M3.a");
