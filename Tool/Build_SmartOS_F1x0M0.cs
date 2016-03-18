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
			build.Cortex = 0;
            build.GD32 = true;	// 先设置Cortex才设置GD32，使用STM32F0+M0编译
			build.Output = "F1x0M0";
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
            build.BuildLib("..\\SmartOS_F1x0M0");

			build.Debug = true;
            build.CompileAll();
            build.BuildLib("..\\SmartOS_F1x0M0");

			//build.Defines.Add("TINY");
            //build.CompileAll();
            //build.BuildLib("..\\SmartOS_F0T");
        }
    }

}
	//include=MDK.cs
