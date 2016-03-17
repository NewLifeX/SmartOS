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
			build.Cortex = 4;
			build.Output = "F4";
			build.AddIncludes("..\\..\\Lib\\CMSIS");
			build.AddIncludes("..\\..\\Lib\\Inc");
            build.AddFiles("..\\", "*.c;*.cpp", false, "CAN;DMA;Memory");
            build.AddFiles("..\\Platform", "Boot_F4.cpp");
            build.AddFiles("..\\Platform", "startup_stm32f4xx.s");
            build.AddFiles("..\\Security", "*.cpp");
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
            build.BuildLib("..\\SmartOS_F4");

			build.Debug = false;
            build.CompileAll();
            build.BuildLib("..\\SmartOS_F4");
        }
    }

}
	//include=MDK.cs
