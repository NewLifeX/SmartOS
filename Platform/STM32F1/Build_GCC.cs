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
using System.Text.RegularExpressions;
using System.Threading;
using System.Xml;
using NewLife.Web;

namespace NewLife.Reflection
{
    public class ScriptEngine
    {
        static void Main()
        {
            var build = Builder.Create("GCC");
            build.Init();
			build.Cortex = 3;
			build.Defines.Add("STM32F1");
			build.AddIncludes("..\\..\\..\\Lib\\CMSIS");
			build.AddIncludes("..\\..\\..\\Lib\\Inc");
			build.AddIncludes("..\\", false);
			build.AddIncludes("..\\..\\", false);
            build.AddFiles(".", "*.c;*.cpp;startup_stm32f10x_gcc.s");
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
        }
    }
}
