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
            XTrace.UseConsole();

            var build = new Builder();
            build.Init();
            build.GD32 = true;
			build.AddIncludes("..\\..\\Lib");
            build.CompileAll("..\\", "*.c;*.cpp", false, "CAN;DMA;I2C;Memory;String");
            build.CompileAll("..\\Platform", "Boot_F0.cpp");
            build.CompileAll("..\\Platform", "startup_stm32f0xx.s");
            build.BuildLib("..\\SmartOS_M0");
            //build.Build("");
        }
    }

}
	//include=MDK.cs
