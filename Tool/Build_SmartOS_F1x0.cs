using System;
using System.Collections;
using System.Diagnostics;
using System.Reflection;
using System.Text;
using System.Linq;
using System.IO;
using System.Collections.Generic;
using Microsoft.Win32;
using NewLife.Log;

namespace NewLife.Reflection
{
    public class ScriptEngine
    {
        static void Main()
        {
            var build = new Builder();
            build.Init();
            build.GD32 = true;	// 先设置GD32才设置Cortex，使用STM32F0+M3编译
			build.Cortex = 0;
			build.Output = "F1x0";
			build.AddIncludes("..\\..\\Lib\\CMSIS");
			build.AddIncludes("..\\..\\Lib\\Inc");
            build.AddFiles("..\\", "*.c;*.cpp", false, "CAN;DMA;Memory");
            build.AddFiles("..\\Platform", "Boot_F0.cpp");
            build.AddFiles("..\\Platform", "startup_stm32f0xx.s");
            build.AddFiles("..\\Security");
            build.AddFiles("..\\Storage");
            build.AddFiles("..\\App");
            build.AddFiles("..\\Drivers");
            build.AddFiles("..\\Net");
            build.AddFiles("..\\TinyIP", "*.c;*.cpp", false, "HttpClient");
            build.AddFiles("..\\Message");
            build.AddFiles("..\\TinyNet");
            build.AddFiles("..\\TokenNet");
			build.Libs.Clear();
            build.CompileAll();
            build.BuildLib("..\\SmartOS_F1x0");

			build.Debug = false;
            build.CompileAll();
            build.BuildLib("..\\SmartOS_F1x0");

			/*build.Tiny = true;
            build.CompileAll();
            build.BuildLib("..\\SmartOS_F1x0");*/
        }
    }
}
	//include=MDK.cs
